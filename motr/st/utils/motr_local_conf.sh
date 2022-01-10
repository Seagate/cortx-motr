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

M0_SRC_DIR=$(dirname $(readlink -f $0))
M0_SRC_DIR="$M0_SRC_DIR/../../../"

. $M0_SRC_DIR/utils/functions # m0_default_xprt

XPRT=$(m0_default_xprt)

# Get local address and other parameters to start services
if [ "$XPRT" = "lnet" ]; then
	modprobe lnet &>> /dev/null
	lctl network up &>> /dev/null
fi
LOCAL_NID=$(m0_local_nid_get)

if [ X$LOCAL_NID == X ]; then
	echo "lnet is not up"
	exit
fi

if [ X$MOTR_LOCAL_EP == X ]; then
	if [ "$XPRT" = "lnet" ]; then
		export MOTR_LOCAL_EP=$LOCAL_NID:12345:34:101
		export MOTR_LOCAL_EP2=$LOCAL_NID:12345:34:102
		export MOTR_LOCAL_EP3=$LOCAL_NID:12345:34:103
		export MOTR_LOCAL_EP4=$LOCAL_NID:12345:34:104
	else
		export MOTR_LOCAL_EP=$LOCAL_NID@3101
		export MOTR_LOCAL_EP2=$LOCAL_NID@3102
		export MOTR_LOCAL_EP3=$LOCAL_NID@3103
		export MOTR_LOCAL_EP4=$LOCAL_NID@3104
	fi
fi

if [ X$MOTR_HA_EP == X ]; then
	if [ "$XPRT" = "lnet" ]; then
		MOTR_HA_EP=$LOCAL_NID:12345:34:1
	else
		MOTR_HA_EP=$LOCAL_NID@2001
	fi
fi

if [ X$MOTR_PROF_OPT == X ]; then
	MOTR_PROF_OPT=0x7000000000000001:0
fi

if [ X$MOTR_PROC_FID == X ]; then
	MOTR_PROC_FID=0x7200000000000000:0
fi
