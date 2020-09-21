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

RES_HARE_ST=0
RES_MOTR_ST=0
RES_MOTR_KUT=0
RES_MOTR_UT=0
RES_M0PROV_TEST=0
RES_M0PROV_CONFIG=0

CORTX_LNET_SANITY="/opt/seagate/cortx/motr/sanity/cortx_lnet_sanity.sh"
CORTX_LNET_SANITY_BAK=$CORTX_LNET_SANITY".bak"
echo -e "The vagrant box has different version of lnet installed, than what is finalized for production."
echo -e "Lustre components from $CORTX_LNET_SANITY"
grep "lustre-client" $CORTX_LNET_SANITY
echo -e "Lustre conponents installed in the system"
rpm -qa | grep "lustre-client"
echo -e "Will replace the lustre component names in $CORTX_LNET_SANITY"
sudo cp -v $CORTX_LNET_SANITY $CORTX_LNET_SANITY_BAK
sudo sed -i 's/kmod-lustre-client-2.12.3-1.el7.x86_64/kmod-lustre-client-2.12.3-99.el7.x86_64/g' $CORTX_LNET_SANITY
sudo sed -i 's/lustre-client-2.12.3-1.el7.x86_64/lustre-client-2.12.3-99.el7.x86_64/g' $CORTX_LNET_SANITY

# m0provision config
echo -e "\n\n*** *** *** *** RUN [/usr/sbin/m0provision config] *** *** *** *** \n\n"
time sudo /usr/sbin/m0provision config
RES_M0PROV_CONFIG=$?
echo -e "\n\n*** *** *** *** COMPLETED [/usr/sbin/m0provision config] [$RES_M0PROV_CONFIG] *** *** *** *** \n\n"

# m0provision test
echo -e "\n\n*** *** *** *** RUN [/usr/sbin/m0provision test] *** *** *** *** \n\n"
time sudo /usr/sbin/m0provision test
RES_M0PROV_TEST=$?
echo -e "\n\n*** *** *** *** COMPLETED [/usr/sbin/m0provision test] [$RES_M0PROV_TEST] *** *** *** *** \n\n"

echo -e "Restoring original $CORTX_LNET_SANITY"
sudo rm -v $CORTX_LNET_SANITY
sudo mv -v $CORTX_LNET_SANITY_BAK $CORTX_LNET_SANITY

# To be extra sure
sudo systemctl stop motr-kernel
echo -e "\n\n*** *** *** *** RUN Motr UT *** *** *** *** \n\n"
time sudo /data/motr/scripts/m0 run-ut
RES_MOTR_UT=$?
echo -e "\n\n*** *** *** *** COMPLETED Motr UT [$RES_MOTR_UT] *** *** *** *** \n\n"

# To be extra sure
sudo systemctl stop motr-kernel
echo -e "\n\n*** *** *** *** RUN Motr KUT *** *** *** *** \n\n"
time sudo /data/motr/scripts/m0 run-kut
RES_MOTR_KUT=$?
echo -e "\n\n*** *** *** *** COMPLETED Motr KUT [$RES_MOTR_KUT] *** *** *** *** \n\n"
=======

# To be extra sure
sudo systemctl stop motr-kernel
echo -e "\n\n*** *** *** *** RUN Motr ST *** *** *** *** \n\n"
time sudo /data/motr/scripts/m0 run-st
RES_MOTR_ST=$?
echo -e "\n\n*** *** *** *** COMPLETED Motr ST [$RES_MOTR_ST] *** *** *** *** \n\n"

# To be extra sure
sudo systemctl stop motr-kernel
echo -e "\n\n*** *** *** *** RUN Hare ST *** *** *** *** \n\n"
time sudo /data/hare/scripts/h0 test run-st
RES_HARE_ST=$?
echo -e "\n\n*** *** *** *** COMPLETED Hare ST [$RES_HARE_ST] *** *** *** *** \n\n"

if [ "$RES_HARE_ST" == "0" ]; then
	echo -e "Hare ST has passed."
else
	echo -e "Hare ST failed with error return code [$RES_HARE_ST]"
fi

if [ "$RES_MOTR_ST" == "0" ]; then
	echo -e "Motr ST has passed."
else
	echo -e "Motr ST failed with error return code [$RES_MOTR_ST]"
fi

if [ "$RES_MOTR_KUT" == "0" ]; then
	echo -e "Motr KUT has passed."
else
	echo -e "Motr KUT failed with error return code [$RES_MOTR_KUT]"
fi

if [ "$RES_MOTR_UT" == "0" ]; then
	echo -e "Motr UT has passed."
else
	echo -e "Motr UT failed with error return code [$RES_MOTR_UT]"
fi

if [ "$RES_M0PROV_TEST" == "0" ]; then
	echo -e "Motr [m0provision test] has passed."
else
	echo -e "Motr [m0provision test] failed with error return code [$RES_M0PROV_TEST]"
fi

if [ "$RES_M0PROV_CONFIG" == "0" ]; then
	echo -e "Motr [m0provision config] has passed."
else
	echo -e "Motr [m0provision config] failed with error return code [$RES_M0PROV_CONFIG]"
fi

RESULT=1
if ([ "$RES_HARE_ST" == "0" ] &&
	[ "$RES_MOTR_ST" == "0" ] &&
	[ "$RES_MOTR_KUT" == "0" ] &&
	[ "$RES_MOTR_UT" == "0" ] &&
	[ "$RES_M0PROV_TEST" == "0" ] &&
	[ "$RES_M0PROV_CONFIG" == "0" ]); then
	echo -e "\n\n*** *** *** *** ALL TESTS COMPLETED SUCCESSFULLY *** *** *** *** \n\n"
	RESULT=0
else
	echo -e "\n\n*** *** *** *** TESTS COMPLETED WITH ERRORS *** *** *** *** \n\n"
fi
exit $RESULT
