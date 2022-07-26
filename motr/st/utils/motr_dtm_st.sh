#!/bin/bash
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


# System test framework.

#set -x

SANDBOX_DIR=${SANDBOX_DIR:-/var/motr/sandbox.motr-dtm-st}

emsg="---"
M0_SRC_DIR=`readlink -f $0`
M0_SRC_DIR=${M0_SRC_DIR%/*/*/*/*}
num=10
large_size=30
small_size=20

. $M0_SRC_DIR/utils/functions # sandbox_init, report_and_exit
# following scripts are mandatory for start the motr service (motr_start/stop)
. $M0_SRC_DIR/m0t1fs/linux_kernel/st/common.sh
. $M0_SRC_DIR/m0t1fs/linux_kernel/st/m0t1fs_common_inc.sh
. $M0_SRC_DIR/m0t1fs/linux_kernel/st/m0t1fs_server_inc.sh
. $M0_SRC_DIR/motr/st/utils/motr_local_conf.sh

out_file="${SANDBOX_DIR}/dtm_st.log"
rc_file="${SANDBOX_DIR}/dtm_st.cod.log"
erc_file="${SANDBOX_DIR}/dtm_st.ecod.log"
res_out_file="${SANDBOX_DIR}/dtm_st.results.log"

keys_file="${SANDBOX_DIR}/keys.txt"
keys_bulk_file="${SANDBOX_DIR}/keys_bulk.txt"
vals_file="${SANDBOX_DIR}/vals.txt"
vals_bulk_file="${SANDBOX_DIR}/vals_bulk.txt"
fids_file="${SANDBOX_DIR}/fids.txt"

genf="genf ${num} ${fids_file}"
genv_bulk="genv ${num} ${large_size} ${vals_bulk_file}"
genv="genv ${num} ${small_size} ${vals_file}"
verbose=0

#NODE_UUID=`uuidgen`

interrupt() { echo "Interrupted by user" >&2; stop 2; }
error() { echo "$@" >&2; stop 1; }


function usage()
{
	echo "$0"
	echo "Options:"
	echo "    '-v|--verbose'         output additional info into console"
	echo "    '-h|--help'            show this help"
}

start_motr()
{
	local rc=0
	local multiple_pools=0
	local stride=1024
	local N=$NR_DATA
	local K=$NR_PARITY
	local S=$NR_SPARE
	local P=$POOL_WIDTH
	local FI_OPTS="m0_ha_msg_accept:in-dtm-st:always"

	echo "Motr service starting..."
	if [ $verbose == 1 ]; then
		motr_service start $multiple_pools $stride $N $K $S $P $FI_OPTS
	else
		motr_service start $multiple_pools $stride $N $K $S $P $FI_OPTS >/dev/null 2>&1
	fi
	rc=$?
	if [ $rc -ne "0" ]
	then
		echo "Failed to start Motr Service."
	fi
	return $rc
}

stop_motr()
{
	local rc=0
	echo "Motr service stopping..."
	if [ $verbose == 1 ]; then
		motr_service stop
	else
		motr_service stop >/dev/null 2>&1
	fi
	rc=$?
	if [ $rc -ne "0" ]
	then
		echo "Failed to stop Motr Service."
	fi
	return $rc
}

motr_dtm_st_pre()
{
	sandbox_init
	start_motr || error "Failed to start Motr service"
}

motr_dtm_st_post()
{
	local rc=${1:-$?}

	trap - SIGINT SIGTERM
	stop_motr || rc=$?

#	if [ $rc -eq 0 ]; then
	#	sandbox_fini
#	fi
	report_and_exit $0 $rc
}

function load_item()
{
	local resultvar=$3
	local fil=$1
	local n=$2
	local t=${n}p

	local  myresult=$(sed -n "${t}" < ${fil})
	if [[ "$resultvar" ]]; then
		eval $resultvar="'$myresult'"
	else
		echo "$myresult"
	fi
}

function rm_logs()
{
	rm -rf ${out_file} ${rc_file} ${erc_file} ${res_out_file} >/dev/null
}

st_init()
{
	# generate source files for KEYS, VALS, FIDS
	${MOTRTOOL} ${genf} ${genv} ${genv_bulk} >/dev/null
	[ $? != 0 ] && return 1
	cp ${vals_file} ${keys_file}
	cp ${vals_bulk_file} ${keys_bulk_file}
	fid=$(load_item $fids_file 1)
	key=$(load_item $keys_file 1)
	val=$(load_item $vals_file 1)
	t=${key#* }
	key=$t
	t=${val#* }
	val=$t
	dropf="drop @${fids_file}"
	drop="drop \"${fid}\""
	createf="create @${fids_file}"
	create="create \"${fid}\""

	put_fids_keys="put @${fids_file} @${keys_file} @${vals_file}"
	put_fids_key="put @${fids_file} ${key} ${val}"
	put_fid_keys="put \"${fid}\" @${keys_file} @${vals_file}"
	put_fid_key="put \"${fid}\" ${key} ${val}"
	put_fid_keys_bulk="put \"${fid}\" @${keys_bulk_file} @${vals_bulk_file}"

	del_fids_keys="del @${fids_file} @${keys_file}"
	del_fids_key="del @${fids_file} ${key}"
	del_fid_keys="del \"${fid}\" @${keys_file}"
	del_fid_key="del \"${fid}\" ${key}"

	get_fid_keys="get \"${fid}\" @${keys_file}"
	get_fid_key="get \"${fid}\" ${key}"

	next="next \"${fid}\" ${key} 10"

	lookup="lookup \"${fid}\""
	lookups="lookup @${fids_file}"

	list="list \"${fid}\" ${num}"
	list3="list \"${fid}\" 3"
}

gets1()
{
	local rc=0
	echo "Test:Get KEY"
	emsg="FAILED to get key:${key} from fid:${fid}"
	${MOTRTOOL} ${dropf} >/dev/null
	rc=$?
	[ $rc != 0 ] && return $rc
	${MOTRTOOL} ${create} >/dev/null
	rc=$?
	[ $rc != 0 ] && return $rc
	${MOTRTOOL} ${put_fid_key} >/dev/null
	rc=$?
	[ $rc != 0 ] && return $rc
	${MOTRTOOL} ${get_fid_key} >${out_file}
	rc=$?
	[ $rc != 0 ] && return $rc
	grep "operation rc:" ${out_file} >${rc_file}
	grep -v "operation rc: 0" ${rc_file} >${erc_file}
	[ -s "${erc_file}" ] && return 1
	grep -A 1 "KEY" ${out_file} > ${res_out_file}
	key_line=$(load_item ${res_out_file} 1)
	val_line=$(load_item ${res_out_file} 2)
	key_line=${key_line#"/KEY/"}
	val_line=${val_line#"/VAL/"}
	[ "${val_line}" != "${key_line}" ] && return 1
	[ $(cat ${res_out_file} | wc -l) == 2 ] || return 1
	rm_logs
	return $rc
}

dtm_run_kv_ios()
{
	local rc
	local vflag=''

	if [ $verbose == 1 ] ; then
		vflag="-v"
	fi
	MOTRTOOL="$M0_SRC_DIR/motr/st/utils/m0kv_start.sh local $vflag index"

	st_init
	rc=$?
	if [ $rc -ne 0 ]; then
		echo "Failed to init test $rc"
		return $rc
	fi

	gets1
	rc=$?
	if [ $rc != 0 ]; then
		echo "Failed: $f"
		echo "MSG   : ${emsg}"
		return $rc
	fi
	return $rc
}

# param
# (i)   trace_path Path to the trace file
# (ii)  pattern    String to grep.
# (iii) exp_cnt    Expected number of lines that match the pattern.
expect_trace_lines()
{
    local trace_path="$1"
    local pattern="$2"
    local exp_cnt="$3"
    local cnt
    local cmd;

    echo "expect trace: path=$trace_path, pattern=$pattern, exp_cnt=$exp_cnt"
    cnt=$($M0_SRC_DIR/utils/trace/m0trace -i "$trace_path*" | grep -c "$pattern")
    if [[ $cnt -ne $exp_cnt ]]; then
        echo "Unexpected number of trace lines: $cnt != $exp_cnt"
        return 1
    fi

    echo "Found"
    return 0
}

expect_trace_lines_from_pid()
{
    local dir="$1"
    local pid="$2"
    local pattern="$3"
    local exp_cnt="$4"
    local path="$dir/m0trace.$pid."

    if expect_trace_lines "$path" "$pattern" "$exp_cnt"; then
        return 0;
    fi

    return 1;
}

wait_ios3_to_recover()
{
	local pid=$(pgrep -a m0d | grep $IOS3_FID | awk '{ print $1; }')

	while ! expect_trace_lines_from_pid $IOS3_DIR $pid "ALL2ALL_DTM_RECOVERED" 1;
	do
		sleep 1;
	done

}

restart_ios3()
{
	local rc
	local pid=$(pgrep -a m0d | grep $IOS3_FID | awk '{ print $1; }')

	echo "ios3: pid=$pid fid=$IOS3_FID"
	echo "ios3: cmd=$IOS3_CMD"

	echo "terminating ios3: pid=$pid"
	kill -TERM $pid
	sleep 4
	ps -p $pid
	rc=$?
	echo $rc
	if [ $rc == 0 ]; then
		echo "ios3: pid=$pid is not terminated, killing it now"
		kill -KILL $pid
		sleep 2
	fi
	#dtm_run_kv_ios

	local m0d_log=$IOS3_DIR/m0d.log
	rm -rf $m0d_log
	echo $IOS3_CMD
	(eval "$IOS3_CMD") &

	sleep 1
	wait_ios3_to_recover
}

main()
{
	motr_dtm_st_pre
	if [ $verbose == 1 ]; then
		wait_ios3_to_recover
		restart_ios3
	else
		wait_ios3_to_recover >/dev/null 2>&1
		restart_ios3 >/dev/null 2>&1
	fi
	dtm_run_kv_ios
	motr_dtm_st_post
}

arg_list=("$@")

[ `id -u` -eq 0 ] || die 'Must be run by superuser'

OPTS=`getopt -o vhni --long verbose,help -n 'parse-options' -- "$@"`
if [ $? != 0 ] ; then echo "Failed parsing options." >&2 ; exit 1 ; fi
eval set -- "$OPTS"
while true; do
	case "$1" in
	-v | --verbose ) verbose=1; shift ;;
	-h | --help )    usage ; exit 0; shift ;;
	-- ) shift; break ;;
	* ) break ;;
	esac
done

for arg in "${arg_list[@]}"; do
	if [ "${arg:0:2}" != "--" -a "${arg:0:1}" != "-" ]; then
		tests_list+=(${arg})
	fi
done

if [ ${#tests_list[@]} -eq 0 ]; then
	declare -a tests_list=(motr-start-stop
			      )
fi

trap interrupt SIGINT SIGTERM
main

# Local variables:
# sh-basic-offset: 8
# sh-indentation: 8
# tab-width: 8
# End:
