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

# set -eu -o pipefail ## commands tend to fail unpredictably

SINGLENODE="/opt/seagate/cortx/hare/share/cfgen/examples/singlenode.yaml"

sudo /usr/sbin/m0setup
if [ "$?" != 0 ]; then
    echo -e "\n\n\nERROR !! [/usr/sbin/m0setup] failed.\n\n\n"
    echo -e "\nPress CTRL+C to terminate in 10 seconds."; read -t 10 a
else
    sleep 10
fi

hctl status
if [ "$?" != 0 ]; then
    echo -e "\n\n\nERROR !! [hctl status] failed.\n\n\n"
    echo -e "\nPress CTRL+C to terminate in 10 seconds."; read -t 10 a
fi


hctl bootstrap --mkfs $SINGLENODE
if [ "$?" != 0 ]; then
    echo -e "\n\n\nERROR !! [hctl bootstrap] failed.\n\n\n"
    echo -e "\nPress CTRL+C to terminate in 10 seconds."; read -t 10 a
else
    sleep 30
fi

hctl status
if [ "$?" != 0 ]; then
    echo -e "\n\n\nERROR !! [hctl status] failed.\n\n\n"
    echo -e "\nPress CTRL+C to terminate in 10 seconds."; read -t 10 a
else
    sleep 10
fi

hctl shutdown
if [ "$?" != 0 ]; then
    echo -e "\n\n\nERROR !! [hctl status] failed.\n\n\n"
    echo -e "\nPress CTRL+C to terminate in 10 seconds."; read -t 10 a
else
    sleep 10
fi

sudo m0setup -C
if [ "$?" != 0 ]; then
    echo -e "\n\n\nERROR !! [sudo m0setup -C] failed.\n\n\n"
fi
