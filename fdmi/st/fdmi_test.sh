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

MOTR_SRC_ROOT=$PWD/`dirname $0`/../..
M0T1FS_TEST_DIR=$MOTR_SRC_ROOT/m0t1fs/linux_kernel/st
ECHO_PLUGIN_DIR=$MOTR_SRC_ROOT/fdmi/st/echo

. $M0T1FS_TEST_DIR/common.sh
. $M0T1FS_TEST_DIR/m0t1fs_common_inc.sh
. $M0T1FS_TEST_DIR/m0t1fs_client_inc.sh
. $M0T1FS_TEST_DIR/m0t1fs_server_inc.sh

MOTR_CORE_ROOT=${M0T1FS_TEST_DIR%/m0t1fs*}
MOTR_SVCS_CNT=1

unmount_and_stop()
{
	for i in `seq 1 $MOTR_SVCS_CNT` ; do
		echo "Unmounting file system $i ..."
		umount $MOTR_M0T1FS_MOUNT_DIR-$i
	done

	sleep 10
	echo "Stopping services..."

	motr_service stop
	if [ $? -ne "0" ]
	then
		echo "Failed to stop Motr Services"
		return 1
	fi
}

fdmi_test_prepare()
{
	NODE_UUID=`uuidgen`

	for i in `seq 1 $MOTR_SVCS_CNT` ; do
		MOTR_M0T1FS_TEST_DIR=/var/motr/systest-$$-$i

		CONFD_EP=12345:3$i:100

		# list of io server end points: e.g., tmid in [900, 999).
		IOSEP=(
		    12345:3$i:900   # IOS1 EP
		    12345:3$i:901   # IOS2 EP
		    12345:3$i:902   # IOS3 EP
		    #12345:33:903   # IOS4 EP
		)

		# list of md server end points tmid in [800, 899)
		MDSEP=(
		    12345:3$i:800   # MDS1 EP
		    #12345:33:801   # MDS2 EP
		    #12345:33:802   # MDS3 EP
		)

		echo "Starting Motr service #$i"
		motr_service start
		if [ $? -ne "0" ]
		then
			echo "Failed to start Motr Service."
			convert_logs

			return 1
		else
			echo "Motr Services are started successfully."
		fi

		mount_m0t1fs $MOTR_M0T1FS_MOUNT_DIR-$i $NR_DATA $NR_PARITY $POOL_WIDTH || {
			return 1
		}
	done
}

fdmi_file_creation_test()
{
	local nr_files=$1
	local target_dir=$2
	local SOURCE_TXT=/tmp/source.txt

	for i in {a..z} {A..Z} ; do
		for c in `seq 1 4095`;
			do echo -n $i ;
		done;
		echo;
	done > $SOURCE_TXT

	echo "Test: Creating $nr_files files on m0t1fs..."
	for ((i=0; i<$nr_files; ++i)); do
		touch $target_dir/file$i || break
		cp -v $SOURCE_TXT $target_dir/file$i || break
		cp -v $target_dir/file$i /tmp/dest.txt || break
		diff -C 0 $SOURCE_TXT /tmp/dest.txt || {
			echo "file content differ!!!!!!!!!!!!!!!!! at $i file. "
#			echo "Press Enter to go";
#			read;
			break;
		}
	done
	echo -n "Test: file creation: "
	if [ $i -eq $nr_files ]; then
		echo "success."
	else
		echo "failed."
		return 1
	fi

	echo "Test: removing $nr_files files on m0t1fs..."
	for ((i=0; i<$nr_files; ++i)); do
		rm -vf $target_dir/file$i || break
	done

	unmount_and_clean
	echo -n "Test: file removal: "
	if [ $i -eq $nr_files ]; then
		echo "success."
	else
		echo "failed."
		return 1
	fi

	return 0
}

fdmi_test_unprepare()
{
	local rc=0
	unmount_and_stop || rc=$?
	unprepare || rc=$?
	return $rc
}

main()
{
	local rc=0
	fdmi_test_prepare
	echo "Run plugin..."
	#read
	for i in `seq 1 $MOTR_SVCS_CNT`; do
		fdmi_file_creation_test 10 $MOTR_M0T1FS_MOUNT_DIR-$i
	done
	sleep 10
	fdmi_test_unprepare || rc=$?
	return $rc
}

trap unprepare EXIT
main
