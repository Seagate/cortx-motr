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

. /opt/seagate/cortx/motr/common/m0_utils_common.sh

dix_repair_or_rebalance_status()
{
        local rc=0
        local op=32

        [ "$1" == "repair" ] && op=$CM_OP_REPAIR_STATUS
        [ "$1" == "rebalance" ] && op=$CM_OP_REBALANCE_STATUS

        repair_status=$(get_m0repair_utils_cmd "$op")
        run "$repair_status"
        rc=$?
        if [ $rc != 0 ]; then
                echo "DIX $1 status query failed"
        fi

        return $rc
}

wait_for_dix_repair_or_rebalance()
{
        local rc=0
        local op=32

        [ "$1" == "repair" ] && op=$CM_OP_REPAIR_STATUS
        [ "$1" == "rebalance" ] && op=$CM_OP_REBALANCE_STATUS

        status_cmd=$(get_m0repair_utils_cmd "$op")
        while true ; do
                sleep 5
                echo "$status_cmd"
                status=$(eval "$status_cmd")
                rc=$?
                if [ $rc != 0 ]; then
                        echo "DIX $1 status query failed"
                        return $rc
                fi

                echo "$status" | grep status=2 && continue #dix repair is active, continue waiting
                break;
        done

        op=$(echo "$status" | grep status=3)
        [[ !  -z  "$op"  ]] && return 1

        return 0
}

dix_repair()
{
        local rc=0

        repair_trigger=$(get_m0repair_utils_cmd "$CM_OP_REPAIR")
        run "$repair_trigger"
        rc=$?
        if [ $rc != 0 ]; then
                echo "DIX Repair failed"
        fi
        return $rc

}

dix_rebalance()
{
        local rc=0

        rebalance_trigger=$(get_m0repair_utils_cmd "$CM_OP_REBALANCE")
        run "$rebalance_trigger"
        rc=$?
        if [ $rc != 0 ]; then
                echo "DIX Re-balance failed"
        fi

        return $rc
}
