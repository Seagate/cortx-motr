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


#set -x

motr_st_util_dir=$( cd "$(dirname "$0")" ; pwd -P )
m0t1fs_dir="$motr_st_util_dir/../../../m0t1fs/linux_kernel/st"

# Re-use as many m0t1fs system scripts as possible
. "$m0t1fs_dir"/common.sh
. "$m0t1fs_dir"/m0t1fs_common_inc.sh
. "$m0t1fs_dir"/m0t1fs_client_inc.sh
. "$m0t1fs_dir"/m0t1fs_server_inc.sh
. "$m0t1fs_dir"/m0t1fs_sns_common_inc.sh

. "$motr_st_util_dir"/motr_local_conf.sh
. "$motr_st_util_dir"/motr_st_inc.sh

N=6
K=2
S=0
P=15
stride=32
BLOCKSIZE=""
BLOCKCOUNT=""
OBJ_ID1="1048577"
OBJ_ID2="1048578"
export MOTR_CLIENT_ONLY=1

MOTR_TEST_DIR=$SANDBOX_DIR
MOTR_TEST_LOGFILE=$SANDBOX_DIR/motr_`date +"%Y-%m-%d_%T"`.log
MOTR_TRACE_DIR=$SANDBOX_DIR/motr

main()
{
	sandbox_init

	NODE_UUID=`uuidgen`
	motr_dgmode_sandbox="$MOTR_TEST_DIR/sandbox"
	src_file="$MOTR_TEST_DIR/motr_source"
	dest_file="$MOTR_TEST_DIR/motr_dest"
	dest_file2="$MOTR_TEST_DIR/motr_dest2"
	rc=0

	BLOCKSIZE=16384 #4096
	BLOCKCOUNT=8
	echo "dd if=/dev/urandom bs=$BLOCKSIZE count=$BLOCKCOUNT of=$src_file"
	dd if=/dev/urandom bs=$BLOCKSIZE count=$BLOCKCOUNT of="$src_file" \
              2> "$MOTR_TEST_LOGFILE" || {
		echo "Failed to create a source file"
		motr_service_stop
		return 1
	}

	mkdir "$MOTR_TRACE_DIR"

	motr_service_start $N $K $S $P $stride

	#Initialise dix
	dix_init

	# Currently motr does not provide any API to check attributes of an
	# object. It has to be checked with S3 level or in motr trace logs.

	# write an object
	io_conduct "WRITE" "$src_file" $OBJ_ID1 "false"
	if [ $rc -ne "0" ]
	then
		echo "Write failed."
		error_handling $rc
	fi
	echo "Write succeeds."

	rm -f "$SANDBOX_DIR"/m0cat.log
	# read the written object
	io_conduct "READ" $OBJ_ID1  "$dest_file" "false" "true"
	rc=$?
	if [ $rc -ne "0" ]
	then
		echo "Read failed."
		error_handling $rc
	fi
	echo "Read file succeeds."

	if [[ -f $SANDBOX_DIR/m0cat.log ]] 
	then 
		pver_obj1=$(cat "$SANDBOX_DIR"/m0cat.log | grep 'Object pool version is' | cut -d '=' -f2)
			
		if [ -z "$pver_obj1" ]
		then
			echo "Pool version for pver_obj1 found empty."
			error_handling 1
		fi
	fi
	
	echo "Fail two disks"
	fail_device1=1
	fail_device2=2

	# fail a disk and read an object
	disk_state_set "failed" $fail_device1 $fail_device2 || {
		echo "Operation to mark device failure failed."
		error_handling 1
	}

	io_conduct "WRITE" "$src_file" $OBJ_ID2 "false"
	if [ $rc -ne "0" ]
	then
		echo "Write failed."
		error_handling $rc
	fi
	echo "Write succeeds."

	rm -f "$SANDBOX_DIR"/m0cat.log
	# read the written object
	io_conduct "READ" $OBJ_ID2  "$dest_file2" "false" "true"
	rc=$?
	if [ $rc -ne "0" ]
	then
		echo "Read failed."
		error_handling $rc
	fi
	echo "Read file succeeds."
	
	if [[ -f $SANDBOX_DIR/m0cat.log ]]
	then 
		pver_obj2=$(cat "$SANDBOX_DIR"/m0cat.log | grep 'Object pool version is' | cut -d '=' -f2)
		
		if [ -z "$pver_obj2" ]
        then
        	echo "Pool version for pver_obj2 found empty."
            error_handling 1
        fi
	fi

    rm -f "$SANDBOX_DIR"/m0cat.log

	if [ "$pver_obj1" != "$pver_obj2" ]; then
		echo "Pool version of obj1 and obj2 are different as expected."
	else
		rc=1
		motr_error_handling $rc "Pool version of obj1 and obj2 is same"
	fi

	motr_service_stop || rc=1

	if [ $rc -eq 0 ]; then
		sandbox_fini
	else
		error_handling $rc
	fi
	return $rc
}

echo "pver_assign Test ... "
trap unprepare EXIT
main
report_and_exit pool-version-assignment $?
