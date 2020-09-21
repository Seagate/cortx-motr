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


# main entry
#set -x

MOTR_ST_UTIL_DIR=`dirname $0`

if [ ${0:0:1} = "/" ]; then
	MOTR_ST_UTIL_DIR=`dirname $0`
else
	MOTR_ST_UTIL_DIR=$PWD/`dirname $0`
fi
MOTR_DIR=${MOTR_ST_UTIL_DIR%/motr*}
echo " MOTR_DIR =$MOTR_DIR"

M0T1FS_ST_DIR=$MOTR_DIR/m0t1fs/linux_kernel/st
echo "$M0T1FS_ST_DIR"

. $M0T1FS_ST_DIR/common.sh
. $M0T1FS_ST_DIR/m0t1fs_common_inc.sh
. $M0T1FS_ST_DIR/m0t1fs_client_inc.sh
. $M0T1FS_ST_DIR/m0t1fs_server_inc.sh


# start | stop service
multiple_pools_flag=1
case "$1" in
    start)
	motr_service start $multiple_pools_flag
	if [ $? -ne "0" ]
	then
		echo "Failed to start Motr Service."
	fi
	;;
    stop)
	motr_service stop $multiple_pools_flag
	echo "Motr services stopped."
	;;
    *)
	echo "Usage: $0 {start|stop]}"
esac

# Local variables:
# sh-basic-offset: 8
# sh-indentation: 8
# tab-width: 8
# End:
