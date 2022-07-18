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

# set -x

motr_st_util_dir=$(dirname $(readlink -f $0))
motr_src="$motr_st_util_dir/../../../"
m0t1fs_dir="$motr_src/m0t1fs/linux_kernel/st"

# Re-use as many m0t1fs system scripts as possible
. $m0t1fs_dir/common.sh
. $m0t1fs_dir/m0t1fs_common_inc.sh
. $m0t1fs_dir/m0t1fs_client_inc.sh
. $m0t1fs_dir/m0t1fs_server_inc.sh
. $motr_st_util_dir/motr_local_conf.sh
. $motr_st_util_dir/motr_st_inc.sh

SANDBOX_DIR=/var/motr
MOTR_TEST_DIR=$SANDBOX_DIR
MOTR_TEST_LOGFILE=$SANDBOX_DIR/motr_`date +"%Y-%m-%d_%T"`.log
MOTR_TRACE_DIR=$SANDBOX_DIR/motr
export MOTR_CLIENT_ONLY=1

N=4
K=2
S=2
P=20
rc=0
stride=4
src_file="$MOTR_TEST_DIR/src_file"
update_file="$MOTR_TEST_DIR/update_file"
dest_file="$MOTR_TEST_DIR/dest_file"
object_id=1048580
block_size=1048576
block_count=4
update_offset=4096
MOTR_PARAMS="-l $MOTR_LOCAL_EP -H $MOTR_HA_EP -p $MOTR_PROF_OPT \
	       -P $MOTR_PROC_FID"
create_files()
{
	local update_count=$1

	rm -rf $src_file $src_file'2' $dest_file $update_file
	dd if=/dev/urandom bs=$block_size count=$block_count of=$src_file \
           2> $MOTR_TEST_LOGFILE || {
		error_handling $? "Failed to create a source file"
		break
	}
	cp $src_file $src_file'2'
	dd if=/dev/urandom bs=$block_size count=$update_count of=$update_file\
           2> $MOTR_TEST_LOGFILE || {
		error_handling $? "Failed to create a source file"
		break
	}
	dd conv=notrunc if=$update_file of=$src_file'2' seek=$update_offset oflag=seek_bytes\
           2> $MOTR_TEST_LOGFILE || {
		error_handling $? "Failed to update a source file"
		break
	}
}

write_update_read()
{
	local LID=9
	local update_count=$1

	echo "m0cp"
	$motr_st_util_dir/m0cp -G $MOTR_PARAMS -o $object_id $src_file \
                                -s $block_size -c $block_count -L $LID || {
		error_handling $? "Failed to copy object"
		break
	}

	# echo "m0cp update"
	# $motr_st_util_dir/m0cp -G $MOTR_PARAMS -o $object_id $update_file \
        #                          -s $block_size -c $update_count -L $LID \
        #                          -u -O $update_offset|| {
	# 	error_handling $? "Failed to copy object"
	# 	break
	# }
	echo "m0cat"
	$motr_st_util_dir/m0cat -G $MOTR_PARAMS -o $object_id -s $block_size \
                                  -c $block_count -L $LID \
                                  $dest_file'_'$LID
	if [ "$?" -eq 0 ]; then
            rc=1
        else
            rc=0
        fi
}

test_corruption_detection()
{
	update_count=1
	rm -rf $MOTR_TRACE_DIR
	mkdir $MOTR_TRACE_DIR

	motr_service_start $N $K $S $P $stride

	create_files $update_count
	write_update_read $update_count

	motr_service_stop || rc=1
}

main()
{
	sandbox_init

	NODE_UUID=`uuidgen`
	motr_dgmode_sandbox="$MOTR_TEST_DIR/sandbox"
	rc=0

	echo "Testing DI Corruption Detection"
	test_corruption_detection

	sandbox_fini
	return $rc
}

echo "Motr DI Corruption Detection Test ... "
main
report_and_exit motr_di_corruption_detection_IO $?
