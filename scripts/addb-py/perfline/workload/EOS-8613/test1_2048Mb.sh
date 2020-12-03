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
set -x

SCRIPT_NAME=`echo $0 | awk -F "/" '{print $NF}'`
SCRIPT_PATH="$(readlink -f $0)"
SCRIPT_DIR="${SCRIPT_PATH%/*}"

function stop_pcs() {
    pcs resource disable motr-ios-c1
    pcs resource disable s3server-c1-{1,2,3,4,5,6,7,8,9,10,11}
    sleep 30
}

function start_pcs() {
    pcs resource enable motr-ios-c1
    sleep 10
    pcs resource enable s3server-c1-{1,2,3,4,5,6,7,8,9,10,11}
    sleep 30
}

$SCRIPT_DIR/../s3bench_run.sh -n 10000 -c 128 -o 2048m -f log_before.log
stop_pcs
start_pcs
$SCRIPT_DIR/../s3bench_run.sh -n 10000 -c 128 -o 2048m -f log_after.log
