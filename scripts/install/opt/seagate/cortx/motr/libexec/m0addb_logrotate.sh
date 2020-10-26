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

# script is used to delete the old motr addb stobs
# argument1: <number of latest addb stob dir to retain>
# Default number of latest addb stob dir is 2
# ./m0addb_logrotate.sh 2

usage() { 
        echo "Usage: bash $(basename $0) [--help|-h]
                     [-n LogDirCount]
Retain recent modified directories of given count and remove older motr addb stob directories.
where:
-n            number of latest addb stob directories to retain 
              Physical : Default count of addb stob directories are 2+2 first directories
              virtual  : Default count of log dirs is 2
--help|-h     display this help and exit" 1>&2; 
        exit 1; 
}

get_platform() 
{
    plt=$(systemd-detect-virt)
 
    if [[ $plt = "none" ]]; then
        plt="physical"
    else
        plt="virtual"
    fi

    echo "$plt"
}

check_param() 
{
    PARAM=$1
    echo "PARAM: $PARAM"
    if [[ -n $PARAM ]]; then
        retval="OK"
    else
        echo "PARAM is empty"
        retval="continue"
    fi

    echo $retval
}

platform=$(get_platform)
echo "Server type is $platform"

# max addb stob directories count in each log directory
log_dirs_max_count=2
# have hard coded the log path, 
# Need to get it from config file 
motr_logdirs=`ls -d /var/motr*`

while getopts ":n:" option; do
    case "${option}" in
        n)
            log_dirs_max_count=${OPTARG}
            if [[ -z ${log_dirs_max_count} ]]; then
              usage
            fi
            ;;
        *)
            usage
            ;;
    esac
done

if [[ $platform = "virtual" ]]; then
    log_dirs_max_count=2
else
    log_dirs_max_count=`expr $log_dirs_max_count + 2`
fi

echo "Max log dir count: $log_dirs_max_count"
echo "ADDB Log directory: $motr_logdirs"

# check for log directory entries
for motr_logdir in $motr_logdirs ; do
    [[ $(check_param $motr_logdir) = "continue" ]] && continue || echo "$motr_logdir"

    # get the log directory of each m0d instance
    log_dirs=`find $motr_logdir -maxdepth 1 -type d -name m0d-\*`

    [[ $(check_param $log_dirs) = "continue" ]] && continue || echo "$log_dirs"

    # iterate through all log directories of each m0d instance
    for log_dir in $log_dirs ; do
        # get the no. of stob dir count
        addb_dirs=`find $log_dir -maxdepth 1 -type d -name "addb-stobs-*"`
        addb_dirs_count=`echo "$addb_dirs" | grep -v "^$" | wc -l`

        echo "## found $addb_dirs_count dir(s) in log directory $log_dir ##"

        # check addb stob dir count is greater than max dir  count
        if [[ $addb_dirs_count -gt $log_dirs_max_count ]]; then
            # get dirs sort by date - older will come on top
            remove_dir_count=`expr $addb_dirs_count - $log_dirs_max_count`

            echo "## ($remove_dir_count) dir(s) can be removed from \
                           log directory($log_dir) ##"               

            # get the dirs sorted by time modified 
            # (most recently modified comes last), that 
            # is older dirs comes first
            echo "LOG_DIR is $log_dir"
            
            if [[ $platform = "physical" ]]; then
                peserve_dirs=`expr $log_dirs_max_count - 2`
                dirs_to_remove=`ls -tr "$log_dir" | grep addb-stobs- | \
                                     head -n -$peserve_dirs | awk 'NR>2'`
            else
                dirs_to_remove=`ls -tr "$log_dir" | grep addb-stobs- | \
                                     head -n $remove_dir_count`
            fi

            for dir in $dirs_to_remove ; do
                # remove only if dir not sym dir
                if !  [[ -L "$log_dir/$dir" ]]; then
                    echo "$log_dir/$dir"
                    rm -rf "$log_dir/$dir"
                    if [[ $? -ne 0 ]]; then
                       echo "Unable to remove directory $dir"
                    fi
                fi
            done
        else
            echo "## No dirs to remove ##"
        fi
    done
done
