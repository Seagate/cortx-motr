#!/bin/sh
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

# script is used to delete the old core files
# argument1: <number of latest core files to retain>
# Default number of latest core files is 2
# ./core_logrotate.sh -n 5

source "/opt/seagate/cortx/motr/common/cortx_util_funcs.sh"

usage() { 
        echo "Usage: bash $(basename "$0") [--help|-h] [-n CoreFileCount]"
        "Retain recent modified files of given count and remove older core files."
        "where:"
        "-n   number of latest core files to retain" 
              "Physical : Default count of core files are 5+2 first files"
              "virtual  : Default count of core files is 2"
        "--help|-h     display this help and exit" 1>&2; 
        exit 1; 
}

check_param() 
{
    PARAM=$1
    echo "PARAM: $PARAM"
    if [[ -n "$PARAM" ]]; then
        retval="OK"
    else
        echo "PARAM is empty"
        retval="continue"
    fi

    echo $retval
}

platform=$(get_platform)
echo "Server type is $platform"

# max core files count in each m0d directory
core_files_max_count=5

motr_coredirs=("/var/crash/" "/var/log/crash/")

MACHINE_ID=$(cat /etc/machine-id)
if [ -d "/etc/cortx/motr/sysconfig/$MACHINE_ID" ]; then
    MOTR_SYSCONFIG="/etc/cortx/motr/sysconfig/$MACHINE_ID/motr"
else
    MOTR_SYSCONFIG="/etc/sysconfig/motr"
fi
M0TR_M0D_CORE_DIR=$(source "$MOTR_SYSCONFIG" ; echo "$MOTR_M0D_DATA_DIR")
if [ -n "$M0TR_M0D_CORE_DIR" ]; then
    motr_coredirs+=("$M0TR_M0D_CORE_DIR")
fi

while getopts ":n:" option; do
    case "${option}" in
        n)
            core_files_max_count=${OPTARG}
            if [[ -z "${core_files_max_count}" ]]; then
              usage
            fi
            ;;
        *)
            usage
            ;;
    esac
done

if [[ $platform = "virtual" && $core_files_max_count -gt 2 ]]; then
    core_files_max_count=2
fi

echo "Max core file count: $core_files_max_count"
echo "Core file directory: $motr_coredirs"

# check for core directory entries
for motr_coredir in ${motr_coredirs[@]}; do
    [[ -d $motr_coredir ]] || {
        echo "$motr_coredir: no such directory"
        continue
    }

    [[ $(check_param "$motr_coredir") = "continue" ]] && continue || echo "$motr_coredir"

    # get the core directory of each m0d instance
    core_dirs=$(find "$motr_coredir" -maxdepth 1 -type d -name m0d-\*)

    [[ $(check_param "$core_dirs") = "continue" ]] && continue || echo "$core_dirs"

    # iterate through all core directories of each m0d instance
    for core_dir in $core_dirs ; do
        # get the no. of core file count
        core_files=$(find "$core_dir" -maxdepth 1 -type f -name "core.*")
        core_files_count=$(echo "$core_files" | grep -v "^$" | wc -l)

        echo "## found $core_files_count file(s) in core directory $core_dir ##"

        # check core files count is greater than max core file count
        if [[ "$core_files_count" -gt "$core_files_max_count" ]]; then
            # get files sort by date - older will come on top
            remove_file_count=$((core_files_count - core_files_max_count))

            echo "## ($remove_file_count) file(s) can be removed from \
                           core directory($core_dir) ##"

            # get the files sorted by time modified 
            # (most recently modified comes last), that 
            # is older files comes first
            
            files_to_remove=$(ls -tr "$core_dir" | grep core | \
                                     head -n "$remove_file_count")

            for file in $files_to_remove ; do
                # remove only if file not sym file
                if !  [[ -L "$core_dir/$file" ]]; then
                    echo "$core_dir/$file"
                    rm -f "$core_dir/$file"
                    if [[ $? -ne 0 ]]; then
                       echo "Unable to remove file $file"
                    fi
                fi
            done
        else
            echo "## No files to remove ##"
        fi
    done
done
