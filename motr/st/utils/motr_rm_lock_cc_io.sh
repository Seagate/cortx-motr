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


motr_st_util_dir=$(dirname $(realpath $0))
m0t1fs_dir="$motr_st_util_dir/../../../m0t1fs/linux_kernel/st"

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

#Checking the version of file read to be in non-decreasing order.
check_file_version()
{
	i=1
	j=1

	while (( j<=$reader_numb ))
	do
		if (( i>$writer_numb ))
		then
			return 1
		fi

		diff $dest_file$j $src_file$i > /dev/null || {
			((i++))
			continue
		}
		((j++))
	done

	return 0
}

main()
{
	sandbox_init

	NODE_UUID=`uuidgen`
	motr_dgmode_sandbox="$MOTR_TEST_DIR/sandbox"
	src_file="$MOTR_TEST_DIR/src_file"
	dest_file="$MOTR_TEST_DIR/dest_file"
	object_id=1048580
	block_size=4096
	block_count=2048
	writer_numb=5
	reader_numb=5
	SRC_FILES=""
	DEST_FILES=""
	MOTR_PARAMS="-l $MOTR_LOCAL_EP -H $MOTR_HA_EP -p $MOTR_PROF_OPT \
                       -P $MOTR_PROC_FID"
	MOTR_PARAMS_2="-l $MOTR_LOCAL_EP2 -H $MOTR_HA_EP -p $MOTR_PROF_OPT \
                         -P $MOTR_PROC_FID"
	MOTR_PARAMS_3="-l $MOTR_LOCAL_EP3 -H $MOTR_HA_EP -p $MOTR_PROF_OPT \
                         -P $MOTR_PROC_FID"
	MOTR_PARAMS_4="-l $MOTR_LOCAL_EP4 -H $MOTR_HA_EP -p $MOTR_PROF_OPT \
                         -P $MOTR_PROC_FID"

	rm -f $src_file $dest_file $src_file'1'

	dd if=/dev/urandom bs=$block_size count=$block_count of=$src_file \
           2> $MOTR_TEST_LOGFILE || {
		motr_error_handling $? "Failed to create a source file"
	}
	dd if=/dev/urandom bs=$block_size count=$block_count of=$src_file'1' \
           2> $MOTR_TEST_LOGFILE || {
		MOTR_error_handling $? "Failed to create a source file"
	}
	mkdir $MOTR_TRACE_DIR

	for (( i=1; i<=$writer_numb; i++ ))
	do
		tmp_src="$src_file$i"
		rm -f $tmp_src
		dd if=/dev/urandom bs=$block_size count=$block_count of=$tmp_src \
                   2> $MOTR_TEST_LOGFILE || {
			motr_error_handling $? "Failed to create a source file"
	        }
		SRC_FILES+=" $tmp_src"
	done
        SRC_FILES="$(echo -e "${SRC_FILES}" | sed -e 's/^[[:space:]]*//')"

	for (( j=1; j<=$reader_numb; j++ ))
	do
		tmp_dest="$dest_file$j"
		rm -f $tmp_dest
		DEST_FILES+=" $tmp_dest"
	done
        DEST_FILES="$(echo -e "${DEST_FILES}" | sed -e 's/^[[:space:]]*//')"

	motr_service_start
	dix_init

##############################################################################
	echo "Read obj while write/update is in process."

	$motr_st_util_dir/m0cp $MOTR_PARAMS_2 -o $object_id $src_file \
                                 -s $block_size -c $block_count -e &
	pid=$!
	sleep 2

	$motr_st_util_dir/m0cat $MOTR_PARAMS -o $object_id -s $block_size \
                                  -c $block_count -e $dest_file || {
		motr_error_handling $? "Failed to read object"
	}

	wait $pid
	diff $src_file $dest_file || {
		rc = $?
		motr_error_handling $rc "Files are different when concurrent read/write"
	}

	$motr_st_util_dir/m0unlink $MOTR_PARAMS -o $object_id || {
		motr_error_handling $? "Failed to delete object"
	}

	rm -f $dest_file
###############################################################################
	echo "Delete obj while write/update is in process."

	$motr_st_util_dir/m0cp $MOTR_PARAMS_2 -o $object_id $src_file \
                                 -s $block_size -c $block_count -e &

	pid=$!
	sleep 2

	$motr_st_util_dir/m0unlink $MOTR_PARAMS -o $object_id -e || {
		motr_error_handling $? "Failed to delete object"
	}
	wait $pid

###############################################################################
	echo "Delete obj while read is in process."

	$motr_st_util_dir/m0cp $MOTR_PARAMS -o $object_id $src_file \
                                -s $block_size -c $block_count -e || {
		motr_error_handling $? "Failed to copy object"
	}

	$motr_st_util_dir/m0cat $MOTR_PARAMS_2 -o $object_id -s $block_size \
                                 -c $block_count -e $dest_file &
	pid=$!
	sleep 2

	$motr_st_util_dir/m0unlink $MOTR_PARAMS -o $object_id -e || {
		motr_error_handling $? "Failed to delete object"
	}

	wait $pid
	diff $src_file $dest_file || {
		rc = $?
		motr_error_handling $rc "Files are different when concurrent delete/write"
	}

	rm -f $dest_file
#############################################################################
	echo "Test exclusivity among Readers and Writers"
	$motr_st_util_dir/m0cp $MOTR_PARAMS -o $object_id $src_file \
                                -s $block_size -c $block_count -e || {
		motr_error_handling $? "Failed to copy object"
	}

	$motr_st_util_dir/m0cat $MOTR_PARAMS_2 -o $object_id -s $block_size \
                                 -c $block_count -e $dest_file'1' &
	pid1=$!
	$motr_st_util_dir/m0cat $MOTR_PARAMS_3 -o $object_id -s $block_size \
                                 -c $block_count -e $dest_file'2' &
	pid2=$!
	sleep 2

	$motr_st_util_dir/m0cp $MOTR_PARAMS -o $object_id $src_file'1' \
                                -s $block_size -c $block_count -e -u || {
		motr_error_handling $? "Failed to update the object"
	}

	wait $pid1 $pid2

	$motr_st_util_dir/m0unlink $MOTR_PARAMS -o $object_id -e || {
		motr_error_handling $? "Failed to delete object"
	}

	diff $src_file $dest_file'1' || {
		rc=$?
		echo -n " Files differ, "
		diff $src_file'1' $dest_file'1' && {
			rc2=$?
			echo "writer updated the file before it could be read"
			motr_error_handling $rc2 ""
		}
		echo "object got corrupted"
		motr_error_handling $rc ""
	}

	diff $src_file $dest_file'2' || {
		rc=$?
		echo -n "Files differ, "
		diff $src_file'1' $dest_file'2' && {
			rc2=$?
			echo "writer updated the file before it could be read"
			motr_error_handling $rc2 ""
		}
		echo "object got corrupted"
		motr_error_handling $rc ""
	}

	rm -f $dest_file'1' $dest_file'2'
#############################################################################
	echo "Test exclusivity among Writers"
	$motr_st_util_dir/m0cp $MOTR_PARAMS -o $object_id $src_file \
                                -s $block_size -c $block_count -e &
	pid1=$!
	sleep 2

	$motr_st_util_dir/m0cp $MOTR_PARAMS_2 -o $object_id $src_file'1' \
                                -s $block_size -c $block_count -e -u &
	pid2=$!
	sleep 2

	$motr_st_util_dir/m0cat $MOTR_PARAMS_3 -o $object_id -s $block_size \
                                 -c $block_count -e $dest_file || {
		motr_error_handling $? "Failed to read object"
	}
	wait $pid1 $pid2

	$motr_st_util_dir/m0unlink $MOTR_PARAMS -o $object_id -e || {
		motr_error_handling $? "Failed to delete object"
	}

	diff $src_file'1' $dest_file || {
		rc=$?
		echo -n "Files differ, "
		diff $src_file $dest_file && {
			rc2=$?
			echo "read the object before it could be updated"
			motr_error_handling $rc2 ""
		}
		echo "object got corrupted"
		motr_error_handling $rc ""
	}

	rm -f $dest_file
#############################################################################
	echo "Launch multiple Writer and Reader threads."
	echo "To check the data read by reader threads is fresh."
	$motr_st_util_dir/m0cc_cp_cat $MOTR_PARAMS -W $writer_numb \
					-R $reader_numb -o $object_id \
					-s $block_size -c $block_count \
					$SRC_FILES $DEST_FILES || {
		motr_error_handling $? "Failed concurrent read write"
	}

	$motr_st_util_dir/m0unlink $MOTR_PARAMS -o $object_id -e || {
		motr_error_handling $? "Failed to delete object"
	}

	echo "Checking file versions for stale data."
	check_file_version || {
		motr_error_handling $? "Stale data read"
	}

	clean &>>$MOTR_TEST_LOGFILE
	motr_service_stop
}

echo "Motr RM lock CC_IO Test ... "
main
report_and_exit motr_rm_lock_cc_io_st $?
