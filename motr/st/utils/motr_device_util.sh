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


# Script for starting Motr system tests in "scripts/m0 run-st"

#set -x

motr_st_util_dir=`dirname "$0"`
m0t1fs_st_dir=$motr_st_util_dir/../../../m0t1fs/linux_kernel/st

# Re-use as many m0t1fs system scripts as possible
. "$m0t1fs_dir"/common.sh
. "$m0t1fs_dir"/m0t1fs_common_inc.sh
. "$m0t1fs_dir"/m0t1fs_client_inc.sh
. "$m0t1fs_dir"/m0t1fs_server_inc.sh
. "$m0t1fs_dir"/m0t1fs_sns_common_inc.sh

motr_st_set_failed_devices()
{
	disk_state_set "failed" "$1" || {
		echo "Failed: pool_mach_set_failure..."
		return 1
	}
}

motr_st_query_devices()
{
	disk_state_get "$1" || {
		echo "Failed: pool_mach_query..."
		return 1
	}
}

# Print out usage
usage()
{
	cat <<.
Usage:

$ sudo motr_device_util [down|query] devices
.
}

# Get options
cmd=$1
devices=$2

case "$cmd" in
	down)
		motr_st_set_failed_devices "$devices"
		;;
	query)
		motr_st_query_devices "$devices"
		;;
	*)
		usage
		exit
esac


# Local variables:
# sh-basic-offset: 8
# sh-indentation: 8
# tab-width: 8
# End:
