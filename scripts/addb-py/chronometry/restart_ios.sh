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

set -e
#set -x

status=$(hctl status)
addr="([0-9]+[.][0-9]+[.][0-9]+[.][0-9]+)"
host=$(hostname -s)

NODE_IP=`echo "$status" | grep "$host" -A 1 | grep -E -o "$addr"`
IOS_FID=`echo "$status" | grep "\[.*\].*ioservice" | grep ${NODE_IP} \
        | awk '{print $3}'`

# Wait before kill
sleep_before=$((( RANDOM % 60 ) + 120))
echo "waiting $sleep_before before killing ioservice"
sleep $sleep_before

hctl status

ios_pid=$(ps ax | grep -v grep | grep $IOS_FID | awk '{print $1}')

if [[ -z "$ios_pid" ]]; then
    echo "m0d ioservice process is not alive"
    exit 0
fi

kill -9 $ios_pid

# Wait after kill
sleep_after=$((( RANDOM % 30 ) + 10))
echo "waiting $sleep_after before starting ioservice"
sleep $sleep_after
hctl status

echo "IOS_FID: $IOS_FID"
systemctl start m0d@$IOS_FID || echo "ioservice starting failed"

sleep 10
hctl status
