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


. $M0_SRC_DIR/utils/functions # die, sandbox_init, report_and_exit
. $M0_SRC_DIR/m0t1fs/linux_kernel/st/common.sh
. $M0_SRC_DIR/m0t1fs/linux_kernel/st/m0t1fs_common_inc.sh
. $M0_SRC_DIR/m0t1fs/linux_kernel/st/m0t1fs_client_inc.sh
. $M0_SRC_DIR/m0t1fs/linux_kernel/st/m0t1fs_server_inc.sh
. $M0_SRC_DIR/m0t1fs/linux_kernel/st/common_service_fids_inc.sh
. $M0_SRC_DIR/m0t1fs/linux_kernel/st/m0t1fs_sns_common_inc.sh


N=3
K=3
S=3
P=15

stride=32
src_bs=10M
src_count=2

XPRT=$(m0_default_xprt)
NID=$(m0_local_nid_get)
if [ "$XPRT" = "lnet" ]; then
    LA_ENDPOINT="$NID:12345:33:1"
    HA_ENDPOINT="$NID:12345:34:1"
else
    LA_ENDPOINT="$NID@3001"
    HA_ENDPOINT="$NID@2001"
fi

SPIEL_FIDS_LIST="
fids = {'profile'       : Fid(0x7000000000000001, 0),
        'pool'          : Fid(0x6f00000000000014, 1),
        'ios'           : Fid(0x7300000000000001, 3),
        'process'       : Fid(0x7200000000000001, 7),
}"
#'pool'          : Fid(0x6f00000000000020, 1), dix_pool_id
#'pool'          : Fid(0x6f00000000000001, 14),dix_pver_id
#'pool'          : Fid(0x6f00000000000001, 9),

DIXINIT_TOOL=$M0_SRC_DIR/dix/utils/m0dixinit
DIXINIT_TOOL_OPTS="-l $LA_ENDPOINT -H $HA_ENDPOINT -p <0x7000000000000001:0> -d <0x7600000000000001:14> -I <0x7600000000000001:14>"
DIXINIT_TOOL_CREATE="$DIXINIT_TOOL $DIXINIT_TOOL_OPTS -a create"
DIXINIT_TOOL_CHECK="$DIXINIT_TOOL $DIXINIT_TOOL_OPTS -a check"
DIXINIT_TOOL_DESTROY="$DIXINIT_TOOL $DIXINIT_TOOL_OPTS -a destroy"


SPIEL_RCONF_START="
print ('Hello, start to run Motr embedded python')

if spiel.cmd_profile_set(str(fids['profile'])):
    sys.exit('cannot set profile {0}'.format(fids['profile']))

if spiel.rconfc_start():
    sys.exit('cannot start rconfc')
"

SPIEL_RCONF_STOP="
spiel.rconfc_stop()
print ('----------Done------------')
"

PYTHON_STUFF=python_files.txt

verify()
{
	local log_file=$SANDBOX_DIR/check.txt
	local res=$SANDBOX_DIR/res.txt
	echo "verifying ..."
	$DIXINIT_TOOL_CHECK  >$log_file 2>&1
	grep "Metadata exists:" $log_file | grep -v "Metadata exists: true" > $res
	if [ -s $res ]
	then
		echo "See log file with results: $log_file, $res"
		return 1
	fi
	echo "dix verification success"
	return 0
}

spiel_prepare()
{
	local NID=$(m0_local_nid_get)
    if [ "$XPRT" = "lnet" ]; then
        local SPIEL_CLIENT_ENDPOINT="$NID:12345:34:1001"
        local SPIEL_HA_ENDPOINT="$NID:12345:34:1"
    else
        local SPIEL_CLIENT_ENDPOINT="$NID@11001"
        local SPIEL_HA_ENDPOINT="$NID@2001"
    fi
	SPIEL_OPTS=" -l $M0_SRC_DIR/motr/.libs/libmotr.so --client $SPIEL_CLIENT_ENDPOINT --ha $SPIEL_HA_ENDPOINT"

	export SPIEL_OPTS=$SPIEL_OPTS
	export SPIEL_FIDS_LIST=$SPIEL_FIDS_LIST

	echo SPIEL_OPTS=$SPIEL_OPTS
	echo SPIEL_FIDS_LIST=$SPIEL_FIDS_LIST

	# install "motr" Python module required by m0spiel tool
	cd $M0_SRC_DIR/utils/spiel
	python2 setup.py install --record $PYTHON_STUFF > /dev/null ||\
		die 'Cannot install Python "motr" module'
	cd -
}

spiel_cleanup()
{
	cd $M0_SRC_DIR/utils/spiel
	cat $PYTHON_STUFF | xargs rm -rf
	rm -rf build/ $PYTHON_STUFF
	cd -
}

spiel_dix_repair_start()
{
echo $M0_SRC_DIR/utils/spiel/m0spiel $SPIEL_OPTS
    $M0_SRC_DIR/utils/spiel/m0spiel $SPIEL_OPTS <<EOF
$SPIEL_FIDS_LIST

$SPIEL_RCONF_START

rc = spiel.dix_repair_start(fids['pool'])
print ("dix repair start rc: " + str(rc))

$SPIEL_RCONF_STOP
EOF
}

spiel_dix_repair_abort()
{
echo $M0_SRC_DIR/utils/spiel/m0spiel $SPIEL_OPTS
    $M0_SRC_DIR/utils/spiel/m0spiel $SPIEL_OPTS <<EOF
$SPIEL_FIDS_LIST

$SPIEL_RCONF_START

rc = spiel.dix_repair_abort(fids['pool'])
print ("dix repair abort rc: " + str(rc))

$SPIEL_RCONF_STOP
EOF
}

spiel_dix_repair_quiesce()
{
echo $M0_SRC_DIR/utils/spiel/m0spiel $SPIEL_OPTS
    $M0_SRC_DIR/utils/spiel/m0spiel $SPIEL_OPTS <<EOF
$SPIEL_FIDS_LIST

$SPIEL_RCONF_START

rc = spiel.dix_repair_quiesce(fids['pool'])
print ("dix repair quiesce rc: " + str(rc))

$SPIEL_RCONF_STOP
EOF
}

spiel_dix_repair_continue()
{
echo $M0_SRC_DIR/utils/spiel/m0spiel $SPIEL_OPTS
    $M0_SRC_DIR/utils/spiel/m0spiel $SPIEL_OPTS <<EOF
$SPIEL_FIDS_LIST

$SPIEL_RCONF_START

rc = spiel.dix_repair_continue(fids['pool'])
print ("dix repair continue rc: " + str(rc))

$SPIEL_RCONF_STOP
EOF
}

spiel_wait_for_dix_repair()
{
echo $M0_SRC_DIR/utils/spiel/m0spiel $SPIEL_OPTS
    $M0_SRC_DIR/utils/spiel/m0spiel $SPIEL_OPTS <<EOF
import time
$SPIEL_FIDS_LIST

$SPIEL_RCONF_START

one_status = SpielSnsStatus()
ppstatus = pointer(one_status)
active = 0
while (1):
    active = 0
    rc = spiel.dix_repair_status(fids['pool'], ppstatus)
    print ("dix repair status responded servers: " + str(rc))
    for i in range(0, rc):
        print "status of ", ppstatus[i].sss_fid, " is: ", ppstatus[i].sss_state
        if (ppstatus[i].sss_state == 2) :
            print "dix is still active on ", ppstatus[i].sss_fid
            active = 1
    if (active == 0):
        break;
    time.sleep(3)

$SPIEL_RCONF_STOP
EOF
}

spiel_dix_rebalance_start()
{
echo $M0_SRC_DIR/utils/spiel/m0spiel $SPIEL_OPTS
    $M0_SRC_DIR/utils/spiel/m0spiel $SPIEL_OPTS <<EOF
$SPIEL_FIDS_LIST

$SPIEL_RCONF_START

rc = spiel.dix_rebalance_start(fids['pool'])
print ("dix rebalance start rc: " + str(rc))

$SPIEL_RCONF_STOP
EOF
}

spiel_dix_rebalance_quiesce()
{
echo $M0_SRC_DIR/utils/spiel/m0spiel $SPIEL_OPTS
    $M0_SRC_DIR/utils/spiel/m0spiel $SPIEL_OPTS <<EOF
$SPIEL_FIDS_LIST

$SPIEL_RCONF_START

rc = spiel.dix_rebalance_quiesce(fids['pool'])
print ("dix rebalance quiesce rc: " + str(rc))

$SPIEL_RCONF_STOP
EOF
}

spiel_dix_rebalance_continue()
{
echo $M0_SRC_DIR/utils/spiel/m0spiel $SPIEL_OPTS
    $M0_SRC_DIR/utils/spiel/m0spiel $SPIEL_OPTS <<EOF
$SPIEL_FIDS_LIST

$SPIEL_RCONF_START

rc = spiel.dix_rebalance_continue(fids['pool'])
print ("dix rebalance continue rc: " + str(rc))

$SPIEL_RCONF_STOP
EOF
}

spiel_wait_for_dix_rebalance()
{
echo $M0_SRC_DIR/utils/spiel/m0spiel $SPIEL_OPTS
    $M0_SRC_DIR/utils/spiel/m0spiel $SPIEL_OPTS <<EOF
import time
$SPIEL_FIDS_LIST

$SPIEL_RCONF_START

one_status = SpielSnsStatus()
ppstatus = pointer(one_status)
active = 0
while (1):
    active = 0
    rc = spiel.dix_rebalance_status(fids['pool'], ppstatus)
    print ("dix rebalance status responded servers: " + str(rc))
    for i in range(0, rc):
        print "status of ", ppstatus[i].sss_fid, " is: ", ppstatus[i].sss_state
        if (ppstatus[i].sss_state == 2) :
            print "dix is still active on ", ppstatus[i].sss_fid
            active = 1
    if (active == 0):
        break;
    time.sleep(3)

$SPIEL_RCONF_STOP
EOF
}

spiel_dix_rebalance_abort()
{
echo $M0_SRC_DIR/utils/spiel/m0spiel $SPIEL_OPTS
    $M0_SRC_DIR/utils/spiel/m0spiel $SPIEL_OPTS <<EOF
$SPIEL_FIDS_LIST

$SPIEL_RCONF_START

rc = spiel.dix_rebalance_abort(fids['pool'])
print ("rebalance abort rc: " + str(rc))

$SPIEL_RCONF_STOP
EOF
}


