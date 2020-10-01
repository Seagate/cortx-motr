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

set -exu


M0_SRC_DIR="$(realpath $0)"
M0_SRC_DIR="${M0_SRC_DIR%/*/*}"
export H0_SRC_DIR=${H0_SRC_DIR:-${M0_SRC_DIR%/*}/halon}


. "$M0_SRC_DIR"/utils/functions

# Start halon and all involved motr services.
"$H0_SRC_DIR"/scripts/h0 start
[ $? -eq 0 ] || report_and_exit m0mt $?

# Run motr load test with parameters $MOTR_EP, $HALON_EP, $PROFILE, $PROCESS
PROFILE=$(halonctl motr status | grep profile | sed -r 's/[ \t]+//g' | sed -r 's/profile:(0x[0-9a-z]+:0x[0-9a-z]+)/\1/g')
PROCESS=$(halonctl motr status | grep motr-app | grep N/A | head -1 | sed -r 's/[ \t]+/~/g' | sed -r 's/.*~(0x[0-9a-z]+:0x[0-9a-z]+)~(.*)~motr-app/\1/g')
MOTR_EP=$(halonctl motr status | grep motr-app | grep N/A | head -1 | sed -r 's/[ \t]+/~/g' | sed -r 's/.*~(0x[0-9a-z]+:0x[0-9a-z]+)~(.*)~motr-app/\2/g')
HALON_EP=$(halonctl motr status | grep halon  | sed -r 's/[ \t]+/~/g' | sed -r 's/.*~(0x[0-9a-z]+:0x[0-9a-z]+)~(.*)~halon/\2/g')
"$M0_SRC_DIR"/motr/st/mt/utils/m0mt -l "$MOTR_EP" -h "$HALON_EP" \
				    -p "$PROFILE" -f "$PROCESS"
[ $? -eq 0 ] || report_and_exit m0mt $?

# Stop halon and all involved motr services.
"$H0_SRC_DIR"/scripts/h0 stop
report_and_exit m0mt $?
