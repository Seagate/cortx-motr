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


BETOOL="sudo utils/m0run m0betool"
LT_BETOOL="lt-m0betool"
KILL_INTERVAL=60

function kill_betool()
{
	kill -SIGKILL `pidof $LT_BETOOL`
}

trap kill_betool EXIT

rm -rf /var/motr/m0betool
$BETOOL st mkfs
i=0
while true; do
	$BETOOL st run &
	PID="$!"
	sleep $KILL_INTERVAL
	kill -SIGKILL `pidof $LT_BETOOL`
	KILL_STATUS=$?
	wait
	if [ $KILL_STATUS -ne 0 ]; then
		break
	fi
	((++i))
	echo "iteration #$i"
done
