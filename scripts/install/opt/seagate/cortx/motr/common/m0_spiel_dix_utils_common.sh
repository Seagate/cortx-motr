#!/bin/bash
#
# Copyright (c) 2022 Seagate Technology LLC and/or its Affiliates
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

spiel_dix_repair_start()
{
echo "$M0_SPIEL_UTILS $SPIEL_OPTS"
    eval "$M0_SPIEL_UTILS $SPIEL_OPTS" <<EOF
$SPIEL_FIDS_LIST

$SPIEL_RCONF_START

rc = spiel.dix_repair_start(fids['pool'])
print ("dix repair start rc: " + str(rc))

$SPIEL_RCONF_STOP
EOF
}

spiel_wait_for_dix_repair()
{
echo "$M0_SPIEL_UTILS $SPIEL_OPTS"
    eval "$M0_SPIEL_UTILS $SPIEL_OPTS" <<EOF
import time
$SPIEL_FIDS_LIST

$SPIEL_RCONF_START

one_status = SpielSnsStatus()
ppstatus = pointer(one_status)
active = 0
while (1):
    active = 0
    nr = spiel.dix_repair_status(fids['pool'], ppstatus)
    print ("dix repair status responded servers: " + str(nr))
    for i in range(0, nr):
        print ("status of ", ppstatus[i].sss_fid, " is: ", ppstatus[i].sss_state)
        if (ppstatus[i].sss_state == 2) :
            print ("dix is still active on ", ppstatus[i].sss_fid)
            active = 1
    if (active == 0):
        break;
    time.sleep(3)

$SPIEL_RCONF_STOP
EOF
}

spiel_dix_repair_status()
{
echo "$M0_SPIEL_UTILS $SPIEL_OPTS"
    eval "$M0_SPIEL_UTILS $SPIEL_OPTS" <<EOF

$SPIEL_FIDS_LIST

$SPIEL_RCONF_START

one_status = SpielSnsStatus()
ppstatus = pointer(one_status)
nr = spiel.dix_repair_status(fids['pool'], ppstatus)
print ("dix repair status responded servers: " + str(nr))
for i in range(0, nr):
        print("status of ", ppstatus[i].sss_fid, " is: ", ppstatus[i].sss_state)
        if (ppstatus[i].sss_state == 2) :
                print("dix repair is still active on ", ppstatus[i].sss_fid)

$SPIEL_RCONF_STOP
EOF
}

spiel_dix_rebalance_start()
{
echo "$M0_SPIEL_UTILS $SPIEL_OPTS"
    eval "$M0_SPIEL_UTILS $SPIEL_OPTS" <<EOF
$SPIEL_FIDS_LIST

$SPIEL_RCONF_START

rc = spiel.dix_rebalance_start(fids['pool'])
print ("dix rebalance start rc: " + str(rc))

$SPIEL_RCONF_STOP
EOF
}

spiel_wait_for_dix_rebalance()
{
echo "$M0_SPIEL_UTILS $SPIEL_OPTS"
    eval "$M0_SPIEL_UTILS $SPIEL_OPTS" <<EOF
import time
$SPIEL_FIDS_LIST

$SPIEL_RCONF_START

one_status = SpielSnsStatus()
ppstatus = pointer(one_status)
active = 0
while (1):
    active = 0
    nr = spiel.dix_rebalance_status(fids['pool'], ppstatus)
    print ("dix rebalance status responded servers: " + str(nr))
    for i in range(0, nr):
        print ("status of ", ppstatus[i].sss_fid, " is: ", ppstatus[i].sss_state)
        if (ppstatus[i].sss_state == 2) :
            print ("dix is still active on ", ppstatus[i].sss_fid)
            active = 1
    if (active == 0):
        break;
    time.sleep(3)

$SPIEL_RCONF_STOP
EOF
}

spiel_dix_rebalance_status()
{
echo "$M0_SPIEL_UTILS $SPIEL_OPTS"
    eval "$M0_SPIEL_UTILS $SPIEL_OPTS" <<EOF

$SPIEL_FIDS_LIST

$SPIEL_RCONF_START

one_status = SpielSnsStatus()
ppstatus = pointer(one_status)
nr = spiel.dix_rebalance_status(fids['pool'], ppstatus)
print ("dix rebalance status responded servers: " + str(nr))
for i in range(0, nr):
        print("status of ", ppstatus[i].sss_fid, " is: ", ppstatus[i].sss_state)
        if (ppstatus[i].sss_state == 2) :
                print("dix rebalance is still active on ", ppstatus[i].sss_fid)

$SPIEL_RCONF_STOP
EOF
}
