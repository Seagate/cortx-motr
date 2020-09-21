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

cd /data/hare
make
if [ "$?" != 0 ]; then
    echo -e "\n\n\nERROR in Hare compilation!!!\n\n\n"
    exit -1

fi

make rpm
if [ "$?" != 0 ]; then
    echo -e "\n\n\nERROR in creating Hare RPMS!!!\n\n\n"
    exit -1

fi

# sudo cp -rv "/root/rpmbuild/RPMS/x86_64/" /home/vagrant/rpmbuild/RPMS/x86_64/
# if [ "$?" != 0 ]; then
#     echo -e "\n\n\nERROR in copying Hare rpm!!!\n\n\n"
#     echo "TEMP_MSG -- error copying. Do manual copy"
#     read a
# fi

sudo yum -y install /home/vagrant/rpmbuild/RPMS/x86_64/cortx-hare-1.*.rpm
if [ "$?" != 0 ]; then
	ls -l /home/vagrant/rpmbuild/RPMS/x86_64/*
    echo -e "\n\n\nERROR in installing cortx-hare-0.* RPM!!!\n\n\n"
    exit -1
fi

sudo usermod -a -G hare $USER
echo -e "$USER was added to the hare group."
exit 0
