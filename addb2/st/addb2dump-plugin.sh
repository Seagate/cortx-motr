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


SCRIPT_PATH="$(realpath $0)"
MOTR_SRC_DIR="${SCRIPT_PATH%/*/*/*}"
UT_SANDBOX_DIR="/var/motr/m0ut/ut-sandbox"
ADDB2_STOB="/var/motr/m0ut/ut-sandbox/__s/o/100000000000000:2"
PLUGIN_SO="${MOTR_SRC_DIR}/addb2/st/.libs/libaddb2dump-plugin-test.so"

. "${MOTR_SRC_DIR}"/utils/functions

function check_root() {
    [[ $UID -eq 0 ]] || {
    echo 'Please, run this script with "root" privileges.' >&2
    exit 1
    }
}

function generate_addb2_stob() {
    "${MOTR_SRC_DIR}"/utils/m0run -- "m0ut -k -t addb2-storage:write-many" > /dev/null
}

function dump_addb2_stob() {
    ADDB2_DUMP=$("${MOTR_SRC_DIR}"/utils/m0addb2dump -f -p "${PLUGIN_SO}" \
    -- "${ADDB2_STOB}" | grep "measurement")
}

function delete_ut_sandbox() {
    rm -rf ${UT_SANDBOX_DIR}
}

function check_ext_measurements() {
    local measure_1=$(echo "$ADDB2_DUMP" \
                | grep -P "\*.*measurement_1\s+param1:\s0x\d+,\sparam2:\s0x\d+")
    local measure_2=$(echo "$ADDB2_DUMP" \
                | grep -P "\*.*measurement_2\s+param1:\s0x\d+,\sparam2:\s0x\d+")
    local measure_3=$(echo "$ADDB2_DUMP" \
                | grep -P "\*.*measurement_3\s+param1:\s0x\d+,\sparam2:\s0x\d+")
    local measure_4=$(echo "$ADDB2_DUMP" \
                | grep -P "\*.*measurement_4\s+param1:\s0x\d+,\sparam2:\s0x\d+")

    [[ -n "$measure_1" && -n "$measure_2" && -n "$measure_3" && -n "$measure_4" ]]
}


check_root
generate_addb2_stob && dump_addb2_stob \
    && delete_ut_sandbox && check_ext_measurements

rc=$?
report_and_exit addb2dump-plugin $rc
