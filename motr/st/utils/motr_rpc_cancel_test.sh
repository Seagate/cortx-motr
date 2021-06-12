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


motr_st_util_dir=$(dirname $(readlink -f $0))
m0t1fs_dir="$motr_st_util_dir/../../../m0t1fs/linux_kernel/st"

. $m0t1fs_dir/common.sh
. $m0t1fs_dir/m0t1fs_common_inc.sh
. $m0t1fs_dir/m0t1fs_client_inc.sh
. $m0t1fs_dir/m0t1fs_server_inc.sh
. $motr_st_util_dir/motr_local_conf.sh
. $motr_st_util_dir/motr_st_inc.sh


MOTR_TEST_DIR=$SANDBOX_DIR
MOTR_TEST_LOGFILE=$SANDBOX_DIR/motr_`date +"%Y-%m-%d_%T"`.log
MOTR_TRACE_DIR=$SANDBOX_DIR/motr
ios2_from_pver0="^s|1:1"

N=3
K=2
S=2
P=15
stride=4

motr_change_controller_state()
{
	local lnet_nid=`sudo lctl list_nids | head -1`
	local s_endpoint="$lnet_nid:12345:34:101"
	local c_endpoint="$lnet_nid:$M0HAM_CLI_EP"
	local dev_fid=$1
	local dev_state=$2

	# Generate HA event
	echo "Send HA event for motr"
	echo "c_endpoint is : $c_endpoint"
	echo "s_endpoint is : $s_endpoint"

	send_ha_events "$dev_fid" "$dev_state" "$s_endpoint" "$c_endpoint"
}

motr_online_session_fop()
{
	echo "Send online fop ha event"
	motr_change_controller_state "$ios2_from_pver0" "online"
	return 0
}

motr_cancel_session_fop()
{
	echo "Send cancel fop ha event"
	motr_change_controller_state "$ios2_from_pver0" "failed"
	return 0
}

motr_cancel_during_write()
{
	object_id=$1;
	block_size=$2;
	block_count=$3;
	src_file=$4;
	dest_file=$5;
	MOTR_PARAMS="-l $MOTR_LOCAL_EP -H $MOTR_HA_EP -p $MOTR_PROF_OPT  \
	-P $MOTR_PROC_FID"
	MOTR_CP_PARAMS="-o $object_id $src_file -s $block_size -c $block_count"
	MOTR_CAT_PARAMS="-o $object_id -s $block_size -c $block_count"

	echo "Cancel during write params"
	echo "Object id : $object_id"
	echo "block_size : $block_size"
	echo "block count : $block_count"
	echo "src file : $src_file"
	echo "dest file : $dest_file"

	local cp_cmd="$motr_st_util_dir/m0cp $MOTR_PARAMS $MOTR_CP_PARAMS  \
	&> $MOTR_TRACE_DIR/m0cp.log  &"

	echo "Executing command: $cp_cmd"
	date
	eval $cp_cmd
	pid=$!

	echo "Wait for few seconds to generate enough fops"
	sleep 3

	echo "Sending cancel fop"
	motr_cancel_session_fop

	echo "Wait for m0cp: $pid to be finished "
	wait $pid
	echo "Copy operation complete"

	# Check for session cancelled messages in m0cp logs
	echo "Check for session cancelled message"
	rc=`cat $MOTR_TRACE_DIR/m0cp.log | grep 'rc=-125' | grep -v grep | wc -l`
	if [ $rc -ne "0" ]
	then
		echo "Cancelled $rc fops during write operation"
	else
		# Probably m0cp operation completed before cancel event passed
		# Hence no fops cancelled
		echo "No fops cancelled during write operation"
		return 1
	fi

	# If session cancelled successfully during write, then same object
	# read should not be same as src file.
	echo "Check for file difference "
	local cat_cmd="$motr_st_util_dir/m0cat $MOTR_PARAMS $MOTR_CAT_PARAMS $dest_file  "
	eval $cat_cmd

	diff $src_file $dest_file
	rc=$?
	if [ $rc -ne "0" ]
	then
		echo "Files are not equal"
	else
		# Probably m0cp operation completed before cancel event passed
		# And hence no difference between src and dest file
		echo "Files are equal, means no fops cancelled during write"
		return 1
	fi

}


motr_cancel_during_read()
{
	object_id=$1;
	block_size=$2;
	block_count=$3;
	src_file=$4;
	dest_file=$5;
	MOTR_PARAMS="-l $MOTR_LOCAL_EP -H $MOTR_HA_EP -p $MOTR_PROF_OPT  \
	-P $MOTR_PROC_FID"
	MOTR_CP_PARAMS="-o $object_id $src_file -s $block_size -c $block_count"
	MOTR_CAT_PARAMS="-o $object_id -s $block_size -c $block_count"

	echo "Cancel during read params"
	echo "Object id : $object_id"
	echo "block_size : $block_size"
	echo "block count : $block_count"
	echo "src file : $src_file"
	echo "dest file : $dest_file"

	echo "Write the object first before reading it"
	local cp_cmd="$motr_st_util_dir/m0cp $MOTR_PARAMS $MOTR_CP_PARAMS  \
	&> $MOTR_TRACE_DIR/m0cp.log  &"

	echo "Executing command: $cp_cmd"
	date
	eval $cp_cmd
	pid=$!

	echo "Wait for m0cp: $pid to be finished "
	wait $pid
	echo "Copy operation complete"

	echo "Read the object with object id $object_id"
	local cat_cmd="$motr_st_util_dir/m0cat $MOTR_PARAMS $MOTR_CAT_PARAMS $dest_file 2> $MOTR_TRACE_DIR/m0cat.log &"
	eval $cat_cmd
	pid=$!
	date
	echo "Sleep for few seconds to generate enough fops"
	sleep 2

	echo "Sending cancel fop"
	motr_cancel_session_fop

	echo "Wait for m0cat:$pid to finish"
	wait $pid
	echo "Cat operation is completed "
	date

	# Check for session cancelled messages into m0cat logs
	echo "Check for session cancelled message"
	rc=`cat $MOTR_TRACE_DIR/m0cat.log | grep 'rc=-125' | grep -v grep | wc -l`
	if [ $rc -ne "0" ]
	then
		echo "Cancelled $rc fops during read operation"
	else
		# Probably m0cat operation completed before cancel event passed
		# Hence no fops cancelled
		echo "No fops cancelled during read operation"
		return 1
	fi

	# If session cancelled successfully during read, then same object
	# read should not be same as src file.
	echo "Check for difference between src file and dest file"
	diff $src_file $dest_file
	rc=$?
	if [ $rc -ne "0" ]
	then
		echo "Files are not equal"
	else
		# Probably m0cat operation completed before cancel event passed
		# And hence no difference between src and dest file
		echo "Files are equal, means no fops cancelled during write"
		return 1
	fi
}

motr_cancel_during_create()
{
	object_id=$1;
	n_obj=$2;

	MOTR_PARAMS="-l $MOTR_LOCAL_EP -H $MOTR_HA_EP -p $MOTR_PROF_OPT  \
	-P $MOTR_PROC_FID"

	echo "Cancel during create parameters"
	echo "Object id : $object_id"
	echo "no of object : $n_obj"

	local touch_cmd="$motr_st_util_dir/m0touch $MOTR_PARAMS -o $object_id -n $n_obj &> $MOTR_TRACE_DIR/m0touch.log &"
	echo "Command : $touch_cmd"
	date
	eval $touch_cmd
	pid=$!

	echo "Wait for few seconds to generate enough fops "
	sleep 3

	echo "Sending cancle fop"
	motr_cancel_session_fop

	echo "Wait for m0touch: $pid to finished "
	wait $pid
	date
	echo "Done"

	# Check for session cancelled messages into m0touch logs
	echo "Check for session cancelled message"
	rc=`cat $MOTR_TRACE_DIR/m0touch.log | grep 'rc=-125' | grep -v grep | wc -l`
	if [ $rc -ne "0" ]
	then
		echo "Cancelled $rc fops during create operation"
	else
		# Probably m0touch operation completed before cancel event passed
		# Hence no fops cancelled
		echo "No fops cancelled during create operation"
		return 1
	fi

	return 0
}


motr_cancel_during_unlink()
{
	object_id=$1;
	n_obj=$2;

	MOTR_PARAMS="-l $MOTR_LOCAL_EP -H $MOTR_HA_EP -p $MOTR_PROF_OPT  \
	-P $MOTR_PROC_FID"

	echo "Cancel during unlink parameters"
	echo "Object id : $object_id"
	echo "no of object : $n_obj"

	# Create objects first to unlink
	local touch_cmd="$motr_st_util_dir/m0touch $MOTR_PARAMS -o $object_id -n $n_obj &> $MOTR_TRACE_DIR/m0touch.log &"
	echo "Command : $touch_cmd"
	date
	eval $touch_cmd
	pid=$!

	echo "Wait for m0touch: $pid to finished "
	wait $pid
	date
	echo "Create Done"

	sleep 10
	local unlink_cmd="$motr_st_util_dir/m0unlink $MOTR_PARAMS -o $object_id -n $n_obj &> $MOTR_TRACE_DIR/m0unlink.log &"
	echo "Command : $unlink_cmd"
	date
	eval $unlink_cmd
	pid=$!

	echo "Wait for few seconds to generate enough fops "
	sleep 15

	echo "Sending cancle fop"
	motr_cancel_session_fop

	echo "Wait for m0unlink: $pid to finished "
	wait $pid
	date
	echo "Unlink Done"

	# Check for session cancelled messages into m0unlink logs
	echo "Check for session cancelled message"
	rc=`cat $MOTR_TRACE_DIR/m0unlink.log | grep 'rc=-125' | grep -v grep | wc -l`
	if [ $rc -ne "0" ]
	then
		echo "Cancelled $rc fops during unlink operation"
	else
		# Probably m0unlink operation completed before cancel event passed
		# Hence no fops cancelled
		echo "No fops cancelled during unlink operation"
		return 1
	fi

	return 0
}

main()
{
	motr_dgmode_sandbox="$MOTR_TEST_DIR/sandbox"
	src_file="$MOTR_TEST_DIR/src_file"
	dest_file1="$MOTR_TEST_DIR/dest_file1"
	dest_file2="$MOTR_TEST_DIR/dest_file2"
	object_id1=1048580
	object_id2=1048581
	object_id3=1048587
	block_size=8192
	block_count=16384
	n_obj=400	# No of objects to create/unlink
	n_obj2=200	# No of objects to create/unlink

	sandbox_init

	echo "Creating src file"
	dd if=/dev/urandom bs=$block_size count=$block_count of=$src_file \
	      2> $MOTR_TEST_LOGFILE || {
		error_handling $? "Failed to create a source file"
	}
	echo "Make motr test directory $MOTR_TRACE_DIR"
	mkdir $MOTR_TRACE_DIR

	echo "Starting motr services"
	motr_service_start $N $K $S $P $stride

	echo "=========================================================="
	echo "TC1. Motr RPC cancel during motr write."
	echo "=========================================================="
	motr_cancel_during_write $object_id1 $block_size $block_count \
		$src_file $dest_file1
	if [ $? -ne "0" ]
	then
		echo "Failed to run the test, Stop motr services and exit"
		motr_service_stop
		return 1
	fi
	echo "=========================================================="
	echo "TC1. Motr cancel during motr write complete."
	echo "=========================================================="


	echo "=========================================================="
	echo "TC2. Motr RPC cancel during motr read."
	echo "=========================================================="
	motr_cancel_during_read $object_id2 $block_size $block_count \
		$src_file $dest_file2
	if [ $? -ne "0" ]
	then
		echo "Failed to run the test, Stop motr services and exit"
		motr_service_stop
		return 1
	fi
	echo "=========================================================="
	echo "TC2. Motr cancel during motr read complete."
	echo "=========================================================="

	echo "=========================================================="
	echo "TC3. Motr RPC cancel during motr create."
	echo "=========================================================="
	motr_cancel_during_create $object_id3 $n_obj
	if [ $? -ne "0" ]
	then
		echo "Failed to run the test, Stop motr services and exit"
		motr_service_stop
		return 1
	fi
	echo "=========================================================="
	echo "TC3. Motr cancel during motr create complete."
	echo "=========================================================="

	echo "=========================================================="
	echo "TC4. Motr RPC cancel during motr unlink."
	echo "=========================================================="
	motr_cancel_during_unlink $object_id2 $n_obj2
	if [ $? -ne "0" ]
	then
		echo "Failed to run the test, Stop motr services and exit"
		motr_service_stop
		return 1
	fi
	echo "=========================================================="
	echo "TC4. Motr cancel during motr unlink complete."
	echo "=========================================================="

	echo "Stopping Motr services"
	motr_service_stop

	sandbox_fini
	return 0;
}

echo "motr RPC cancel test..."
main
report_and_exit motr-rpc-cancel $?
