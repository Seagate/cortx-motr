#!/usr/bin/env python
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


import fileinput
import record
import sys

def filter(argv):
    tr = record.trace(height = 10, width = 1000, loc_nr = 1, duration = 1,
                      step = 1)
    rec = ""
    fname = ""
    f = None
    node = ""
    for line in fileinput.input([]):
        params = line[1:].split()
        if line[0] == "*":
            if rec != "":
                name = "out." + node + "." + pid + "." + time
                if name != fname:
                    if f != None:
                        f.close()
                    f = open(name, "w+")
                    fname = name
                f.write(rec)
                rec = ""
            time = params[0][0:19]
            keep = record.keep(params[1])
        elif params[0] == "node":
            node = params[1]
        elif params[0] == "pid":
            pid = params[1]
        if keep:
            rec += line
    f.close()

if __name__ == "__main__":
    filter(sys.argv)


