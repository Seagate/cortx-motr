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


SANDBOX_DIR=/var/motr
MOTR_TEST_DIR=$SANDBOX_DIR
MOTR_TEST_LOGFILE=$SANDBOX_DIR/motr_`date +"%Y-%m-%d_%T"`.log
MOTR_TRACE_DIR=$SANDBOX_DIR/motr

clean()
{
        multiple_pools=$1
	local i=0
	for ((i=0; i < ${#IOSEP[*]}; i++)) ; do
		# Removes the stob files created in stob domain since
		# there is no support for m0_stob_delete() and after
		# unmounting the client file system, from next mount,
		# fids are generated from same baseline which results
		# in failure of cob_create fops.
		local ios_index=`expr $i + 1`
		rm -rf $MOTR_TEST_DIR/d$ios_index/stobs/o/*
	done

        if [ ! -z "$multiple_pools" ] && [ $multiple_pools == 1 ]; then
		local ios_index=`expr $i + 1`
		rm -rf $MOTR_TEST_DIR/d$ios_index/stobs/o/*
        fi
}

error_handling()
{
	rc=$1
	msg=$2
	clean 0 &>>$MOTR_TEST_LOGFILE
	motr_service_stop
	echo $msg
	echo "Test log file available at $MOTR_TEST_LOGFILE"
	echo "Motr trace files are available at: $MOTR_TRACE_DIR"
	exit $1
}

test_with_N_K()
{
	src_file="$MOTR_TEST_DIR/src_file"
	src_file_extra="$MOTR_TEST_DIR/src_file_extra"
	dest_file="$MOTR_TEST_DIR/dest_file"
	object_id1=0x7300000000000001:0x32
	object_id2=0x7300000000000001:0x33
	object_id3=0x7300000000000001:0x34
	object_id4=1048577
	block_size=4096
	block_count=5120
	obj_count=5
	trunc_len=2560
	trunc_count=17
	read_verify="false"
	blks_per_io=100
	MOTR_PARAMS="-l $MOTR_LOCAL_EP -H $MOTR_HA_EP -p $MOTR_PROF_OPT \
                       -P $MOTR_PROC_FID"
	MOTR_PARAMS_V="-l $MOTR_LOCAL_EP -H $MOTR_HA_EP -p $MOTR_PROF_OPT \
                         -P $MOTR_PROC_FID"
	if [[ $read_verify == "true" ]]; then
		MOTR_PARAMS_V+=" -r"
	fi
	rm -f $src_file
	local source_abcd=$MOTR_TEST_DIR/"abcd"
	dd if=$source_abcd bs=$block_size count=$block_count of=$src_file \
           2> $MOTR_TEST_LOGFILE || {
		error_handling $? "Failed to create a source file"
	}
	dd if=$source_abcd bs=$block_size \
	   count=$(($block_count + $trunc_count)) of=$src_file_extra \
	   2> $MOTR_TEST_LOGFILE || {
		error_handling $? "Failed to create a source file"
	}
	echo "count: $count"

	N=$1
	K=$2
	S=$3
	P=$4
	stride=32

	motr_service_start $N $K $S $P $stride
	dix_init

	# Test m0client utility
	/usr/bin/expect <<EOF
	set timeout 20
	spawn $motr_st_util_dir/m0client $MOTR_PARAMS_V > $SANDBOX_DIR/m0client.log
	expect "m0client >>"
	send -- "touch $object_id3\r"
	expect "m0client >>"
	send -- "write $object_id2 $src_file $block_size $block_count $blks_per_io\r"
	expect "m0client >>"
	send -- "read $object_id2 $dest_file $block_size $block_count $blks_per_io\r"
	expect "m0client >>"
	send -- "delete $object_id3\r"
	expect "m0client >>"
	send -- "delete $object_id2\r"
	expect "m0client >>"
	send -- "quit\r"
EOF
	echo "m0client test is Successful"
	rm -f $dest_file

	echo "m0touch and m0unlink"
	$motr_st_util_dir/m0touch $MOTR_PARAMS -o $object_id1 -L 9|| {
		error_handling $? "Failed to create a object"
	}
	$motr_st_util_dir/m0unlink $MOTR_PARAMS -o $object_id1 || {
		error_handling $? "Failed to delete object"
	}
	echo "m0touch and m0unlink successful"

	$motr_st_util_dir/m0touch $MOTR_PARAMS -o $object_id1 -L 9 || {
		error_handling $? "Failed to create a object"
	}

	$motr_st_util_dir/m0unlink $MOTR_PARAMS -o $object_id1 || {
		error_handling $? "Failed to delete object"
	}
	echo "m0touch and m0unlink successful"

	$motr_st_util_dir/m0cp $MOTR_PARAMS_V -o $object_id1 $src_file \
                                 -s $block_size -c $block_count -L 9 \
                                 -b $blks_per_io || {
		error_handling $? "Failed to copy object"
	}
	$motr_st_util_dir/m0cat $MOTR_PARAMS_V -o $object_id1 \
				  -s $block_size -c $block_count -L 9 -b $blks_per_io \
				  $dest_file || {
		error_handling $? "Failed to read object"
	}
	$motr_st_util_dir/m0unlink $MOTR_PARAMS -o $object_id1 || {
		error_handling $? "Failed to delete object"
	}
	diff $src_file $dest_file || {
		rc=$?
		error_handling $rc "Files are different"
	}
	echo "motr r/w test with m0cp and m0cat is successful"
	rm -f $dest_file

	# Test m0cp_mt
	echo "m0cp_mt test"
	$motr_st_util_dir/m0cp_mt $MOTR_PARAMS_V -o $object_id4 \
				    -n $obj_count $src_file -s $block_size \
				    -c $block_count -L 9 -b $blks_per_io || {
		error_handling $? "Failed to copy object"
	}
	for i in $(seq 0 $(($obj_count - 1)))
	do
		object_id=$(($object_id4 + $i));
		$motr_st_util_dir/m0cat $MOTR_PARAMS_V -o $object_id \
					  -s $block_size -c $block_count -L 9 \
                                          -b $blks_per_io $dest_file || {
			error_handling $? "Failed to read object"
		}
		diff $src_file $dest_file || {
			rc=$?
			error_handling $rc "Files are different"
		}
		rm -f $dest_file
	done
	$motr_st_util_dir/m0unlink $MOTR_PARAMS -o $object_id4 \
				     -n $obj_count || {
		error_handling $? "Failed to delete object"
	}
	echo "m0cp_mt is successful"

	# Test truncate/punch utility
	$motr_st_util_dir/m0cp $MOTR_PARAMS_V -o $object_id1 $src_file \
                                 -s $block_size -c $block_count -L 9 \
                                 -b $blks_per_io || {
		error_handling $? "Failed to copy object"
	}
	$motr_st_util_dir/m0trunc $MOTR_PARAMS -o $object_id1 \
				    -c $trunc_count -t $trunc_len \
				    -s $block_size -L 9 -b $blks_per_io || {
		error_handling $? "Failed to truncate object"
	}
	$motr_st_util_dir/m0cat $MOTR_PARAMS_V -o $object_id1 \
				  -s $block_size -c $block_count -L 9 \
                                  -b $blks_per_io \
				  $dest_file-full || {
		error_handling $? "Failed to read object"
	}
	$motr_st_util_dir/m0unlink $MOTR_PARAMS -o $object_id1 || {
		error_handling $? "Failed to delete object"
	}
	cp $src_file $src_file-punch
	fallocate -p -o $(($trunc_count * $block_size)) \
		  -l $(($trunc_len * $block_size)) -n $src_file-punch
	diff -q $src_file-punch $dest_file-full || {
		rc=$?
		error_handling $rc "Punched Files are different"
	}
	echo "m0trunc: Punching hole is successful"
	rm -f $src_file-punch $dest_file-full

	# Truncate file to zero
	$motr_st_util_dir/m0cp $MOTR_PARAMS_V -o $object_id1 $src_file \
                                 -s $block_size -c $block_count -L 9 \
                                 -b $blks_per_io || {
		error_handling $? "Failed to copy object"
	}
	$motr_st_util_dir/m0trunc $MOTR_PARAMS -o $object_id1 -c 0 \
                                   -t $block_count -s $block_size -L 9 \
                                   -b $blks_per_io || {
		error_handling $? "Failed to truncate object"
	}
	$motr_st_util_dir/m0cat $MOTR_PARAMS_V -o $object_id1 \
				  -s $block_size -c $block_count -L 9 \
                                  -b $blks_per_io \
				  $dest_file || {
		error_handling $? "Failed to read from truncated object"
	}
	$motr_st_util_dir/m0unlink $MOTR_PARAMS -o $object_id1 || {
		error_handling $? "Failed to delete object"
	}
	cp $src_file $src_file-trunc
	fallocate -p -o 0 -l $(($block_count * $block_size)) -n $src_file-trunc
	diff -q $src_file-trunc $dest_file || {
		rc=$?
		error_handling $rc "Truncated Files are different"
	}
	echo "m0trunc: Truncate file to zero is successful"
	rm -f $src_file-trunc $dest_file

	# Truncate range beyond EOF
	$motr_st_util_dir/m0cp $MOTR_PARAMS_V -o $object_id1 $src_file \
                                 -s $block_size -c $block_count -L 9 \
                                 -b $blks_per_io || {
		error_handling $? "Failed to copy object"
	}
	$motr_st_util_dir/m0trunc $MOTR_PARAMS -o $object_id1 \
				    -c $trunc_count -t $block_count \
				    -s $block_size -L 9 -b $blks_per_io || {
		error_handling $? "Failed to truncate object"
	}
	$motr_st_util_dir/m0cat $MOTR_PARAMS_V -o $object_id1 \
				  -s $block_size \
				  -c $(($block_count + $trunc_count)) -L 9 \
                                  -b $blks_per_io \
				  $dest_file || {
		error_handling $? "Failed to read from truncated object"
	}
	$motr_st_util_dir/m0unlink $MOTR_PARAMS -o $object_id1 || {
		error_handling $? "Failed to delete object"
	}
	fallocate -p -o $(($trunc_count * $block_size)) \
		  -l $(($block_count  * $block_size)) -n $src_file_extra
	diff -q $src_file_extra $dest_file || {
		rc=$?
		error_handling $rc "Truncat Files beyond EOF are different"
	}
	echo "m0trunc: Truncate range beyond EOF is successful"
	rm -f $src_file_extra $dest_file

	# Truncate a zero size object
	$motr_st_util_dir/m0touch $MOTR_PARAMS -o $object_id1 -L 9|| {
		error_handling $? "Failed to create a object"
	}
	$motr_st_util_dir/m0trunc $MOTR_PARAMS -o $object_id1 -c 0 \
				    -t $block_count -s $block_size -L 9 \
                                    -b $blks_per_io || {
		error_handling $? "Failed to truncate object"
	}
	$motr_st_util_dir/m0unlink $MOTR_PARAMS -o $object_id1 || {
		error_handling $? "Failed to delete object"
	}
	echo "m0trunc: Truncate zero size object successful"

	rm -f $src_file
	clean &>>$MOTR_TEST_LOGFILE
	motr_service_stop
	return 0
}

main()
{
	sandbox_init

	rc=0
	NODE_UUID=`uuidgen`
	motr_dgmode_sandbox="$MOTR_TEST_DIR/sandbox"

	rm -f $source_abcd
	st_dir=$M0_SRC_DIR/m0t1fs/linux_kernel/st
	prog_file_pattern="$st_dir/m0t1fs_io_file_pattern"

	local source_abcd="abcd"
	echo "Creating data file $source_abcd"
	$prog_file_pattern $source_abcd || {
		echo "Failed: $prog_file_pattern"
		error_handling $? "Failed to copy object"
	}
	mkdir $MOTR_TRACE_DIR

	N=1
	K=0
	S=0
	P=8
	test_with_N_K $N $K $S $P
	if [ $rc -ne "0" ]
	then
		echo "Motr util test with N=$N K=$K failed"
		return $rc
	fi
	echo "Motr util test with N=$N K=$K is successful"

	N=1
	K=2
	S=2
	P=8
	test_with_N_K $N $K $S $P
	rc=$?
	if [ $rc -ne "0" ]
	then
		echo "Motr util test with N=$N K=$K failed"
		return $rc
	fi
	echo "Motr util test with N=$N K=$K is successful"

	N=4
	K=2
	S=2
	P=8
	test_with_N_K $N $K $S $P
	rc=$?
	echo $rc
	if [ $rc -ne "0" ]
	then
		echo "Motr util test with N=$N K=$K failed"
		return $rc
	fi
	echo "Motr util test with N=$N K=$K is successful"

	rm -f $source_abcd
	sandbox_fini
	return $rc
}

echo "Motr Utils Test ... "
main
report_and_exit motr-utils-st $?
