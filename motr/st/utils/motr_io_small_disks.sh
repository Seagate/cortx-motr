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
. "${TOPDIR}/m0t1fs/linux_kernel/st/m0t1fs_sns_common_inc.sh"
. "${TOPDIR}/motr/st/utils/sns_repair_common_inc.sh"


export MOTR_CLIENT_ONLY=1

# The ioservice will have a very small disk to trigger -ENOSPC test
export IOS_DISK_SEEK_BLOCK_COUNT=500

motr_io_small_disks()
{
	local rc=0

	echo "Startin motr_io_small_disks testing ..."

	prepare_datafiles_and_objects
	rc=$?
	if [[ $rc -eq 28 ]] ; then
		# ENOSPC == 28
		echo "We got $rc as expected"
		rc=0
	else
		echo "We got $rc. This is not expected."
		rc=1
	fi

	return $?
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
