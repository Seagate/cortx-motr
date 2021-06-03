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
. $m0t1fs_dir/common.sh
. $m0t1fs_dir/m0t1fs_common_inc.sh
. $m0t1fs_dir/m0t1fs_client_inc.sh
. $m0t1fs_dir/m0t1fs_server_inc.sh
. $m0t1fs_dir/m0t1fs_sns_common_inc.sh

. $motr_st_util_dir/motr_local_conf.sh
. $motr_st_util_dir/motr_st_inc.sh

N=3
K=3
P=15
stride=32
BLOCKSIZE=""
BLOCKCOUNT=""
OBJ_ID1="1048577"
OBJ_ID2="1048578"
# The second half is hex representations of OBJ_ID1 and OBJ_ID2.
OBJ_HID1="0:100001"
OBJ_HID2="0:100002"
PVER_1="7600000000000001:a"
PVER_2="7680000000000001:42"
read_verify="true"
motr_pids=""
export cnt=1

# Dgmode IO

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
	rc=0

	BLOCKSIZE=4096
	BLOCKCOUNT=25
	echo "dd if=/dev/urandom bs=$BLOCKSIZE count=$BLOCKCOUNT of=$src_file"
	dd if=/dev/urandom bs=$BLOCKSIZE count=$BLOCKCOUNT of=$src_file \
              2> $MOTR_TEST_LOGFILE || {
		echo "Failed to create a source file"
		unmount_and_clean &>>$MOTR_TEST_LOGFILE
		motr_service_stop
		return 1
	}

	mkdir $MOTR_TRACE_DIR

	motr_service_start $N $K $P $stride

	#Initialise dix
	dix_init

	#mount m0t1fs as well. This helps in two ways:
	# 1) Currently motr does not have a utility to check attributes of an
	#    object. Hence checking of attributes is done by fetching them via
	#    m0t1fs.
	# 2) A method to send HA notifications assumes presence of m0t1fs. One
	#    way to circumvent this is by assigning same end-point to motr,
	#    but creating a motr instance concurrently with HA notifications
	#    is hard. Another way is to re-write the method to send HA
	#    notifications by excluding m0t1fs end-point. We have differed these
	#    changes in current patch.
	local mountopt="oostore,verify"
	mount_m0t1fs $MOTR_M0T1FS_MOUNT_DIR $mountopt || return 1

	# write an object
	io_conduct "WRITE" $src_file $OBJ_ID1 "false"
	if [ $rc -ne "0" ]
	then
		echo "Healthy mode, write failed."
		error_handling $rc
	fi
	echo "Healthy mode write succeeds."

	# read the written object
	io_conduct "READ" $OBJ_ID1  $dest_file "false"
	rc=$?
	if [ $rc -ne "0" ]
	then
		echo "Healthy mode, read failed."
		error_handling $rc
	fi
	diff $src_file $dest_file
	rc=$?
	if [ $rc -ne "0" ]
	then
		echo "Healthy mode, read file differs."
		error_handling $rc
	fi
	echo "Healthy mode, read file succeeds."
	# read the written object
	io_conduct "READ" $OBJ_ID1  $dest_file $read_verify
	rc=$?
	if [ $rc -ne "0" ]
	then
		echo "Healthy mode, read verify failed."
		error_handling $rc
	fi
	diff $src_file $dest_file
	rc=$?
	if [ $rc -ne "0" ]
	then
		echo "Healthy mode, read file differs."
		error_handling $rc
	fi
	echo "Healthy mode, read file with parity verify succeeds"
	echo "Motr: Healthy mode IO succeeds."
	echo "Fail two disks"
	fail_device1=1
	fail_device2=2

	# fail a disk and read an object
	disk_state_set "failed" $fail_device1 $fail_device2 || {
		echo "Operation to mark device failure failed."
		error_handling 1
	}

	# Test degraded read
	rm -f $dest_file
	io_conduct "READ" $OBJ_ID1 $dest_file "false"
	rc=$?
	if [ $rc -ne "0" ]
	then
		echo "Degraded read failed."
		error_handling $rc
	fi
	echo "Dgmode Read of 1st obj succeeds."
	diff $src_file $dest_file
	rc=$?
	if [ $rc -ne "0" ]
	then
		echo "Obj read in degraded mode differs."
		error_handling $rc
	fi
	rm -f $dest_file

	#Dgmode read of with Parity Verify.
	io_conduct "READ" $OBJ_ID1 $dest_file $read_verify
	rc=$?
	if [ $rc -ne "0" ]
	then
		echo "Degraded read failed."
		error_handling $rc
	fi
	echo "Dgmode Parity verify Read of 1st obj succeeds."
	diff $src_file $dest_file
	rc=$?
	if [ $rc -ne "0" ]
	then
		echo "Obj read in degraded mode differs."
		error_handling $rc
	fi
	rm -f $dest_file

	# Test write, when a disk is failed
	io_conduct "WRITE" $src_file $OBJ_ID2 "false"
	rc=$?
	if [ $rc -ne "0" ]
	then
		echo "Degraded write failed."
		error_handling $rc
	fi
	echo "New Obj write succeeds."
	echo "Check pver of the first object"
	output=`getfattr -n pver $MOTR_M0T1FS_MOUNT_DIR/"$OBJ_HID1"`
	echo $output
	if [[ $output != *"$PVER_1"* ]]
	then
		echo "getattr failed on $OBJ_HID1."
		error_handling 1
	fi
	echo "Check pver of the second object, created post device failure."
	output=`getfattr -n pver $MOTR_M0T1FS_MOUNT_DIR/"$OBJ_HID2"`
	echo $output
	if [[ $output != *"$PVER_2"* ]]
	then
		echo "getattr failed on $OBJ_HID2"
		error_handling 1
	fi
	rm -f $dest_file

	echo "Fail another disk"
	fail_device3=3
	# fail a disk and read an object
	disk_state_set "failed" $fail_device3 || {
		echo "Operation to mark device failure failed."
		error_handling 1
	}


	# Read a file from the new pool version.
	io_conduct "READ" $OBJ_ID2 $dest_file "false"
	rc=$?
	if [ $rc -ne "0" ]
	then
		echo "Reading a file from a new pool version failed."
		error_handling $rc
	fi
	diff $src_file $dest_file
	rc=$?
	if [ $rc -ne "0" ]
	then
		echo "File read from a new pool version differs."
		error_handling $rc
	fi
	echo "Motr: Dgmod mode read from new pver succeeds."

	#Read in Parity Verify from new pool version.
	io_conduct "READ" $OBJ_ID2 $dest_file $read_verify
	rc=$?
	if [ $rc -ne "0" ]
	then
		echo "Reading a file from a new pool version failed."
		error_handling $rc
	fi
	diff $src_file $dest_file
	rc=$?
	if [ $rc -ne "0" ]
	then
		echo "File read from a new pool version differs."
		error_handling $rc
	fi
	echo "Motr: Dgmod mode read verify from new pver succeeds."
	echo "Motr: Dgmod mode IO succeeds."
	motr_inst_cnt=`expr $cnt - 1`
	for i in `seq 1 $motr_inst_cnt`
	do
		echo "motr pids=${motr_pids[$i]}" >> $MOTR_TEST_LOGFILE
	done

	unmount_and_clean &>> $MOTR_TEST_LOGFILE
	motr_service_stop || rc=1

	if [ $rc -eq 0 ]; then
		sandbox_fini
	else
		error_handling $rc
	fi
	return $rc
}

echo "DGMODE IO Test ... "
trap unprepare EXIT
main
report_and_exit degraded-mode-IO $?
