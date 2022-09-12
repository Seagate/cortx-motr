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
	echo "Usage: $(basename $0) server_nid"
	echo "Please provide the server nid you want to use."
	echo "e.g. 192.168.172.130@tcp"
}

if [ $# -lt 1 ]
then
	usage
        exit 1
fi

if [ "x$1" = "x-h" ];
then
	usage
	exit 0
fi

server_nid=$1

. $(dirname $0)/common.sh
. $(dirname $0)/m0t1fs_common_inc.sh
. $(dirname $0)/m0t1fs_client_inc.sh

main()
{
	prepare

	echo "Prepare done, starting tests..."

	m0t1fs_system_tests
	if [ $? -ne "0" ]
	then
		return 1
	fi

        return 0
}

trap unprepare EXIT

main $1
