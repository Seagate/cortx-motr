#!/bin/bash

# Script to set BE log size
# 1. parse hctl status output
# 2. list size of existing services log file
# 3. set new size using m0betool
# 4. list size of all io services log file

M0D_LIST_FID=""
BE_LOG_SIZE=$((256 * 1024 * 1024))

function usage()
{
   echo "be_log_resize.sh [-s <be_log_size>]"
   echo "Options:"
   echo "    '-s <size>'  BE log size to change"
   echo "Example.."
   echo "./be_log_resize.sh"
   echo "./be_log_resize.sh -s <BE log size to change>"
   exit 1
}

change_be_log_size() {
    for fid in $M0D_LIST_FID; do
    	local log_file="/var/motr/m0d-${fid}/db-log/o/0:28"
    	local db_file="/var/motr/m0d-${fid}/db"
	echo "Before setting BE log size log_file=$log_file"
	[[ -f "$log_file" ]] && ls -lsh $log_file
	echo "db_file=$db_file"
	echo "m0betool be_log_resize $db_file $BE_LOG_SIZE"
	m0betool be_log_resize $db_file $BE_LOG_SIZE
	[[ $? -ne 0 ]] && echo "Failed" && exit 1
	echo "After setting BE log size log_file=$log_file"
	[[ -f "$log_file" ]] && ls -lsh $log_file
    done
}

_local_params() {
    local addr="([0-9]+[.][0-9]+[.][0-9]+[.][0-9]+)"
    local host

    if [[ $(cat /var/lib/hare/node-name) == "srvnode-1" ]]; then
	host="srvnode-1"
    elif [[ $(cat /var/lib/hare/node-name) == "srvnode-2" ]]; then
	host="srvnode-2"
    else
	echo "cluster node srvnode-1/srvnode-2 not found" && exit 1
    fi

    STATUS=$(hctl status)
    NODE_IP=$(echo "$STATUS" | grep "$host" -A 1 | grep -E -o "$addr")
    M0D_LIST_FID=$(echo "$STATUS" | grep "${NODE_IP}" |  grep "\[.*\].*ioservice" |
                   awk '{print $3}')
}

OPTIONS_STRING="s:h"
while getopts "$OPTIONS_STRING" OPTION; do
        case "$OPTION" in
                s)
                        BE_LOG_SIZE="$OPTARG"
                        ;;
                *)
                        usage
                        exit 1
                        ;;
        esac
done

[[ -z "$BE_LOG_SIZE" ]] && usage

[[ -z $(ps uax | grep m0d) ]] && echo "m0ds not started" && exit 1

 _local_params

which m0betool > /dev/null
[[ $? -ne 0 ]] && echo "export m0betool" && exit 1

echo "BE_LOG_SIZE=$BE_LOG_SIZE"
change_be_log_size "$BE_LOG_SIZE"
[[ $? -ne 0 ]] && echo "Failed to set log size $BE_LOG_SIZE" && exit 1

echo "Successful"
