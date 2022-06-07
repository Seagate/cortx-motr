#!/usr/bin/env bash
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


usage()
{
	echo "Usage: $(basename $0) <start|stop> [server_nid]"
	echo "Please provide the server endpoint address you want to use."
	echo "e.g. 192.168.172.130@tcp"
	echo "If you want to use the default nid registered for lnet, then do"
	echo "$(basename $0) start default"
}

if [ $# -lt 1 ]
then
	usage
        exit 1
fi

if [ "x$1" = "x-h" ]; then
	usage
	exit 0
fi

. $(dirname $0)/common.sh
. $(dirname $0)/m0t1fs_common_inc.sh
. $(dirname $0)/m0t1fs_server_inc.sh

main()
{
	local multiple_pools=0

	motr_service $1 $multiple_pools
	if [ $? -ne "0" ]
	then
		echo "Failed to trigger Motr Service."
		exit 1
	fi
}

main $1 $2
