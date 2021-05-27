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

unit2id_map=(
	[    4]=1
	[    8]=2
	[   16]=3
	[   32]=4
	[   64]=5
	[  128]=6
	[  256]=7
	[  512]=8
	[ 1024]=9
	[ 2048]=10
	[ 4096]=11
	[ 8192]=12
	[16384]=13
	[32768]=14
)

touch_file()
{
	local file=$1
	local unitsz_id=${unit2id_map[$2]}

	if [ x$unitsz_id = x ]; then
		echo "Invalid unit_size: $2"
		return 1
	fi

	run "touch $file" &&
	run "setfattr -n lid -v $unitsz_id $file"

	return $?
}

mount_m0t1fs()
{
	if [ $# -ne 1 -a $# -ne 2 ]
	then
		echo "Usage: mount_m0t1fs <mount_dir> [mount_options]"
		return 1
	fi

	local m0t1fs_mount_dir=$1
	local mountop=$2

	# Create mount directory
	sudo mkdir -p $m0t1fs_mount_dir || {
		echo "Failed to create mount directory."
		return 1
	}

	lsmod | grep m0tr || {
		echo "Failed to	mount m0t1fs file system. (m0tr not loaded)"
		return 1
	}

	echo "Mounting file system..."

	local cmd="sudo mount -t m0t1fs \
	    -o pfid='$M0T1FS_PROC_ID',profile='$PROF_OPT',ha='$lnet_nid:$HA_EP',$mountop \
	    none $m0t1fs_mount_dir"
	echo $cmd
	eval $cmd || {
		echo "Failed to mount m0t1fs file system."
		return 1
	}

	echo "mount | grep m0t1fs"
	mount | grep m0t1fs
	local retcode=$?
	if [ $retcode = 0 ]; then
	    time df $m0t1fs_mount_dir
	fi
	return $retcode
}

unmount_and_clean()
{
	m0t1fs_mount_dir=$MOTR_M0T1FS_MOUNT_DIR
        multiple_pools=$1
	echo "Unmounting file system ..."
	umount $m0t1fs_mount_dir &>/dev/null

	sleep 2

	echo "mount | grep m0t1fs"
	mount | grep m0t1fs

	echo "Cleaning up mount test directory..."
	rm -rf $m0t1fs_mount_dir &>/dev/null

	local i=0
	for ((i=0; i < ${#IOSEP[*]}; i++)) ; do
		# Removes the stob files created in stob domain since
		# there is no support for m0_stob_delete() and after
		# unmounting the client file system, from next mount,
		# fids are generated from same baseline which results
		# in failure of cob_create fops.
		local ios_index=`expr $i + 1`
		rm -rf $MOTR_M0T1FS_TEST_DIR/d$ios_index/stobs/o/*
	done

        if [ ! -z "$multiple_pools" ] && [ $multiple_pools == 1 ]; then
		local ios_index=`expr $i + 1`
		rm -rf $MOTR_M0T1FS_TEST_DIR/d$ios_index/stobs/o/*
        fi
}

unmount_m0t1fs()

{	if [ $# -ne 1 ]
	then
		echo "Usage: unmount_m0t1fs <mount_dir>"
		return 1
	fi

	local m0t1fs_mount_dir=$1
	echo "Unmounting file system ..."
	umount $m0t1fs_mount_dir &>/dev/null

	sleep 2

	echo "Cleaning up test directory..."
	rm -rf $m0t1fs_mount_dir &>/dev/null
}


bulkio_test()
{
	local fsname1="0:100125"
	local fsname2="0:600456"
	local fsname3="0:a0089b"
	local local_input=$MOTR_M0T1FS_TEST_DIR/$fsname1
	local local_output=$MOTR_M0T1FS_TEST_DIR/$fsname2
	local m0t1fs_mount_dir=$MOTR_M0T1FS_MOUNT_DIR
	local m0t1fs_file=$m0t1fs_mount_dir/1:23456
	local unit_size=$1
	local io_size=$2
	local io_counts=$3
	local mode=$4
	local mountopt=$5

	mount_m0t1fs $m0t1fs_mount_dir "$mode,$mountopt" || return 1

	echo "Creating local input file of I/O size ..."
	run "dd if=/dev/urandom of=$local_input bs=$io_size count=$io_counts"
	if [ $? -ne 0 ]; then
		echo "Failed to create local input file."
		unmount_and_clean
		return 1
	fi

	echo "Writing data to m0t1fs file ..."
	touch_file $m0t1fs_file $unit_size &&
	run "dd if=$local_input of=$m0t1fs_file bs=$io_size count=$io_counts"
	if [ $? -ne 0 ]; then
		echo "Failed to write data on m0t1fs file."
		unmount_and_clean
		return 1
	fi

	echo -n "Reading data from m0t1fs file "
	if [ $io_counts -gt 1 ]; then
		trigger=`expr \( ${trigger:-0} + 1 \) % 2`
		# run 50% of such tests with different io_size
		if [ $trigger -eq 0 ]; then
			echo -n "with different io_size "
			io_suffix=${io_size//[^KM]}
			io_size=`expr ${io_size%[KM]} '*' $io_counts`$io_suffix
			io_counts=1
		fi
	fi
	echo "..."
	run "dd if=$m0t1fs_file of=$local_output bs=$io_size count=$io_counts"
	if [ $? -ne 0 ]; then
		echo "Failed to read data from m0t1fs file."
		unmount_and_clean
		return 1
	fi

	echo "Comparing data written and data read from m0t1fs file ..."
	if ! cmp $local_input $local_output
	then
		echo -n "Failed: data written and data read from m0t1fs file "
		echo    "are not same."
		unmount_and_clean
		return 1
	fi

	echo "Successfully tested $io_counts I/O(s) of size $io_size."

	run "rm -f $m0t1fs_file"

	unmount_and_clean

	return 0
}

show_write_speed()
{
	cat $MOTR_TEST_LOGFILE | grep copied | tail -2 | head -1 | \
		awk -F, '{print $3}'
}

# This test runs for various unit size values and writes
# in multiple of full stripes (sum of data units in parity group).
io_combinations()
{
	echo "Test: io_combinations: (N,K,P) = ($2,$3,$1) $4 ..."

	pool_width=$1
	data_units=$2
	parity_units=$3
	mode=$4

	p=`expr $data_units + 2 '*' $parity_units`
	if [ $p -gt $pool_width ]
	then
		echo "Error: pool_width should be >= data_units + 2 * parity_units."
		return 1
	fi

	# unit size in K
	for unit_size in 4 8 32 128 512 2048 4096
	do
	    stripe_size=`expr $unit_size '*' $data_units`

	    for io_size in 1 2 4
	    do
		io_size=`expr $io_size '*' $stripe_size`
		io_size=${io_size}K

		for io_count in 1 2
		do
			echo -n "Test: I/O for stripe = ${stripe_size}K," \
			     "bs = $io_size, count = $io_count... "
			bulkio_test $unit_size $io_size $io_count $mode "" &>> $MOTR_TEST_LOGFILE
			if [ $? -ne "0" ]
			then
				return 1
			fi
			show_write_speed
		done
	    done

	done
	return 0
}

file_creation_test()
{
	local nr_files=$1
	local mode=$2
	local SFILE=/tmp/source.txt
	local DFILE=/tmp/dest.txt
	local START_FID=15     # Added to skip root and other system fids.
	local BS=$((4 * $NR_DATA * 2))K

	for i in {a..z} {A..Z}; do
		for c in `seq 1 4095`;
			do echo -n $i ;
		done;
		echo;
	done > $SFILE

	NR_FILES=$[ $nr_files - $START_FID ]
	NR_FILES=$[ $NR_FILES * $NR_FILES ]

	mount_m0t1fs $MOTR_M0T1FS_MOUNT_DIR $mode || return 1
	echo "Test: Creating $NR_FILES files on m0t1fs ..."
	for ((i=$START_FID; i<$nr_files; ++i)); do
		for ((j=$START_FID; j<$nr_files; ++j)); do
			touch $MOTR_M0T1FS_MOUNT_DIR/$j:$i || break
			run "dd if=$SFILE of=$MOTR_M0T1FS_MOUNT_DIR/$j:$i bs=$BS" || break
			dd if=$MOTR_M0T1FS_MOUNT_DIR/$j:$i of=$DFILE bs=$BS 2>/dev/null || break
			diff -C 0 $SFILE $DFILE || {
				echo "file content differ!!!!!!!!!!!!!!!!! at $j:$i file. "
				break;
			}
		done
	done
	echo -n "Test: file creation: "
	if (( $i == $nr_files && $j == $nr_files )); then
		echo "success."
	else
		echo "failed."
		return 1
	fi

	echo "Test: removing half of the files on m0t1fs..."
	for ((i=$START_FID; i<$nr_files; i+=2)); do
		for ((j=$START_FID; j<$nr_files; j+=2)); do
			run "rm -vf $MOTR_M0T1FS_MOUNT_DIR/$j:$i" || break
		done
	done
	echo -n "Test: file removal: "
	if (( $i >= $nr_files && $j >= $nr_files )); then
		echo "success."
	else
		echo "failed."
		return 1
	fi

	echo "Test: Creating new $NR_FILES files on m0t1fs..."
	for ((i=$START_FID; i<$nr_files; ++i)); do
		for ((j=$START_FID; j<$nr_files; ++j)); do
			touch $MOTR_M0T1FS_MOUNT_DIR/1$j:$i || break
			run "dd if=$SFILE of=$MOTR_M0T1FS_MOUNT_DIR/1$j:$i bs=$BS" || break
			dd if=$MOTR_M0T1FS_MOUNT_DIR/1$j:$i of=$DFILE bs=$BS 2>/dev/null || break
			diff -C 0 $SFILE $DFILE || {
				echo "file content differ!!!!!!!!!!!!!!!!! at $j:$i file. "
				break;
			}
		done
	done
	echo -n "Test: file creation: "
	if (( $i == $nr_files && $j == $nr_files )); then
		echo "success."
	else
		echo "failed."
		return 1
	fi

	echo "Test: removing all the files on m0t1fs..."
	for ((i=$START_FID; i<$nr_files; ++i)); do
		for ((j=$START_FID; j<$nr_files; ++j)); do
			run "rm -vf $MOTR_M0T1FS_MOUNT_DIR/$j:$i" || break
			run "rm -vf $MOTR_M0T1FS_MOUNT_DIR/1$j:$i" || break
		done
	done
	unmount_and_clean
	echo -n "Test: file removal: "
	if (( $i == $nr_files && $j == $nr_files )); then
		echo "success."
	else
		echo "failed."
		return 1
	fi

	if [ "$mode" == "oostore" ]; then
		return 0
	fi

	mount_m0t1fs $MOTR_M0T1FS_MOUNT_DIR "verify" || {
                return 1
        }
	for ((i=0; i<$nr_files; ++i)); do
		#arbitrary file size. "1021" is a prime close to 1024.
		touch $MOTR_M0T1FS_MOUNT_DIR/file$i || break
		dd if=$SFILE of=/tmp/src bs=1021 count=`expr $i + 1` >/dev/null 2>&1 || break
		cp -v /tmp/src $MOTR_M0T1FS_MOUNT_DIR/file$i      || break
		cp -v $MOTR_M0T1FS_MOUNT_DIR/file$i /tmp/dest     || break
		diff -C 0 /tmp/src /tmp/dest || {
			echo "file content differ at file$i file. "
			break;
		}
	done

	if [ $i -eq $nr_files ]; then
		echo "success."
	else
		echo "failed."
		return 1
	fi

	for ((i=0; i<$nr_files; ++i)); do
		rm -vf $MOTR_M0T1FS_MOUNT_DIR/file$i      || break
	done
	if [ $i -eq $nr_files ]; then
		echo "success."
	else
		echo "failed."
		return 1
	fi
	unmount_and_clean
	return 0
}

multi_client_test()
{
	local mount_dir_1=${MOTR_M0T1FS_MOUNT_DIR}aa
	local mount_dir_2=${MOTR_M0T1FS_MOUNT_DIR}bb
	local mount_dir_3=${MOTR_M0T1FS_MOUNT_DIR}cc

	local rc=0

	mount_m0t1fs ${mount_dir_1} "fid_start=65536" ||
		return 1
	mount | grep m0t1fs
	mount_m0t1fs ${mount_dir_2} "fid_start=66536" || {
		unmount_m0t1fs ${mount_dir_1}
		return 1
	}
	mount | grep m0t1fs
	mount_m0t1fs ${mount_dir_3} "fid_start=67536" || {
		unmount_m0t1fs ${mount_dir_1}
		unmount_m0t1fs ${mount_dir_2}
		return 1
	}
	echo "Three clients mounted:"
	mount | grep m0t1fs
	cp -av /bin/ls ${mount_dir_1}/obj1 || rc=1
	cp -av /bin/ls ${mount_dir_2}/obj2 || rc=1
	cp -av /bin/ls ${mount_dir_3}/obj3 || rc=1
	ls -liR ${mount_dir_1} || rc=1
	ls -liR ${mount_dir_2} || rc=1
	ls -liR ${mount_dir_3} || rc=1

	diff /bin/ls ${mount_dir_1}/obj1 || rc=1
	diff /bin/ls ${mount_dir_1}/obj2 || rc=1
	diff /bin/ls ${mount_dir_1}/obj3 || rc=1

	diff /bin/ls ${mount_dir_1}/obj1 || rc=1
	diff /bin/ls ${mount_dir_2}/obj2 || rc=1
	diff /bin/ls ${mount_dir_3}/obj3 || rc=1

	unmount_m0t1fs ${mount_dir_1}
	unmount_m0t1fs ${mount_dir_2}
	unmount_m0t1fs ${mount_dir_3}
	echo "First round done."
	mount_m0t1fs ${mount_dir_1} "fid_start=65536" || {
		return 1
	}
	mount_m0t1fs ${mount_dir_2} "fid_start=66536" || {
		unmount_m0t1fs ${mount_dir_1}
		return 1
	}
	mount_m0t1fs ${mount_dir_3} "fid_start=67536" || {
		unmount_m0t1fs ${mount_dir_1}
		unmount_m0t1fs ${mount_dir_2}
		return 1
	}
	echo "Three clients mounted:"
	mount | grep m0t1fs
	ls -liR ${mount_dir_1} || rc=1
	ls -liR ${mount_dir_2} || rc=1
	ls -liR ${mount_dir_3} || rc=1

	diff /bin/ls ${mount_dir_1}/obj1 || rc=1
	diff /bin/ls ${mount_dir_2}/obj2 || rc=1
	diff /bin/ls ${mount_dir_3}/obj3 || rc=1

	# Completely repeat the first round of the test.
	diff /bin/ls ${mount_dir_1}/obj1 || rc=1
	diff /bin/ls ${mount_dir_1}/obj2 || rc=1
	diff /bin/ls ${mount_dir_1}/obj3 || rc=1

	unmount_m0t1fs ${mount_dir_1}
	unmount_m0t1fs ${mount_dir_2}
	unmount_m0t1fs ${mount_dir_3}
	echo "Second round done"
	mount | grep m0t1fs
	return $rc
}


rmw_test()
{
	local mode=$1
	local mountopt=$2

	echo "Test: RMW (with mountopt=\"${mountopt}\") ..."
	for unit_size in 4 8 16 32
	do
		for io in 1 2 3 4 5 15 16 17 32
		do
			io_size=${io}K
			for io_count in 1 2
			do
				echo -n "IORMW Test: I/O for unit ="\
				     "${unit_size}K, bs = $io_size, count = $io_count ... "
				bulkio_test $unit_size $io_size $io_count $mode $mountopt &>> $MOTR_TEST_LOGFILE || return 1
				show_write_speed
			done
		done
	done

	echo "Test: IORMW: Success."

	return 0
}

###########################################
# This test is only valid in OOSTORE mode.
# Motr.cmd hash file by filename.
# In oostore mode directory support is not available,
# so creation of the file inside a sub directory test is removed.
###########################################
obf_test()
{
	local rc=0

	echo "Test: obf..."
	mount_m0t1fs $MOTR_M0T1FS_MOUNT_DIR "oostore" || {
		return 1
	}
	touch $MOTR_M0T1FS_MOUNT_DIR/0:30000 || rc=1
	stat $MOTR_M0T1FS_MOUNT_DIR/0:30000 || rc=1
	ls -la $MOTR_M0T1FS_MOUNT_DIR/0:30000 || rc=1
	rm $MOTR_M0T1FS_MOUNT_DIR/0:30000 || rc=1
	unmount_m0t1fs $MOTR_M0T1FS_MOUNT_DIR
	if [ $rc -eq 0 ]; then
		echo "Success: Open-by-fid test."
	else
		echo "Failure: Open-by-fid test."
	fi
	return $rc
}

m0t1fs_crud()
{
	local rc=0

	local fsname1=$1
	local fsname2=$2
	local fsname3=$3
	local mode=$4
	touch $MOTR_M0T1FS_MOUNT_DIR/$fsname1 || rc=1
	touch $MOTR_M0T1FS_MOUNT_DIR/$fsname2 || rc=1
	touch $MOTR_M0T1FS_MOUNT_DIR/$fsname3 || rc=1
	stat  $MOTR_M0T1FS_MOUNT_DIR/$fsname1 || rc=1
	stat  $MOTR_M0T1FS_MOUNT_DIR/$fsname2 || rc=1
	stat  $MOTR_M0T1FS_MOUNT_DIR/$fsname3 || rc=1
	chmod 567 $MOTR_M0T1FS_MOUNT_DIR/$fsname1 || rc=1
	chmod 123 $MOTR_M0T1FS_MOUNT_DIR/$fsname2 || rc=1
	chmod 345 $MOTR_M0T1FS_MOUNT_DIR/$fsname3 || rc=1
	stat  $MOTR_M0T1FS_MOUNT_DIR/$fsname1 -c "%n: %a %s" || rc=1
	stat  $MOTR_M0T1FS_MOUNT_DIR/$fsname2 -c "%n: %a %s" || rc=1
	stat  $MOTR_M0T1FS_MOUNT_DIR/$fsname3 -c "%n: %a %s" || rc=1
	dd if=/dev/zero of=$MOTR_M0T1FS_MOUNT_DIR/$fsname1 bs=4K   count=1 || rc=1
	dd if=/dev/zero of=$MOTR_M0T1FS_MOUNT_DIR/$fsname2 bs=128K count=1 || rc=1
	touch_file $MOTR_M0T1FS_MOUNT_DIR/$fsname3 64 &&
	dd if=/dev/zero of=$MOTR_M0T1FS_MOUNT_DIR/$fsname3 bs=1M   count=1 || rc=1
	sync
	stat  $MOTR_M0T1FS_MOUNT_DIR/$fsname1 -c "%n: %a %s" || rc=1
	stat  $MOTR_M0T1FS_MOUNT_DIR/$fsname2 -c "%n: %a %s" || rc=1
	stat  $MOTR_M0T1FS_MOUNT_DIR/$fsname3 -c "%n: %a %s" || rc=1
	echo 3 > /proc/sys/vm/drop_caches
	dd of=/dev/zero if=$MOTR_M0T1FS_MOUNT_DIR/$fsname1 bs=4K   count=1 || rc=1
	dd of=/dev/zero if=$MOTR_M0T1FS_MOUNT_DIR/$fsname2 bs=128K count=1 || rc=1
	dd of=/dev/zero if=$MOTR_M0T1FS_MOUNT_DIR/$fsname3 bs=1M   count=1 || rc=1
	stat  $MOTR_M0T1FS_MOUNT_DIR/$fsname1 -c "%n: %a %s" || rc=1
	stat  $MOTR_M0T1FS_MOUNT_DIR/$fsname2 -c "%n: %a %s" || rc=1
	stat  $MOTR_M0T1FS_MOUNT_DIR/$fsname3 -c "%n: %a %s" || rc=1
	dd if=/dev/zero of=$MOTR_M0T1FS_MOUNT_DIR/$fsname1 bs=4K   count=1 || rc=1
	dd if=/dev/zero of=$MOTR_M0T1FS_MOUNT_DIR/$fsname2 bs=128K count=1 || rc=1
	dd if=/dev/zero of=$MOTR_M0T1FS_MOUNT_DIR/$fsname3 bs=1M   count=1 || rc=1
	stat  $MOTR_M0T1FS_MOUNT_DIR/$fsname1 -c "%n: %a %s" || rc=1
	stat  $MOTR_M0T1FS_MOUNT_DIR/$fsname2 -c "%n: %a %s" || rc=1
	stat  $MOTR_M0T1FS_MOUNT_DIR/$fsname3 -c "%n: %a %s" || rc=1
	if [ "$mode" != "oostore" ]; then
		ls -l $MOTR_M0T1FS_MOUNT_DIR || rc=1
	fi
	unmount_and_clean
	echo "Remount and perform write on already created file"
	mount_m0t1fs $MOTR_M0T1FS_MOUNT_DIR "$mode" || rc=1
	dd if=/dev/zero of=$MOTR_M0T1FS_MOUNT_DIR/$fsname1 bs=4K   count=1 || rc=1
	dd if=/dev/zero of=$MOTR_M0T1FS_MOUNT_DIR/$fsname2 bs=128K count=1 || rc=1
	dd if=/dev/zero of=$MOTR_M0T1FS_MOUNT_DIR/$fsname3 bs=1M   count=1 || rc=1
	rm -f $MOTR_M0T1FS_MOUNT_DIR/$fsname1 || rc=1
	rm -f $MOTR_M0T1FS_MOUNT_DIR/$fsname2 || rc=1
	rm -f $MOTR_M0T1FS_MOUNT_DIR/$fsname3 || rc=1
	stat  $MOTR_M0T1FS_MOUNT_DIR/$fsname1 2>/dev/null && rc=1
	stat  $MOTR_M0T1FS_MOUNT_DIR/$fsname2 2>/dev/null && rc=1
	stat  $MOTR_M0T1FS_MOUNT_DIR/$fsname3 2>/dev/null && rc=1

	# Test for CROW
	dd if=/dev/zero of=$MOTR_M0T1FS_MOUNT_DIR/$fsname1 bs=4K   count=1 || rc=1
	rm -f $MOTR_M0T1FS_MOUNT_DIR/$fsname1 || rc=1
	return $rc
}

m0t1fs_basic()
{
	local rc=0
	local fsname1="123456"
	local fsname2="890"
	local fsname3="xyz0"
	echo "Test: basic..."
	mount_m0t1fs $MOTR_M0T1FS_MOUNT_DIR || rc=1
	mount | grep m0t1fs
	m0t1fs_crud $fsname1 $fsname2 $fsname3 || rc=1
	unmount_and_clean
	return $rc
}

###############################################################
# The following readdir() test will send two readdir requests
# to mdservice 0, and then EOF is returned; So client readdir()
# switches to another mdservice, and does the same, until
# all mdservices are iterated.
# m0t1fs_large_dir mode fsname_prex
###############################################################
m0t1fs_large_dir()
{
	local rc=0
	local mode=$1
	local fsname_prex=$2
	local count=512

	echo "Test: larde_dir: mode=$1 fsname_prefix=$2..."
	mount_m0t1fs $MOTR_M0T1FS_MOUNT_DIR "$mode" || rc=1
	mount | grep m0t1fs                                 || rc=1
	for i in `seq 1 $count`; do
		touch $MOTR_M0T1FS_MOUNT_DIR/$fsname_prex$i || rc=1
		stat  $MOTR_M0T1FS_MOUNT_DIR/$fsname_prex$i -c "%n: %a %s" || rc=1
	done

	local dirs=`/bin/ls $MOTR_M0T1FS_MOUNT_DIR -U`
	local dirs_count=`echo $dirs | wc -w`
	echo "readdir count: result $dirs_count, expected $count"
	if [ ! $dirs_count -eq $count ] ; then
		rc=1
	fi
	for i in `seq 1 $count`; do
		local match=`echo $dirs | grep -c "\<$fsname_prex$i\>"`
		if [ ! $match -eq 1 ] ; then
			echo "match $fsname_prex$i failed: $match"
			rc=1
		else
			rm -v $MOTR_M0T1FS_MOUNT_DIR/$fsname_prex$i || rc=1
		fi
	done

	unmount_and_clean
	return $rc
}

m0t1fs_oostore_mode()
{
	local rc=0
	local fsname1="0:100125"
	local fsname2="0:600456"
	local fsname3="0:a0089b"
	local mode="oostore"

	echo "Test: oostore_mode..."
	mount_m0t1fs $MOTR_M0T1FS_MOUNT_DIR $mode || rc=1
	mount | grep m0t1fs                              || rc=1
	m0t1fs_crud $fsname1 $fsname2 $fsname3 $mode || rc=1
	touch $MOTR_M0T1FS_MOUNT_DIR/123456 2>/dev/null && rc=1
	stat  $MOTR_M0T1FS_MOUNT_DIR/123456 2>/dev/null && rc=1
	touch $MOTR_M0T1FS_MOUNT_DIR/abcdef 2>/dev/null && rc=1
	stat  $MOTR_M0T1FS_MOUNT_DIR/abcdef 2>/dev/null && rc=1
	unmount_and_clean

	return $rc
}

m0t1fs_oostore_mode_basic()
{
	local rc=0
	local fsname1="0:100125"
	local fsname2="0:100321"
	local file="1:23456"
	local file1="3:23457"
	local file2="2:23458"
	local file3="2:23459"
	local file4="3:23459"

	local SOURCE_TXT=/tmp/source.txt

	echo "Test: oostore_mode_basic..."

	for i in {a..z} {A..Z} ; do
		for c in `seq 1 4095`;
			do echo -n $i ;
		done;
		echo;
	done > $SOURCE_TXT

	mount_m0t1fs $MOTR_M0T1FS_MOUNT_DIR "oostore" || rc=1
	mount | grep m0t1fs                                 || rc=1
	cp -v $SOURCE_TXT $MOTR_M0T1FS_MOUNT_DIR/$fsname1   || rc=1
	cat $MOTR_M0T1FS_MOUNT_DIR/$fsname1 > /tmp/$fsname1 || rc=1
	# sleep two seconds, so the {a,c,m}time are different
	sleep 2
	cp -v $SOURCE_TXT $MOTR_M0T1FS_MOUNT_DIR/$fsname2   || rc=1
	cat $MOTR_M0T1FS_MOUNT_DIR/$fsname2 > /tmp/$fsname2 || rc=1
	diff $SOURCE_TXT /tmp/$fsname1                      || rc=1
	diff $SOURCE_TXT /tmp/$fsname2                      || rc=1

	chmod 123 $MOTR_M0T1FS_MOUNT_DIR/$fsname1           || rc=1
	chmod 456 $MOTR_M0T1FS_MOUNT_DIR/$fsname2           || rc=1

	cat $MOTR_M0T1FS_MOUNT_DIR/$fsname1 > /dev/null     || rc=1
	cat $MOTR_M0T1FS_MOUNT_DIR/$fsname2 > /dev/null     || rc=1

	stat $MOTR_M0T1FS_MOUNT_DIR/$fsname1                || rc=1
	stat $MOTR_M0T1FS_MOUNT_DIR/$fsname2                || rc=1

	rm -f $MOTR_M0T1FS_MOUNT_DIR/$fsname1               || rc=1
	rm -f $MOTR_M0T1FS_MOUNT_DIR/$fsname2               || rc=1

	# Recreate and remove the file
	touch $MOTR_M0T1FS_MOUNT_DIR/$fsname1               || rc=1
	rm -f $MOTR_M0T1FS_MOUNT_DIR/$fsname1               || rc=1

	cp -v $SOURCE_TXT $MOTR_M0T1FS_MOUNT_DIR/$file                   || rc=1
	cp -v $MOTR_M0T1FS_MOUNT_DIR/$file $MOTR_M0T1FS_MOUNT_DIR/$file1 || rc=1
	cp -v $MOTR_M0T1FS_MOUNT_DIR/$file $MOTR_M0T1FS_MOUNT_DIR/$file2 || rc=1
	cp -v $MOTR_M0T1FS_MOUNT_DIR/$file $MOTR_M0T1FS_MOUNT_DIR/$file3 || rc=1
	cp -v $MOTR_M0T1FS_MOUNT_DIR/$file $MOTR_M0T1FS_MOUNT_DIR/$file4 || rc=1
	rm -f $MOTR_M0T1FS_MOUNT_DIR/$file                               || rc=1
	rm -f $MOTR_M0T1FS_MOUNT_DIR/$file1                              || rc=1
	rm -f $MOTR_M0T1FS_MOUNT_DIR/$file2                              || rc=1
	rm -f $MOTR_M0T1FS_MOUNT_DIR/$file3                              || rc=1
	rm -f $MOTR_M0T1FS_MOUNT_DIR/$file4                              || rc=1

	unmount_and_clean

	return $rc
}

m0t1fs_parallel_io_test()
{
	local mode=$1
	BS=$((10 * 1024 * 1024))
	echo "Parallel IO test"
	mount_m0t1fs $MOTR_M0T1FS_MOUNT_DIR "$mode" || rc=1
	mount | grep m0t1fs                         || rc=1
	echo "Create files"
	for i in 0:100{0..4}000; do
		touch $MOTR_M0T1FS_MOUNT_DIR/$i     || rc=1
	done

	for i in 0:100{0..4}000; do
		setfattr -n writesize -v $BS $MOTR_M0T1FS_MOUNT_DIR/$i || rc=1
	done
	echo "Spawn parallel dd's"
	for i in `seq 1 4`
	do
		fid="0:100"$i"000"
		dd if=/dev/zero of=$MOTR_M0T1FS_MOUNT_DIR/$fid \
			bs=$BS count=10 &
		dd_pid[$i]=$!
	done
	echo "Wait for IO to complete"
	for i in `seq 1 4`
	do
		echo ${dd_pid[$i]}
		wait ${dd_pid[$i]}
	done
	for i in 0:100{0..4}000; do
		rm -f $MOTR_M0T1FS_MOUNT_DIR/$i      || rc=1
	done
	unmount_and_clean

	return $rc
}

m0t1fs_big_bs_io_test()
{
	local mode=$1
	echo "Big BS IO test"
	rc=0
	mount_m0t1fs $MOTR_M0T1FS_MOUNT_DIR "$mode" || rc=1
	mount | grep m0t1fs                         || rc=1
	echo "Create file"
	i=0:1000000
	touch $MOTR_M0T1FS_MOUNT_DIR/$i                        || rc=1
	setfattr -n writesize -v 65536 $MOTR_M0T1FS_MOUNT_DIR/$i || rc=1
	dd if=/dev/zero of=$MOTR_M0T1FS_MOUNT_DIR/$i bs=32M count=1  || rc=1
	rm -f $MOTR_M0T1FS_MOUNT_DIR/$i                                || rc=1
	echo "Test the error case"
	touch $MOTR_M0T1FS_MOUNT_DIR/$i                        || rc=1
	setfattr -n writesize -v 4096 $MOTR_M0T1FS_MOUNT_DIR/$i || rc=1
	dd if=/dev/zero of=$MOTR_M0T1FS_MOUNT_DIR/$i bs=32M count=1  || rc=0
	rm -f $MOTR_M0T1FS_MOUNT_DIR/$i                                || rc=1
	unmount_and_clean

	return $rc
}

#
# This test does large file creation and write with large block-size.
# It also checks the disk space usage before and after the written,
# and after file deletion. This check is to verify balloc alloc/free.
# This test writes 4GB of data. It requires at least 7GB of disk space.
m0t1fs_test_MOTR_2099()
{
	local rc=0
	mount_m0t1fs $MOTR_M0T1FS_MOUNT_DIR || rc=1

	df $MOTR_M0T1FS_MOUNT_DIR
	used_before=`df $MOTR_M0T1FS_MOUNT_DIR --output=used | tail -n 1`
	for i in 0:00{0..9}{0..1}; do
		# This is to create 20 files, 200MB each, 8GB in total.
		m0t1fs_file=$MOTR_M0T1FS_MOUNT_DIR/${i}
		touch_file $m0t1fs_file 8192 && run "dd if=/dev/zero of=$m0t1fs_file bs=200M count=1" || rc=1
	done
	df $MOTR_M0T1FS_MOUNT_DIR
	used_after=`df $MOTR_M0T1FS_MOUNT_DIR --output=used | tail -n 1`
	for i in 0:00{0..9}{0..1}; do
		m0t1fs_file=$MOTR_M0T1FS_MOUNT_DIR/${i}
		rm -f $m0t1fs_file
	done
	df $MOTR_M0T1FS_MOUNT_DIR
	used_delete=`df $MOTR_M0T1FS_MOUNT_DIR --output=used | tail -n 1`
	echo "used_before used_after used_delete $used_before $used_after $used_delete"
	if [ $used_before -ne $used_delete ] ; then
		echo "balloc space leak? After deletion, used blocks are not the same as before."
		rc=1
	fi

	total_blocks=`expr 200 \* 20 \* 1024` #in 1K blocks
	echo "total_blocks = $total_blocks"
	if [ $used_after -le $total_blocks ] ; then
		echo "Are you kidding? The used blocks are less than expected."
		rc=1
	fi

	unmount_and_clean
	return $rc
}

m0t1fs_system_tests()
{
	m0t1fs_basic || {
		echo "Failed: m0t1fs basic test failed."
		return 1
	}

	m0t1fs_test_MOTR_2099 || {
		echo "Failed: MOTR-2099."
		return 1
	}

	file_creation_test $MAX_NR_FILES || {
		echo "Failed: File creation test failed."
		return 1
	}

	file_creation_test $MAX_NR_FILES "oostore" || {
		echo "Failed: File creation test failed."
		return 1
	}

	m0t1fs_parallel_io_test || {
		echo "Failed: m0t1fs parallel io test failed."
		return 1
	}

	m0t1fs_oostore_mode || {
		echo "Failed: m0t1fs oostore mode test failed."
		return 1
	}

	m0t1fs_oostore_mode_basic || {
		echo "Failed: m0t1fs oostore mode basic test failed."
		return 1
	}

	m0t1fs_parallel_io_test "oostore" || {
		echo "Failed: m0t1fs parallel io test failed."
		return 1
	}

	m0t1fs_big_bs_io_test "oostore" || {
		echo "Failed: m0t1fs big BS io test failed."
		return 1
	}

	m0t1fs_large_dir "" "motr-testfile-" || {
		echo "Failed: m0t1fs large dir test failed."
		return 1
	}

	obf_test || {
		echo "Failed: Open-by-fid test failed."
		return 1
	}

	io_combinations $POOL_WIDTH $NR_DATA $NR_PARITY || {
		echo "Failed: IO failed.."
		return 1
	}

	io_combinations $POOL_WIDTH $NR_DATA $NR_PARITY "oostore"|| {
		echo "Failed: IO failed oostore mode.."
		return 1
	}

	rmw_test "" "" || {
		echo "Failed: IO-RMW failed.."
		return 1
	}

	rmw_test "" "verify" || {
		echo "Failed: IO-RMW failed.."
		return 1
	}

	return 0
}
