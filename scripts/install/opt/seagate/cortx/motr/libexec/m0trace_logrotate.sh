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

# script is used to delete the old motr logs
# argument1: <number of latest log files to retain>
# Default number of latest log files is 5
# ./m0trace_logrotate.sh 5

source "/opt/seagate/cortx/motr/common/cortx_util_funcs.sh"

usage() { 
        echo "Usage: bash $(basename $0) [--help|-h]
                     [-n LogFileCount]
Retain recent modified files of given count and remove older motr log files.
where:
-n            number of latest log files to retain 
              Physical : Default count of log files are 5+2 first files
              virtual  : Default count of log files is 2
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

platform=$(get_platform)
echo "Server type is $platform"

# max log files count in each log directory
log_files_max_count=5
# have hard coded the log path, 
# Need to get it from config file 
motr_logdirs=`ls -d /var/motr*`
M0TR_M0D_TRACE_DIR=$(cat /etc/sysconfig/motr  | grep "^MOTR_M0D_TRACE_DIR" | cut -d '=' -f2)
M0D_TRACE_DIR="${M0TR_M0D_TRACE_DIR%\'}"
M0D_TRACE_DIR="${M0D_TRACE_DIR#\'}"
if [ -n "$M0D_TRACE_DIR" ]; then
    motr_logdirs="$motr_logdirs $M0D_TRACE_DIR"
fi

while getopts ":n:" option; do
    case "${option}" in
        n)
            log_files_max_count=${OPTARG}
            if [[ -z "${log_files_max_count}" ]]; then
              usage
            fi
            ;;
        *)
            usage
            ;;
    esac
done

if [[ $platform = "virtual" ]]; then
    log_files_max_count=2
else
    log_files_max_count=`expr $log_files_max_count + 2`
fi

echo "Max log file count: $log_files_max_count"
echo "Log file directory: $motr_logdirs"

# check for log directory entries
for motr_logdir in $motr_logdirs ; do
    [[ $(check_param $motr_logdir) = "continue" ]] && continue || echo "$motr_logdir"

    # get the log directory of each m0d instance
    log_dirs=`find $motr_logdir -maxdepth 1 -type d -name m0d-\*`

    [[ $(check_param $log_dirs) = "continue" ]] && continue || echo "$log_dirs"

    # iterate through all log directories of each m0d instance
    for log_dir in $log_dirs ; do
        # get the no. of log file count
        log_files=`find $log_dir -maxdepth 1 -type f -name "m0trace.*"`
        log_files_count=`echo "$log_files" | grep -v "^$" | wc -l`

        echo "## found $log_files_count file(s) in log directory $log_dir ##"

        # check log files count is greater than max log file count
        if [[ $log_files_count -gt $log_files_max_count ]]; then
            # get files sort by date - older will come on top
            remove_file_count=`expr $log_files_count - $log_files_max_count`

            echo "## ($remove_file_count) file(s) can be removed from \
                           log directory($log_dir) ##"               

            # get the files sorted by time modified 
            # (most recently modified comes last), that 
            # is older files comes first
            echo "LOG_DIR is $log_dir"
            
            if [[ $platform = "physical" ]]; then
                peserve_files=`expr $log_files_max_count - 2`
                files_to_remove=`ls -tr "$log_dir" | grep m0trace | \
                                     head -n -$peserve_files | awk 'NR>2'`
            else
                files_to_remove=`ls -tr "$log_dir" | grep m0trace | \
                                     head -n $remove_file_count`
            fi

            for file in $files_to_remove ; do
                # remove only if file not sym file
                if !  [[ -L "$log_dir/$file" ]]; then
                    echo "$log_dir/$file"
                    rm -f "$log_dir/$file"
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
