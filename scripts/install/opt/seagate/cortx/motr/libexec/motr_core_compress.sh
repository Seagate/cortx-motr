#!/usr/bin/env bash
#
# Copyright (c) 2020-2021 Seagate Technology LLC and/or its Affiliates
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

# set -x

# In k8 env m0d directories are in /etc/cortx/motr
motr_logdirs=`ls -d /etc/cortx/motr*`

for motr_logdir in $motr_logdirs; do

    # m0d_dirs contains dirs like 
    # /etc/cortx/motr/m0d-0x7200000000000001:0x19/
    # /etc/cortx/motr/m0d-0x7200000000000001:0xa
    # /etc/cortx/motr/m0d-0x7200000000000001:0x28
    m0d_dirs=`find $motr_logdir -maxdepth 1 -type d -name m0d-\*`

    # Traverse through all m0d directories
    for m0d_dir in $m0d_dirs; do
        pushd $m0d_dir

        # core_files contain all core files excluding already compressed  files.
        core_files=`find ./ \( -name "core.*" ! -iname "*.gz" \)`
        for core_file in $core_files; do
            echo "Found core file =$core_file"
            echo "Compressing core file $core_file"
            gzip $core_file
        done

        popd
    done
done

