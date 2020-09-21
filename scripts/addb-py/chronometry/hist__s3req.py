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

attr={ "name": "s3_req" }
def query(from_, to_):
    q=f"""
    SELECT (SELECT MIN(to_.time)
            FROM s3_request_state AS to_
            WHERE to_.id = frm.id
              AND to_.pid = frm.pid
              AND to_.time > frm.time
              AND to_.state = "{to_}") - frm.time AS time,
           "{from_}",
           "{to_}",
           frm.id
    FROM s3_request_state AS frm
    JOIN (SELECT flt.pid, flt.id
          FROM s3_request_state AS flt
          WHERE flt.state = "{to_}") filter_state
    ON filter_state.id = frm.id AND filter_state.pid = frm.pid
    WHERE frm.state="{from_}";
    """
    return q

if __name__ == '__main__':
    import sys
    sys.exit(1)
