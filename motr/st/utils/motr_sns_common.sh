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

BLOCKSIZE=""
BLOCKCOUNT=""
OBJ_ID1="1048577"
OBJ_ID2="1048578"
# The second half is hex representations of OBJ_ID1 and OBJ_ID2.
OBJ_HID1="0:100001"
OBJ_HID2="0:100002"
PVER_1="7600000000000001:a"
PVER_2="7680000000000000:4"
motr_pids=""
parity_verify="true"
export cnt=1

MOTR_TEST_DIR=$SANDBOX_DIR
MOTR_TEST_LOGFILE=$SANDBOX_DIR/motr_`date +"%Y-%m-%d_%T"`.log
MOTR_TRACE_DIR=$SANDBOX_DIR/motr

motr_sns_repreb()
{
	N=$1
	K=$2
	P=$3
	stride=$4

	sandbox_init

	NODE_UUID=`uuidgen`
	motr_dgmode_sandbox="$MOTR_TEST_DIR/sandbox"
	src_file="$MOTR_TEST_DIR/motr_source"
	dest_file="$MOTR_TEST_DIR/motr_dest"
	rc=0

	dd if=/dev/urandom bs=4K count=100 of=$src_file 2> $MOTR_TEST_LOGFILE || {
		echo "Failed to create a source file"
		motr_service_stop
		return 1
	}
	mkdir $MOTR_TRACE_DIR

	motr_service_start $N $K $P $stride
	#Initialise dix
	dix_init

	BLOCKSIZE="4096"
	BLOCKCOUNT="100"

	# write an object
	io_conduct "WRITE" $src_file $OBJ_ID1 $parity_verify
	if [ $rc -ne "0" ]
	then
		echo "Healthy mode, write failed."
		error_handling $rc
	fi

	# read the written object
	io_conduct "READ" $OBJ_ID1 $dest_file $parity_verify
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
	echo "Motr: Healthy mode IO succeeds."
	echo "Fail a disk"
	fail_device1=1
	fail_device2=9

	# fail a disk and read an object
	motr_st_disk_state_set "failed" $fail_device1 $fail_device2 || {
		echo "Operation to mark device failure failed."
		error_handling 1
	}

	# Test degraded read
	rm -f $dest_file
	io_conduct "READ" $OBJ_ID1 $dest_file $parity_verify
	rc=$?
	if [ $rc -ne "0" ]
	then
		echo "Degraded read failed."
		error_handling $rc
	fi
	diff $src_file $dest_file
	rc=$?
	if [ $rc -ne "0" ]
	then
		echo "Obj read in degraded mode differs."
		error_handling $rc
	fi
	rm -f $dest_file

	motr_st_disk_state_set "repair" $fail_device1 $fail_device2 || {
		echo "Operation to mark device repair failed."
		error_handling 1
	}

	sns_repair || {
		echo "Operation to start sns repair failed."
		error_handling 1
	}

	echo "Start concurrent io during sns repair"
	io_conduct "READ" $OBJ_ID1  $dest_file $parity_verify
	rc=$?
	if [ $rc -ne "0" ]
	then
		echo "concurrent read during sns repair failed."
		error_handling $rc
	fi

	echo "Concurrent io during sns repair successful"

	echo "wait for sns repair"
	wait_for_sns_repair_or_rebalance "repair" || {
		echo "Failure occurred while waiting for sns repair to complete."
		error_handling 1
	}

	echo "query sns repair status"
	sns_repair_or_rebalance_status "repair" || {
		echo "SNS repair status query failure."
		error_handling 1
	}

	motr_st_disk_state_set "repaired" $fail_device1 $fail_device2 || {
		echo "Operation to mark device repaired failed."
		error_handling 1
	}
	echo "SNS Repair done."

	echo "Read after sns repair"

	rm -f $dest_file
	io_conduct "READ" $OBJ_ID1 $dest_file $parity_verify
	rc=$?
	if [ $rc -ne "0" ]
	then
		echo "read failure after sns repair."
		error_handling $rc
	fi
	diff $src_file $dest_file
	rc=$?
	if [ $rc -ne "0" ]
	then
		echo "Obj read after sns repair differs."
		error_handling $rc
	fi
	echo "Motr: Read after repair successfull."
	rm -f $dest_file

	echo "Starting SNS Re-balance for device $fail_device1 $fail_device2"
	motr_st_disk_state_set "rebalance" $fail_device1 $fail_device2 || {
		echo "Operation to mark device rebalance failed."
		error_handling 1
	}

	sns_rebalance || {
		echo "Operation to start sns rebalance failed."
		error_handling 1
	}

	echo "wait for sns rebalance"
	wait_for_sns_repair_or_rebalance "rebalance" || {
		echo "Failure occurred while waiting for sns rebalance to complete."
		error_handling 1
	}

	echo "query sns rebalance status"
	sns_repair_or_rebalance_status "rebalance" || {
		echo "SNS rebalance status query failure."
		error_handling 1
	}

	motr_st_disk_state_set "online" $fail_device1 $fail_device2 || {
		echo "Operation to mark device online failed."
		error_handling 1
	}
	echo "SNS Rebalance done."

	rm -f $dest_file
	io_conduct "READ" $OBJ_ID1 $dest_file $parity_verify
	rc=$?
	if [ $rc -ne "0" ]
	then
		echo "read failure after sns rebalance."
		error_handling $rc
	fi
	diff $src_file $dest_file
	rc=$?
	if [ $rc -ne "0" ]
	then
		echo "Obj read after sns rebalance differs."
		error_handling $rc
	fi
	rm -f $dest_file

	motr_inst_cnt=`expr $cnt - 1`
	for i in `seq 1 $motr_inst_cnt`
	do
		echo "motr pids=${motr_pids[$i]}" >> $MOTR_TEST_LOGFILE
	done

	motr_service_stop || rc=1

	if [ $rc -eq 0 ]; then
		sandbox_fini
	else
		error_handling $rc
	fi
	return $rc
}
