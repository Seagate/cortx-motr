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
              Default count of addb stob directories is 2
--help|-h     display this help and exit" 1>&2; 
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

# max addb stob directories count in each log directory
log_dirs_max_count=2
# have hard coded the log path, 
# Need to get it from config file 
ADDB_RECORD_DIR=$(cat /etc/sysconfig/motr  | grep "^MOTR_M0D_ADDB_STOB_DIR" | cut -d '=' -f2)
if [ -z "$ADDB_RECORD_DIR" ]; then
   ADDB_RECORD_DIR="/var/motr/m0d-*"
fi

ADDB_DIR="${ADDB_RECORD_DIR%\'}"
ADDB_DIR="${ADDB_DIR#\'}"
addb_rec_dirs=`ls -d $ADDB_DIR`
if [ -n "$ADDB_DIR" ]; then
    addb_rec_dirs="$addb_rec_dirs $ADDB_DIR"
fi

while getopts ":n:" option; do
    case "${option}" in
        n)
            log_dirs_max_count=${OPTARG}
            if [[ -z "${log_dirs_max_count}" ]]; then
              usage
            fi
            ;;
        *)
            usage
            ;;
    esac
done

log_dirs_max_count=2

echo "Max log dir count: $log_dirs_max_count"
echo "ADDB Log directory: $addb_rec_dirs"

# check for log directory entries
for addb_rec_dir in $addb_rec_dirs ; do
    [[ $(check_param $addb_rec_dir) = "continue" ]] && continue || echo "$addb_rec_dir"

    # get the log directory of each m0d instance
    log_dirs=`find $addb_rec_dir -maxdepth 1 -type d -name m0d-\*`

    [[ $(check_param $log_dirs) = "continue" ]] && continue || echo "$log_dirs"

    # iterate through all log directories of each m0d instance
    for log_dir in $log_dirs ; do
        # get the no. of stob dir count
        addb_dirs=`find $log_dir -maxdepth 1 -type d -name "addb-stobs*"`
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
            
            dirs_to_remove=`ls -tr "$log_dir" | grep addb-stobs | \
                                     head -n $remove_dir_count`

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
