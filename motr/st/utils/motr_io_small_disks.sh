#!/usr/bin/env bash
#
# Copyright (c) 2022 Seagate Technology LLC and/or its Affiliates
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


TOPDIR=$(dirname "$0")/../../../

. "${TOPDIR}/m0t1fs/linux_kernel/st/common.sh"
. "${TOPDIR}/m0t1fs/linux_kernel/st/m0t1fs_common_inc.sh"
. "${TOPDIR}/m0t1fs/linux_kernel/st/m0t1fs_client_inc.sh"
. "${TOPDIR}/m0t1fs/linux_kernel/st/m0t1fs_server_inc.sh"

export N=2
export K=1
export S=1
export P=4
export stride=32


export MOTR_CLIENT_ONLY=1

# The ioservice will have a very small disk to trigger -ENOSPC test
export IOS_DISK_SEEK_BLOCK_COUNT=1024

writing_objects_to_fill_devices()
{
	local rc=0
	local i=0

	dd if=/dev/urandom bs=1M count=128 \
	   of="$MOTR_M0T1FS_TEST_DIR/srcfile" || return $?

	for i in $(seq 0 1000) ; do
		local lid=9
		local us=$((1024 * 1024))
		local count=128
		local obj="100000:10000$i"

		echo "creating object ${obj} bs=${us} * c=${count}"

		"$M0_SRC_DIR/motr/st/utils/m0cp" -l "${lnet_nid}:$SNS_MOTR_CLI_EP"  \
						 -H "${lnet_nid}:$HA_EP"            \
						 -p "$PROF_OPT"                     \
						 -P "$M0T1FS_PROC_ID"               \
						 -L "${lid}"                        \
						 -s "${us}"                         \
						 -c "${count}"                      \
						 -o "${obj}"                        \
						 "$MOTR_M0T1FS_TEST_DIR/srcfile" || {
			rc=$?
			echo "Writing object ${obj} failed: $rc"
			return $rc
		}
		rm -f m0trace.*
	done
	return $rc
}


motr_io_small_disks()
{
	local rc=0

	echo "Startin motr_io_small_disks testing ..."

	writing_objects_to_fill_devices
	rc=$?
	if [[ $rc -eq 28 ]] ; then
		# ENOSPC == 28
		echo "We got $rc as expected"
		rc=0
	else
		echo "We got $rc. This is not expected."
		rc=1
	fi

	return $rc
}

main()
{
	local rc=0

	sandbox_init

	NODE_UUID=$(uuidgen)
	local multiple_pools=0
	motr_service start $multiple_pools $stride $N $K $S $P || {
		echo "Failed to start Motr Service."
		return 1
	}


	if [[ $rc -eq 0 ]] && ! motr_io_small_disks ; then
		rc=1
		echo "Failed: Motr I/O testing on small disks.."
	fi

	motr_service stop || {
		echo "Failed to stop Motr Service."
		rc=1
	}
	if [[ $rc -eq 0 ]]; then
		sandbox_fini
	fi
	return $rc
}

trap unprepare EXIT
main
report_and_exit motr_io_small_disks $?
