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


## Declare all variables
CL_DIR_TAG=""
CL_DIR_PATH=""
M0VG=""
CL_HOME="$HOME/virtual_clusters"
FORCE="FALSE"

destroy_cluster()
{
    local FSTART=$(date +%s); local RESULT=0
    echo -e "\nHere to destroy $CL_DIR_TAG in the $CL_DIR_PATH"
    $M0VG status
    if [ "$FORCE" == "FALSE" ]; then
        echo -en "\nYou are about to destroy virtual cluster $CL_DIR_TAG at $CL_DIR_PATH. Type [YES] in 30 seconds to continue. "
        read -t 30 CH
        if [ "$CH" != "YES" ]; then
            echo -e "\nVirtual cluster $CL_DIR_TAG at $CL_DIR_PATH was not destroyed."
            echo -e "\n\n"
            return $RESULT
        fi
        echo -e "\nThanks for confirmation"
    fi
    $M0VG destroy --force
    RESULT=$?
	echo -e "EXTRA CAUTIOUS CLEANUP"
    virsh -c qemu:///system list --all | grep "$CL_DIR_TAG"
	virsh -c qemu:///system destroy $CL_DIR_TAG"_centos77_cmu"
	virsh -c qemu:///system destroy $CL_DIR_TAG"_centos77_ssu1"
	virsh -c qemu:///system destroy $CL_DIR_TAG"_centos77_ssu2"
	virsh -c qemu:///system destroy $CL_DIR_TAG"_centos77_client1"
	virsh -c qemu:///system undefine $CL_DIR_TAG"_centos77_cmu"
	virsh -c qemu:///system undefine $CL_DIR_TAG"_centos77_ssu1"
	virsh -c qemu:///system undefine $CL_DIR_TAG"_centos77_ssu2"
	virsh -c qemu:///system undefine $CL_DIR_TAG"_centos77_client1"
	virsh -c qemu:///system list --all | grep $CL_DIR_TAG

	ls -lh /home/libvirt/images/$CL_DIR_TAG*
	rm -rf /home/libvirt/images/$CL_DIR_TAG*

	ls -lh $CL_DIR_PATH
	rm -rf $CL_DIR_PATH
    FTIME=$(( $(date +%s) - FSTART ))
    echo -e "\nCluster $CL_DIR_TAG destroyed in $FTIME seconds."
}

print_usage()
{
        echo -e "\nInvalid Args."
        echo -e "\n./destroy-cluster.sh <CLUSTER-NAME> [--force]"
        echo -e "\n\n"
}

### main()
if [ $# == 2 ]; then
    if [ "$2" == "--force" ]; then
        FORCE="TRUE"
    else
        print_usage
        exit 1
    fi
    CL_DIR_TAG=$1
elif [ $# == 1 ]; then
    CL_DIR_TAG=$1
else
    print_usage
    exit 1
fi

CL_DIR_PATH=$CL_HOME/$CL_DIR_TAG
M0VG=$CL_DIR_PATH/motr/scripts/m0vg

echo -e "\n[$FUNCNAME: $LINENO] CL_DIR_TAG [$CL_DIR_TAG]"
echo -e "\n[$FUNCNAME: $LINENO] CL_DIR_PATH [$CL_DIR_PATH]"
echo -e "\n[$FUNCNAME: $LINENO] M0VG [$M0VG]"

if [ ! -d "$CL_DIR_PATH" ]; then
    echo -e "\n[$FUNCNAME: $LINENO] Cluster Path $CL_DIR_PATH doesnot exist!!"
    exit 0
fi

destroy_cluster
exit 0
