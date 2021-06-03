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

N=3
K=2
P=20
stride=4
src_file="$MOTR_TEST_DIR/src_file"
update_file="$MOTR_TEST_DIR/update_file"
dest_file="$MOTR_TEST_DIR/dest_file"
object_id=1048580
block_size=4096
block_count=5200
MOTR_PARAMS="-l $MOTR_LOCAL_EP -H $MOTR_HA_EP -p $MOTR_PROF_OPT \
	       -P $MOTR_PROC_FID"
update_offset=4096
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

#TODO: Add functionality in `m0layout` to return disk ids
#      where data unit is present for a particular parity group.
disk_to_fail()
{
	local fail_disk=""
	no_disk=$K
	disk_counter=1
	layout_map=`$motr_src/utils/m0layout $N $K $P 5 100 0 $object_id | sed '3q;d' | cut -c 9-`
	layout_map="$(echo "${layout_map}" | tr -d '[:space:]')"
	for (( i=1; i<${#layout_map}; i+=5, disk_counter++ ))
	do
		if [ ${layout_map:$i:1} -eq 0 ]; then
			if [ ${layout_map:$((i+2)):1} -lt $no_disk ]; then
				fail_disk+="$disk_counter "
			fi
		fi
	done
	fail_disk="$(echo -e "${fail_disk}" | sed -e 's/[[:space:]]*$//')"
	echo "$fail_disk"
}

bring_disk_online()
{
	local failed_disk=$(disk_to_fail)
	disk_state_set "repair" $failed_disk || {
		echo "Operation to mark device repair failed."
		error_handling 1
	}
	disk_state_set "repaired" $failed_disk || {
		echo "Operation to mark device repaired failed."
		error_handling 1
	}
	disk_state_set "rebalance" $failed_disk || {
		echo "Operation to mark device rebalanced failed."
		error_handling 1
	}
	disk_state_set "online" $failed_disk || {
		echo "Operation to mark device online failed."
		error_handling 1
	}
}

write_and_update()
{
	local LID=$1
	local update_count=$2

	echo "m0cp"
	$motr_st_util_dir/m0cp $MOTR_PARAMS -o $object_id $src_file \
                                -s $block_size -c $block_count -L $LID || {
		error_handling $? "Failed to copy object"
		break
	}
	if [ "$3" = true ]
	then
		# fail a disk and read an object
		local failed_disk=$(disk_to_fail)
		disk_state_set "failed" $failed_disk || {
			echo "Operation to mark device failure failed."
			error_handling 1
		}
	fi
	echo "m0cp update"
	$motr_st_util_dir/m0cp $MOTR_PARAMS -o $object_id $update_file \
                                 -s $block_size -c $update_count -L $LID \
                                 -u -O $update_offset|| {
		error_handling $? "Failed to copy object"
		break
	}
	echo "m0cat"
	$motr_st_util_dir/m0cat $MOTR_PARAMS -o $object_id -s $block_size \
                                  -c $block_count -L $LID \
                                  $dest_file'_'$LID || {
		error_handling $? "Failed to read object"
		break
	}
	echo "m0unlink"
	$motr_st_util_dir/m0unlink $MOTR_PARAMS -o $object_id -L $LID || {
		error_handling $? "Failed to delete object"
		break
	}
}

#A test for RMW code path in motr.
#It tests by writing a file in motr and then updating the file to trigger RMW.
#Tests are done in Healthy mode and Degraded mode by default.
#RMW is tested against Layout IDs 1,3,5,7,9.
#param1: N, number of data untis in a parity group.
#        N = 1 is Replicated Layout.
#        N < 1 is Erasure Coded Layout.
#param2: false, to disable degraded mode test.
#param3: false, to disable healthy mode tests.
#
#Usage: test_rmw N dg_mode healthy_mode

#example:-
# test_rmw N (for tests in both mode).
# test_rmw N false (for only healthy mode).
# test_rmw N true false (for only degraded mode).

#XXX: Currently this test parses the output of `m0layout` to identify
#     the disks to fail in degraded mode test. A more greceful way is needed
#     to get the disk ids from `m0layout` as any change in ouptut pattern in
#     `m0layout` would crash this test.
test_rmw()
{
	rm -rf $MOTR_TRACE_DIR
	mkdir $MOTR_TRACE_DIR

	local healthy_mode=true
	local dg_mode=true
	local layout_type=""

	if [ "$2" = "false" ]
	then
		dg_mode=false
	fi

	if [ "$3" = "false" ]
	then
		healthy_mode=false
	fi

	motr_service_start $1 $K $P $stride

	#Initialise dix
	dix_init

	#mount m0t1fs becuase a method to send HA notifications assumes presence of m0t1fs.
	local mountopt="oostore,verify"
	mount_m0t1fs $MOTR_M0T1FS_MOUNT_DIR $mountopt || return 1

	for i in 3 5 7 9 11
	do
		local LID=$i
		if [ $1 -eq 1 ]
		then
			# Replicated Layout
			local update_count=$(( 2 ** (LID-1) ))
			layout_type="Replicated Layout"
		else
			# EC Layout
			local update_count=$(( (N-1) * 2 ** (LID-1) ))
			layout_type="EC Layout"
		fi

		if [ "$healthy_mode" = "true" ]
		then
			echo "Testing with layout ID $LID in Healthy mode"
			create_files $update_count

			write_and_update $LID $update_count false

			echo "diff"
			diff $src_file'2' $dest_file'_'$LID || {
				rc=$?
				echo -n "Files are different for $layout_type"
				echo " in Healthy Mode with LID $LID"
				error_handling $rc
				break
			}
		fi

		if [ "$dg_mode" = "true" ]
		then
			echo "Testing with layout ID $LID in Degraded mode"
			create_files $update_count

			write_and_update $LID $update_count true

			echo "diff"
			diff $src_file'2' $dest_file'_'$LID || {
				rc=$?
				echo -n "Files are different for $layout_type"
				echo " in Degraded Mode with LID $LID"
				error_handling $rc
				break
			}

			bring_disk_online
		fi
	done

	unmount_and_clean &>>$MOTR_TEST_LOGFILE
	motr_service_stop || rc=1
}


test_updt_lt_unit_rmw()
{
	rm -rf $MOTR_TRACE_DIR
	mkdir $MOTR_TRACE_DIR

	motr_service_start 1 4 $P $stride

	#Initialise dix
	dix_init

	#mount m0t1fs becuase a method to send HA notifications assumes presence of m0t1fs.
	local mountopt="oostore,verify"
	mount_m0t1fs $MOTR_M0T1FS_MOUNT_DIR $mountopt || return 1

	for i in 3 5 7 9 11 13
	do
		local LID=$i
		echo "Testing with layout ID $LID in Healthy mode"
		local update_count=1
		create_files $update_count

		write_and_update $LID $update_count false

		echo "diff"
		diff $src_file'2' $dest_file'_'$LID || {
			rc=$?
			echo "Files are different"
			error_handling $rc
			break
		}
	done

	unmount_and_clean &>>$MOTR_TEST_LOGFILE
	motr_service_stop || rc=1
}

main()
{
	sandbox_init

	NODE_UUID=`uuidgen`
	motr_dgmode_sandbox="$MOTR_TEST_DIR/sandbox"
	rc=0

	echo "Testing RMW in Erasure Coded layout"
	test_rmw $N false
#	test_rmw $N true false

	echo "Testing RMW in Replicaetd layout"
	test_rmw 1 false
#	test_rmw 1 true false

	echo "Testing RMW when update size < unit size"
	test_updt_lt_unit_rmw

	sandbox_fini
	return $rc
}

echo "Motr RMW IO Test ... "
main
report_and_exit motr_rmw_IO $?
