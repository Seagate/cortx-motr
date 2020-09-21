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

attr={ "name": "fom_to_rpc" }

def query(from_, to_):
    q=f"""
    SELECT (rpc_req.time-fom_req.time), fom_req.state, rpc_req.state
    FROM fom_desc
    JOIN fom_req on fom_req.id=fom_sm_id
    JOIN rpc_req on rpc_req.id=rpc_sm_id
    WHERE fom_desc.req_opcode LIKE "%M0_IOSERVICE_%"
    AND rpc_req.pid=fom_req.pid
    AND rpc_req.state="{to_}" AND fom_req.state="{from_}";
    """
    return q

if __name__ == '__main__':
    import sys
    sys.exit(1)
