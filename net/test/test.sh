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

# see original file at utils/linux_kernel/ut.sh

# Small wrapper to run motr network benchmark node module

if [ "$(id -u)" -ne 0 ]; then
    echo "Must be run as root"
    exit 1
fi

d="`git rev-parse --show-cdup`"
if [ -n "$d" ]; then
    cd "$d"
fi

. m0t1fs/linux_kernel/st/common.sh

MODLIST="m0tr.ko"
MODMAIN="m0nettestd.ko"

log='/var/log/kern'
if [ ! -e "$log" ]; then
    log='/var/log/messages'
fi
tailseek=$(( $(stat -c %s "$log") + 1 ))

# currently, kernel UT runs as part of loading m0ut module
modload
insmod $MODMAIN "$*"
rmmod $MODMAIN
modunload

tail -c+$tailseek "$log" | grep ' kernel: '
