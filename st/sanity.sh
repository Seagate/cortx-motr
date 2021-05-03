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

[[ $UID -eq 0 ]] || {
    echo 'Must be run by superuser' >&2
    exit 1
}

M0_SRC_DIR=`readlink -f $0`
M0_SRC_DIR=${M0_SRC_DIR%/*/*}

. $M0_SRC_DIR/utils/functions # report_and_exit

cd $M0_SRC_DIR

echo 'Installing Motr services'
scripts/install-motr-service -u

rm -rf /etc/motr
rm -f  /etc/sysconfig/motr
scripts/install-motr-service -l

PATH=$PATH:$M0_SRC_DIR/utils
scripts/install/opt/seagate/cortx/motr/sanity/motr_sanity.sh
rc=$?

scripts/install-motr-service -u
report_and_exit sanity $rc
