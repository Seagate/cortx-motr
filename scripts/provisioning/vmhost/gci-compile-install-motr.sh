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

# set -eu -o pipefail ## commands tend to fail unpredictably

cd /data/motr
# ./scripts/m0 rebuild
./autogen.sh
if [ "$?" != 0 ]; then
    echo -e "\n\n\nERROR in Motr compilation; autogen.sh failed!!!\n\n\n"
    exit -1
fi

./configure
if [ "$?" != 0 ]; then
    echo -e "\n\n\nERROR in Motr compilation; ./configure failed!!!\n\n\n"
    exit -1
fi

make -j4
if [ "$?" != 0 ]; then
    echo -e "\n\n\nERROR in Motr compilation; [make] failed!!!\n\n\n"
    exit -1
fi

make rpms-notests
if [ "$?" != 0 ]; then
    echo -e "\n\n\nERROR in creating Motr RPMS!!!\n\n\n"
    exit -1
fi

sudo yum -y install /home/vagrant/rpmbuild/RPMS/x86_64/cortx-motr-1.*.rpm
if [ "$?" != 0 ]; then
    echo -e "\n\n\nERROR in installing Motr RPM!!!\n\n\n"
    exit -1
fi

sudo yum -y install /home/vagrant/rpmbuild/RPMS/x86_64/cortx-motr-devel-1.*.rpm
if [ "$?" != 0 ]; then
    echo -e "\n\n\nERROR in installing Motr-Devel RPM!!!\n\n\n"
    exit -1
fi
exit 0
