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

# Choose one of the patterns
PATTERN="%WRITEV%"   # IO write
#PATTERN="%READV%"    # IO read
#PATTERN="%CAS_PUT%"  # CAS put
#PATTERN="%CAS_GET%"  # CAS get

# SQL limit settings
LIMIT_OFFSET=1000
LIMIT=10

echo "============================================================="
echo "Pattern: $PATTERN, limit offset: $LIMIT_OFFSET, limit: $LIMIT"
echo "============================================================="

for x in $(echo "select fom_sm_id, pid from fom_desc where req_opcode like '$PATTERN' limit $LIMIT_OFFSET,$LIMIT;" | sqlite3 m0play.db); do
    IFS='|' read -r -a args <<< "$x"
    echo "FOM id: ${args[0]}, pid: ${args[1]}"
    python3 fom_req.py -f ${args[0]} -p ${args[1]}
    echo "-------------------------------------------------------------"
done
