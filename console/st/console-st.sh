#!/usr/bin/env bash
#
# Copyright (c) 2020 Seagate Technology LLC and/or its Affiliates
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#    http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.
#
# For any questions about this software or licensing,
# please email opensource@seagate.com or cortx-questions@seagate.com.
#

set -eu

umask 0002

## CAUTION: This path will be removed by superuser.
SANDBOX_DIR=${SANDBOX_DIR:-/var/motr/sandbox.console-st}

M0_SRC_DIR=`readlink -f $0`
M0_SRC_DIR=${M0_SRC_DIR%/*/*/*}

. $M0_SRC_DIR/utils/functions # die, opcode, sandbox_init, report_and_exit

CLIENT=$M0_SRC_DIR/console/m0console
SERVER=$M0_SRC_DIR/console/st/server

OUTPUT_FILE=$SANDBOX_DIR/client.log
YAML_FILE9=$SANDBOX_DIR/req-9.yaml
YAML_FILE41=$SANDBOX_DIR/req-41.yaml
XPRT=$(m0_default_xprt)
NID=$(m0_loopback_nid_get)
if [ "$XPRT" = "lnet" ]; then
	SERVER_EP_ADDR=$NID:12345:34:1
	CLIENT_EP_ADDR=$NID:12345:34:*
else
	SERVER_EP_ADDR=$NID@3000
	CLIENT_EP_ADDR=$NID@3001
fi
CONF_FILE_PATH=$M0_SRC_DIR/ut/diter.xc
CONF_PROFILE='<0x7000000000000001:0>'

NODE_UUID=02e94b88-19ab-4166-b26b-91b51f22ad91  # required by `common.sh'
. $M0_SRC_DIR/m0t1fs/linux_kernel/st/common.sh  # modload


start_server()
{
	if [ "$XPRT" = "lnet" ]; then
		modprobe lnet
		echo 8 >/proc/sys/kernel/printk
		modload
	fi

	_use_systemctl=0

	echo -n 'Running m0mkfs... ' >&2
	##
	## NOTE: The list of options passed to m0mkfs command should
	## correspond to the content of `server_argv' array in
	## console/st/server.c.
	## m0mkfs does not call m0_cs_default_stypes_init(), which
	## registers "ds1" and "ds2" service types, so we do not pass
	## these services to m0mkfs.
	##
	$M0_SRC_DIR/utils/mkfs/m0mkfs -T AD -D console_st_srv.db \
	    -S console_st_srv.stob -A linuxstob:console_st_srv-addb.stob \
	    -w 10 -e "$XPRT:$SERVER_EP_ADDR" -H $SERVER_EP_ADDR \
	    -q 2 -m $((1 << 17)) \
	    -c $CONF_FILE_PATH  \
	    &>$SANDBOX_DIR/mkfs.log || die 'm0mkfs failed'
	echo 'OK' >&2

	$SERVER -v &>$SANDBOX_DIR/server.log &
	sleep 1
	pgrep $(basename "$SERVER") >/dev/null || die 'Service failed to start'
	echo 'Service started' >&2
}

create_yaml_files()
{
	cat <<EOF >$YAML_FILE9
server  : $SERVER_EP_ADDR
client  : $CLIENT_EP_ADDR

Test FOP:
     - cons_test_type : A
       cons_test_id : 495
       cons_seq : 144
       cons_oid : 233
       cons_size : 5
       cons_buf : abcde
EOF

	## The content of `Write FOP' below should correspond to
	## m0_fop_cob_writev definition (see ioservice/io_fops.h).
	cat <<EOF >$YAML_FILE41
server  : $SERVER_EP_ADDR
client  : $CLIENT_EP_ADDR

Write FOP:
  - # m0_fop_cob_writev.c_rwv :: m0_fop_cob_rw
    ## m0_fop_cob_rw.crw_version :: m0_fv_version
    fvv_read : 10
    fvv_write : 11
    ## m0_fop_cob_rw.crw_gfid :: m0_fid
    f_container : 12
    f_key : 13
    ## m0_fop_cob_rw.crw_fid :: m0_fid
    f_container : 14
    f_key : 15
    ## m0_fop_cob_rw.crw_desc :: m0_io_descs
    id_nr : 1
    ### m0_io_descs.id_descs :: m0_net_buf_desc_data
    #### m0_net_buf_desc_data.bdd_desc :: m0_net_buf_desc
    nbd_len : 5
    nbd_data : Hello
    ###
    bdd_used : 5
    ## m0_fop_cob_rw.crw_ivec :: m0_io_indexvec
    ci_nr : 1
    ### m0_io_indexvec.ci_iosegs :: m0_ioseg
    ci_index : 17
    ci_count : 100
    crw_flags : 21
    ## m0_fop_cob_rw.crw_di_data :: m0_buf
    b_nob : 0
    b_addr : 0
EOF
}

stop_server()
{
	{ pkill -KILL $(basename "$SERVER") && wait; } || true
	if [ "$XPRT" = "lnet" ]; then
		modunload
	fi
}

check_reply()
{
	expected="$1"
	actual=`awk '/replied/ {print $5}' $OUTPUT_FILE`
	[ -z "$actual" ] && die 'Reply not found'
	[ "$actual" -eq "$expected" ] || die 'Invalid reply'
}

test_fop()
{
	[ $# -gt 3 ] || die 'test_fop: Wrong number of arguments'
	local message="$1"; shift
	local request="$1"; shift
	local reply="$1"; shift

	echo -n "$message: " >&2
	$CLIENT -f $request -v "$@" | tee $OUTPUT_FILE
	check_reply $reply
	echo OK >&2
}

run_st()
{
	test_fop 'Console test fop, xcode input' \
		$(opcode M0_CONS_TEST) $(opcode M0_CONS_FOP_REPLY_OPCODE) \
		-d '(65, 22, (144, 233), "abcde")'

	create_yaml_files

	test_fop 'Console test fop, YAML input' \
		$(opcode M0_CONS_TEST) $(opcode M0_CONS_FOP_REPLY_OPCODE) \
		-i -y $YAML_FILE9

	## This test case does not work: $SERVER crashes while
	## processing the fop (m0_fop_cob_writev).
	## See https://jts.seagate.com/browse/MOTR-294 for details.
	if false; then
		test_fop 'Write request fop' \
			$(opcode M0_IOSERVICE_WRITEV_OPCODE) \
			$(opcode M0_IOSERVICE_WRITEV_REP_OPCODE) \
			-i -y $YAML_FILE41
	fi
}

## -------------------------------------------------------------------
## main
## -------------------------------------------------------------------

[ `id -u` -eq 0 ] || die 'Must be run by superuser'

sandbox_init
start_server
run_st
stop_server
sandbox_fini
report_and_exit console 0
