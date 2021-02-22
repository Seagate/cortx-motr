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
SCRIPT_START_TIME="$(date +"%s")"
PROG=${0##*/}
# Creating the log file under /var/log/seagate/motr
now=$(date +"%Y_%m_%d__%H_%M_%S")
LOG_DIR="/var/log/seagate/motr/datarecovery"
mkdir -p $LOG_DIR
LOG_FILE="$LOG_DIR/motr_data_recovery_$now.log"
touch $LOG_FILE
# Current directory where motr_recovery.sh/m0dr is present [ scripts/beck/ ]
SRC_DIR="$(dirname $(readlink -f $0))"
# cortx-motr main dir path
M0_SRC_DIR="${SRC_DIR%/*/*}"
MD_DIR="/var/motr" # Meta Data Directory
CRASH_DIR="/var/log/crash" #Crash Directory
# beck utility path, update this path in get_utility_path() to change path
BECKTOOL=
# m0betool utility path, update this path in get_utility_path() to change path
M0BETOOL=
# Path for metadata device of local node
LOCAL_MOTR_DEVICE=
# Path for metadata device of remote node
REMOTE_MOTR_DEVICE=
# Volumegroup name for local node
LOCAL_MD_VOLUMEGROUP=
# Volumegroup name for remote node
REMOTE_MD_VOLUMEGROUP=
# swap and metadata device names on both nodes
SWAP_DEVICE="lv_main_swap"
MD_DEVICE="lv_raw_metadata"
# Path for the swap and metadata volumes of local node
LOCAL_LV_SWAP_DEVICE=
LOCAL_LV_MD_DEVICE=
# Path for the swap and metadata volumes of remote node
REMOTE_LV_SWAP_DEVICE=
REMOTE_LV_MD_DEVICE=
SNAPSHOT="MD_Snapshot" # Snapshot name
LOCAL_NODE=            # Local node hostname
REMOTE_NODE=           # Remote node hostname
LOCAL_IOS_FID=""         # Local ioservice fid
REMOTE_IOS_FID=""       # Remote ioservice fid
# REMOTE_STORAGE_STATUS is used to determine remote storage is accessibility,
# If value is 0 then remote storage is accessible on remote node,
# If value 1 then remote storage is accessible via local node.
REMOTE_STORAGE_STATUS=0
LOCAL_NODE_RECOVERY_STATE=0  # To determine the recovery state of local node
REMOTE_NODE_RECOVERY_STATE=0 # To determine the recovery state of remote node
# Path to dir on local node where we can access remote metadata dir if remote node failed
FAILOVER_MD_DIR=
beck_only=false # Run only becktool in the script
clean_snapshot=false # Removes MD_Snapshot if present and create lv_main_swap again
ESEGV=134 # Error code for segment fault
RSTATE0=0 # We do not need to perform any actions before starting recovery
RSTATE2=2 # We need to do only create snapshot before starting recovery
RSTATE3=3 # We need to do all operations such as fsck, replay logs, create snapshot before starting recovery
CDF_FILENAME="/var/lib/hare/cluster.yaml" # Cluster defination file used by prov-m0-reset
# HA conf argument file needed by prov-m0-reset script
HA_ARGS_FILENAME="/opt/seagate/cortx/iostack-ha/conf/build-ha-args.yaml"
SINGLE_NODE_RUNNING=  # Will be set if only one node is running.
LOCAL_SEG_GEN_ID=0
REMOTE_SEG_GEN_ID=0

# Add path to utility m0-prov-reset
PATH=$PATH:/opt/seagate/cortx/ha/conf/script/:.

usage() {
    cat <<EOF

Usage: $PROG [--option]
Master script for the motr data recovery.

Optional parameters.
    --beck-only      Performs only beck utility. Use this option only when snapshot
                     is already present on system.
    --clean-snapshot Removes MD_Snapshot if present and create lv_main_swap again
    -h, --help       Shows this help text and exit.

***Exit codes used by functions in script*** :
 - If a function is executed successfully on both local node
   and remote node then function will return 0.
 - If a function fails only on local node then function will return 1.
 - If a function fails only on remote node then function will return 2.
 - If a function fails on both local and remote node then function will return 3.
EOF
}

TEMP=$(getopt --options h \
              --longoptions help \
              --longoptions beck-only \
              --longoptions clean-snapshot \
              --name "$PROG" -- "$@" || true)

(($? == 0)) || { echo "show usage"; exit 1; }

eval set -- "$TEMP"

while true; do
    case "$1" in
        -h|--help)           usage; exit ;;
        --beck-only)         beck_only=true; shift ;;
        --clean-snapshot)    clean_snapshot=true; shift ;;
        --)                  shift; break ;;
        *)                   break ;;
    esac
done

m0drlog() {
    echo "[`date`]: $@" 2>&1 | tee -a ${LOG_FILE}
}

die() { m0drlog "$@" ; exit 1; }

# Check if the current user is root user or not
# return 1 if not a root user
is_user_root_user() {
    if [ "$EUID" -ne 0 ]; then
        return 1 # not a root user
    fi
}

# The function will return 0 if ioservice is started on local node,
# else this will return 1
is_ios_running_on_local_node() {
    if (hctl status | grep -i ioservice | grep $LOCAL_IOS_FID | grep -i started); then
        return 0
    else
        return 1
    fi
}

# The function will return 0 if ioservice is started on remote node,
# else this will return 1
is_ios_running_on_remote_node() {
    if (hctl status | grep -i ioservice | grep $REMOTE_IOS_FID | grep -i started); then
        return 0
    else
        return 1
    fi
}

# This function will initialize values of local and remote ioservices
# fid variable.
get_ios_fid() {
    LOCAL_IOS_FID=$(cat  /etc/sysconfig/m0d-0x7200000000000001\:0x* | grep "$LOCAL_NODE" -B 1 | grep FID | cut -f 2 -d "="| tr -d \')
    REMOTE_IOS_FID=$(cat  /etc/sysconfig/m0d-0x7200000000000001\:0x* | grep "$REMOTE_NODE" -B 1 | grep FID | cut -f 2 -d "="| tr -d \')

    if [[ $LOCAL_IOS_FID == "" ]] || [[ $REMOTE_IOS_FID == "" ]];then
        die "Failed to get ioservice FIDs."
    fi

}

# The function will execute command on the local node
run_cmd_on_local_node() {
    local cmd=$1
    echo "[`date`]: Running '$cmd'" >> ${LOG_FILE}
    eval "$cmd" 2>&1 | tee -a ${LOG_FILE} # execute the command on current/local node
    return ${PIPESTATUS[0]} # return exit status of first command
}

# The function will execute command on the remote node
run_cmd_on_remote_node() {
    local cmd=$1
    echo "[`date`]: Running 'ssh root@$REMOTE_NODE $cmd'" >> ${LOG_FILE}
    ssh root@$REMOTE_NODE "$cmd" 2>&1 | tee -a ${LOG_FILE} # execute the command
    return ${PIPESTATUS[0]} # return exit status of first command
}

# This function will initialize paths for the m0beck and m0betool utility
get_utility_path() {
    if [[ -f $M0_SRC_DIR/be/tool/m0beck ]]; then
        BECKTOOL=$M0_SRC_DIR/be/tool/m0beck
    else
        # use environmental path for utility e.g. /sbin/m0beck
        BECKTOOL="m0beck"
    fi

    if [[ -f $M0_SRC_DIR/be/tool/m0betool ]]; then
        M0BETOOL=$M0_SRC_DIR/be/tool/m0betool
    else
        # use environmental path for utility e.g. /sbin/m0betool
        M0BETOOL="m0betool"
    fi
}

# The recovery state for local node in this function is based on the snapshot
# presence as follows:
# Recovery state is 3 if snapshot is not present and swap device is present then
# all steps needed such as run_fsck, replay_logs_and_get_gen_id_of_seg0, remove_swap
# Recovery state is 2 if snapshot is not present and swap is not present then
# only create_snapshot is needed
# Recovery state is 0 if snapshot is already present
get_recovery_state_of_local_node() {
    local state=0

    test="$(lvs -o name $LOCAL_MD_VOLUMEGROUP | grep $SNAPSHOT | awk '{ print $1}')"
    if [ "$test" != "$SNAPSHOT" ]; then
        test="$(lvs -o name $LOCAL_MD_VOLUMEGROUP | grep $SWAP_DEVICE |  awk '{print $1}')"
        if [ "$test" = "$SWAP_DEVICE" ]; then
            MD_SIZE="$(lvs -o lvname,size $LOCAL_MD_VOLUMEGROUP --units K --nosuffix | \
                        grep $MD_DEVICE | awk '{ print $2 }')"

            SWAP_SIZE="$(lvs -o lvname,size $LOCAL_MD_VOLUMEGROUP --units K --nosuffix| \
                        grep $SWAP_DEVICE | awk '{ print  $2 }')"

            # If the comparison is true then bc return 1 otherwise return 0
            if [ $(bc <<< "$MD_SIZE > $SWAP_SIZE") = 1 ]; then
                die "ERROR: Metadata size is not less than swap size on local node !!"
            fi
            (( state|=1 )); # run_fsck, replay_logs_and_get_gen_id_of_seg0, remove_swap
        fi
        (( state|=2 ));     # create snapshot
    fi

    LOCAL_NODE_RECOVERY_STATE=$state
    m0drlog "Local Node Recovery State is $LOCAL_NODE_RECOVERY_STATE"
}

# The recovery state for remote node in this function is based on the snapshot
# presence as follows:
# Recovery state is 3 if snapshot is not present and swap device is present then
# all steps needed such as run_fsck, replay_logs_and_get_gen_id_of_seg0, remove_swap, create_snapshot
# Recovery state is 2 if snapshot is not present and swap is not present then
# only create_snapshot is needed
# Recovery state is 0 if snapshot is already present
get_recovery_state_of_remote_node() {
    local state=0
    [[ $REMOTE_STORAGE_STATUS == 0 ]] && \
    test=$(run_cmd_on_remote_node "lvs -o name $REMOTE_MD_VOLUMEGROUP | grep $SNAPSHOT | awk '{ print \$1}'") || \
    test="$(lvs -o name $REMOTE_MD_VOLUMEGROUP | grep $SNAPSHOT | awk '{ print $1}')"

    if [ "$test" != "$SNAPSHOT" ]; then
        [[ $REMOTE_STORAGE_STATUS == 0 ]] && \
        test=$(run_cmd_on_remote_node "lvs -o name $REMOTE_MD_VOLUMEGROUP | grep $SWAP_DEVICE | awk '{ print \$1}'") || \
        test="$(lvs -o name $REMOTE_MD_VOLUMEGROUP | grep $SWAP_DEVICE | awk '{ print $1}')"

        if [ "$test" = "$SWAP_DEVICE" ]; then
            [[ $REMOTE_STORAGE_STATUS == 0 ]] && \
            MD_SIZE=$(run_cmd_on_remote_node "lvs -o lvname,size $REMOTE_MD_VOLUMEGROUP --units K --nosuffix | \
                        grep $MD_DEVICE | awk '{ print \$2 }'") || \
            MD_SIZE="$(lvs -o lvname,size $REMOTE_MD_VOLUMEGROUP --units K --nosuffix | \
                        grep $MD_DEVICE | awk '{ print $2 }')"

            [[ $REMOTE_STORAGE_STATUS == 0 ]] && \
            SWAP_SIZE=$(run_cmd_on_remote_node "lvs -o lvname,size $REMOTE_MD_VOLUMEGROUP --units K --nosuffix| \
                        grep $SWAP_DEVICE | awk '{ print  \$2 }'") || \
            SWAP_SIZE="$(lvs -o lvname,size $REMOTE_MD_VOLUMEGROUP --units K --nosuffix| \
                        grep $SWAP_DEVICE | awk '{ print  $2 }')"


            # If the comparison is true then bc return 1 otherwise return 0
            if [ $(bc <<< "$MD_SIZE > $SWAP_SIZE") = 1 ]; then
                die "ERROR: Metadata size is not less than swap size on remote node !!"
            fi
            (( state|=1 )); # run_fsck, replay_logs_and_get_gen_id_of_seg0, remove_swap
        fi
        (( state|=2 ));     # create snapshot
    fi

    REMOTE_NODE_RECOVERY_STATE=$state
    m0drlog "Remote Node Recovery State is $REMOTE_NODE_RECOVERY_STATE"
}

# This function will initialize the variables in this script according to
# cluster configuration, check ios status, initialize the globals based on
# current node and analyse snapshot presence on the both nodes.
# Assumed that cluster is running at this point.
get_cluster_configuration() {

    run_cmd_on_local_node "pvscan --cache"; # Ensure that volumes configs are synced
    # Set global variables depending on which node you are on
    if [[ $(cat /var/lib/hare/node-name) == "srvnode-1" ]]; then

        LOCAL_NODE="srvnode-1"
        REMOTE_NODE="srvnode-2"
        # Path for metadata device for local node
        LOCAL_MOTR_DEVICE=$(grep left-volume $HA_ARGS_FILENAME |\
                            awk '{ print $2 }')
        # Path for metadata device for remote node
        REMOTE_MOTR_DEVICE=$(grep right-volume $HA_ARGS_FILENAME |\
                            awk '{ print $2 }')

        FAILOVER_MD_DIR="/var/motr2"

    elif [[ $(cat /var/lib/hare/node-name) == "srvnode-2" ]]; then

        LOCAL_NODE="srvnode-2"
        REMOTE_NODE="srvnode-1"
        # Path for metadata device for local node
        LOCAL_MOTR_DEVICE=$(grep right-volume $HA_ARGS_FILENAME |\
                            awk '{ print $2 }')
        # Path for metadata device for remote node
        REMOTE_MOTR_DEVICE=$(grep left-volume $HA_ARGS_FILENAME |\
                            awk '{ print $2 }')

        FAILOVER_MD_DIR="/var/motr1"
    fi

    LOCAL_MD_VOLUMEGROUP="vg_metadata_"$LOCAL_NODE
    REMOTE_MD_VOLUMEGROUP="vg_metadata_"$REMOTE_NODE

    LOCAL_LV_SWAP_DEVICE="$(lvs -o path $LOCAL_MD_VOLUMEGROUP | grep $SWAP_DEVICE | awk '{ print $1 }')"
    LOCAL_LV_MD_DEVICE="$(lvs -o path $LOCAL_MD_VOLUMEGROUP | grep $MD_DEVICE | awk '{ print $1 }' )"

    get_ios_fid
    get_utility_path

    # Check the ios health status and verify that storage is accessible or not
    if is_ios_running_on_local_node; then
        m0drlog "Proceeding to perform recovery of storage on local node."
    elif run_cmd_on_local_node "dd if=$LOCAL_MOTR_DEVICE of=/dev/null bs=4096 count=1"; then
        m0drlog "Local ioservice is not running, but we can access the local storage, performing recovery of storage on local node"
    else
        # local ios is failed, can't access the local storage recovery not possible on this node
        die "***ERROR: Local ioservice is not running and cannot access the local storage \
            recovery not possible for the storage of local node, please try to run recovery from $REMOTE_NODE***"
    fi

    if is_ios_running_on_remote_node; then
        m0drlog "Proceeding to perform recovery on remote node."
        run_cmd_on_remote_node "pvscan --cache"; # Ensure that volumes configs are synced
        REMOTE_LV_SWAP_DEVICE=$(run_cmd_on_remote_node "lvs -o path $REMOTE_MD_VOLUMEGROUP | grep $SWAP_DEVICE | awk '{ print \$1 }'" )
        REMOTE_LV_MD_DEVICE=$(run_cmd_on_remote_node "lvs -o path $REMOTE_MD_VOLUMEGROUP | grep $MD_DEVICE | awk '{ print \$1 }'" )
        REMOTE_STORAGE_STATUS=0 # We can run commands on remote node
    elif run_cmd_on_remote_node "dd if=$REMOTE_MOTR_DEVICE of=/dev/null bs=4096 count=1"; then
        m0drlog "Remote ioservice is not running, but we can access the storage on remote node, \
                performing recovery of storage on remote node"
        run_cmd_on_remote_node "pvscan --cache"; # Ensure that volumes configs are synced
        REMOTE_LV_SWAP_DEVICE=$(run_cmd_on_remote_node "lvs -o path $REMOTE_MD_VOLUMEGROUP | grep $SWAP_DEVICE | awk '{ print \$1 }'" )
        REMOTE_LV_MD_DEVICE=$(run_cmd_on_remote_node "lvs -o path $REMOTE_MD_VOLUMEGROUP | grep $MD_DEVICE | awk '{ print \$1 }'" )
        REMOTE_STORAGE_STATUS=0
    elif run_cmd_on_local_node "dd if=$REMOTE_MOTR_DEVICE of=/dev/null bs=4096 count=1"; then
        m0drlog "Remote ioservice is not running and remote node is not reachable, \
                but can access remote storage through local node, performing recovery of storage on remote node, from local node"
        REMOTE_STORAGE_STATUS=1
        REMOTE_LV_SWAP_DEVICE="$(lvs -o path $REMOTE_MD_VOLUMEGROUP | grep $SWAP_DEVICE | awk '{ print $1 }' )"
        REMOTE_LV_MD_DEVICE="$(lvs -o path $REMOTE_MD_VOLUMEGROUP | grep $MD_DEVICE | awk '{ print $1 }')"
        SINGLE_NODE_RUNNING='--single-node'

    else
        # remote ios is failed, can't access the remote storage, recovery not possible on this node
        die "***ERROR: Remote ioservice is not running and cannot access the remote storage, \
            recovery not possible for storage of the remote node***"
    fi

    # Analyse snapshot presence on the both nodes
    get_recovery_state_of_local_node
    get_recovery_state_of_remote_node
}

# This function shutdown the cluster with all services on both nodes
shutdown_services() {
    m0drlog "Stopping cluster, this may take few minutes"
    pcs cluster stop --all
    sleep 10
    m0drlog "Cluster is stoppped"
    if pcs status; then # cluster is still running
        die "Cluster did not shutdown correctly. Please try again."
    fi
}

# This function return 0 if all steps in function run successfully
# else this will return 1
reinit_mkfs() {

    # Unmount the /var/motr if mounted on both nodes
    if mountpoint -q $MD_DIR; then
        if ! umount $MD_DIR; then
            die "MD Device might be busy on local node, please try shutting \
                down cluster and retry"
        fi
    fi

    if [[ $REMOTE_STORAGE_STATUS == 0 ]]; then
        if run_cmd_on_remote_node "mountpoint -q $MD_DIR"; then
            if ! run_cmd_on_remote_node "umount $MD_DIR"; then
                die "MD Device might be busy on remote node, please try shutting \
                    down cluster and retry"
            fi
        fi
    else
        if run_cmd_on_local_node "mountpoint -q $FAILOVER_MD_DIR"; then
            if ! run_cmd_on_local_node "umount $FAILOVER_MD_DIR"; then
                die "MD Device of remote node might be busy on local node, please try shutting \
                    down cluster and retry"
            fi
        fi
    fi

    # command line for reinit cluster with mkfs
    m0drlog "Reinitializing the cluster with mkfs, Please wait may take few minutes"
    run_cmd_on_local_node "set M0_RESET_LOG_FILE=$LOG_FILE; prov-m0-reset $CDF_FILENAME $HA_ARGS_FILENAME --mkfs-only $SINGLE_NODE_RUNNING; unset M0_RESET_LOG_FILE"
    [[ $? -eq 0 ]] || die "***ERROR: Cluster reinitialization with mkfs failed***"
    m0drlog "Cluster reinitialization with mkfs is completed"
    sleep 3
}

# ***Return codes used by following functions*** :
# - If a command/part of code executed successfully on both local node
#   and remote node then function will return 0.
# - If a command/part of code fails only on local node then function
#   will return 1.
# - If a command/part of code fails only on remote node then function
#   will return 2.
# - If a command/part of code fails on both local and remote node then function
#   will return 3.

# This function will replay the logs on both nodes.
# This function will also get the generation id from segment 0.
# If mount has failed after fsck then logs cannot be replayed.
# arg passed to this function is exit status of run_fsck()
replay_logs_and_get_gen_id_of_seg0() {
    local exec_status=0

    # m0betool and m0beck depend on motr-kernel service so we try to get service up;
    # if this service does not start in 3 attempt on local node and remote node
    # then we cannot proceed further in the recovery.
    if [[ $LOCAL_NODE_RECOVERY_STATE == $RSTATE3 ]]; then
        cnt=0
        while [[ $cnt != 3 ]];do
            systemctl start motr-kernel.service; exit_code=$?;
            [[ $exit_code == 0 ]] && break; (( cnt+=1 ));
        done
        sleep 10
        [[ `systemctl is-active motr-kernel.service` == "active" ]] || \
        die "motr-kernel service is not starting on local node, cannot proceed recovery further."
    fi

    if [[ $REMOTE_STORAGE_STATUS == 0 ]] && [[ $REMOTE_NODE_RECOVERY_STATE == $RSTATE3 ]]; then
        cnt=0
        while [[ $cnt != 3 ]];do
            run_cmd_on_remote_node "systemctl start motr-kernel.service"; exit_code=$?;
            [[ $exit_code == 0 ]] && break; (( cnt+=1 ));
        done
        sleep 10
        if ! run_cmd_on_remote_node "[[ \`systemctl is-active motr-kernel.service\` == 'active' ]]";then
            die "motr-kernel service is not starting on remote node, cannot proceed recovery further."
        fi
    fi

    # Check if the fsck is passed on both local node successfully or not
    # Here arg1 value as 0 states fsck passed on local and remote node
    # Here arg1 value as 2 states fsck failed on remote node but passed on local node
    if [[ $LOCAL_NODE_RECOVERY_STATE == $RSTATE3 ]]; then
        if [[ $1 == 0 || $1 == 2 ]]; then
            m0drlog "Replaying journal logs on local node"
            if run_cmd_on_local_node "(cd $MD_DIR/datarecovery; $M0BETOOL \
                be_recovery_run $MD_DIR/m0d-$LOCAL_IOS_FID/db;)"; then
                m0drlog "Journal logs replayed successfully on local node"
            else
                m0drlog "ERROR: Journal logs replay failed on local node"
                (( exec_status|=1));
            fi

            m0drlog "Get generation id of local node from segment 1"
            LOCAL_SEG_GEN_ID="$($BECKTOOL -s $MD_DIR/m0d-$LOCAL_IOS_FID/db/o/100000000000000:2a -p)"
            LOCAL_SEG_GEN_ID=$(echo $LOCAL_SEG_GEN_ID | cut -d "(" -f2 | cut -d ")" -f1)

            if [[ $LOCAL_SEG_GEN_ID -eq 0 ]]; then
                m0drlog "Get generation id of local node from segment 0"
                LOCAL_SEG_GEN_ID="$($BECKTOOL -s $MD_DIR/m0d-$LOCAL_IOS_FID/db/o/100000000000000:29 -p)"
                LOCAL_SEG_GEN_ID=$(echo $LOCAL_SEG_GEN_ID | cut -d "(" -f2 | cut -d ")" -f1)
            fi
            echo "segment genid : ($LOCAL_SEG_GEN_ID)"  >  $MD_DIR/m0d-$LOCAL_IOS_FID/gen_id
        else
            m0drlog "ERROR: Mount failed! Can't replay journal logs on local node..."
            (( exec_status|=1));
        fi
    else
        if run_cmd_on_local_node "[[ -f $MD_DIR/m0d-$LOCAL_IOS_FID/gen_id ]]"; then
            LOCAL_SEG_GEN_ID=$(run_cmd_on_local_node  "cat $MD_DIR/m0d-$LOCAL_IOS_FID/gen_id | grep 'segment genid'")
            LOCAL_SEG_GEN_ID=$(echo $LOCAL_SEG_GEN_ID | cut -d "(" -f2 | cut -d ")" -f1)
        fi
    fi
    # Check if the fsck is passed on remote node successfully or not
    # Here arg1 value as 0, states fsck passed on both local and remote node
    # Here arg1 value as 1, states fsck failed on local node but passed on remote node
    if [[ $REMOTE_STORAGE_STATUS == 0 ]] && [[ $REMOTE_NODE_RECOVERY_STATE == $RSTATE3 ]]; then
        if [[ $1 == 0 || $1 == 1 ]]; then
            m0drlog "Replaying journal logs on remote node"
            if run_cmd_on_remote_node "(cd $MD_DIR/datarecovery; $M0BETOOL \
                be_recovery_run $MD_DIR/m0d-$REMOTE_IOS_FID/db;)"; then
                m0drlog "Journal logs replayed successfully on remote node"
            else
                m0drlog "ERROR: Journal logs replay failed on remote node"
                (( exec_status|=2));
            fi

            m0drlog "Get generation id of remote node from segment 1"
            REMOTE_SEG_GEN_ID=$(run_cmd_on_remote_node "$BECKTOOL -s $MD_DIR/m0d-$REMOTE_IOS_FID/db/o/100000000000000:2a -p")
            REMOTE_SEG_GEN_ID=$(echo $REMOTE_SEG_GEN_ID | cut -d "(" -f2 | cut -d ")" -f1)

            if [[ $REMOTE_SEG_GEN_ID -eq 0 ]]; then
                  m0drlog "Get generation id of remote node from segment 0"
                  REMOTE_SEG_GEN_ID=$(run_cmd_on_remote_node "$BECKTOOL -s $MD_DIR/m0d-$REMOTE_IOS_FID/db/o/100000000000000:29 -p")
                  REMOTE_SEG_GEN_ID=$(echo $REMOTE_SEG_GEN_ID | cut -d "(" -f2 | cut -d ")" -f1)
            fi

            run_cmd_on_remote_node "echo \"segment genid : ($REMOTE_SEG_GEN_ID)\"  > $MD_DIR/m0d-$REMOTE_IOS_FID/gen_id"
        else
            m0drlog "ERROR: Mount failed! Can't replay journal logs on remote node..."
            (( exec_status|=2));
        fi
    elif [[ $REMOTE_NODE_RECOVERY_STATE == $RSTATE3 ]]; then
        if [[ $1 == 0 || $1 == 1 ]]; then
            m0drlog "Replaying journal logs for remote node from local node"
            if run_cmd_on_local_node "(cd $FAILOVER_MD_DIR/datarecovery; $M0BETOOL \
                be_recovery_run $FAILOVER_MD_DIR/m0d-$REMOTE_IOS_FID/db;)"; then
                m0drlog "Journal logs replayed successfully for remote node from local node"
            else
                m0drlog "ERROR: Journal logs replay failed for remote node"
                (( exec_status|=2));
            fi

            m0drlog "Get generation id of remote node from segment 1"
            REMOTE_SEG_GEN_ID="$($BECKTOOL -s $FAILOVER_MD_DIR/m0d-$REMOTE_IOS_FID/db/o/100000000000000:2a -p)"
            REMOTE_SEG_GEN_ID=$(echo $REMOTE_SEG_GEN_ID | cut -d "(" -f2 | cut -d ")" -f1)
            if [[ $REMOTE_SEG_GEN_ID -eq 0 ]]; then
                m0drlog "Get generation id of remote node from segment 0"
                REMOTE_SEG_GEN_ID="$($BECKTOOL -s $FAILOVER_MD_DIR/m0d-$REMOTE_IOS_FID/db/o/100000000000000:29 -p)"
                REMOTE_SEG_GEN_ID=$(echo $REMOTE_SEG_GEN_ID | cut -d "(" -f2 | cut -d ")" -f1)
            fi
            echo "segment genid : ($REMOTE_SEG_GEN_ID)"  >  $FAILOVER_MD_DIR/m0d-$REMOTE_IOS_FID/gen_id
        else
            m0drlog "ERROR: Mount failed! Can't replay journal logs on remote node..."
            (( exec_status|=2));
        fi
    elif [[ $REMOTE_STORAGE_STATUS == 0 ]]; then
        if run_cmd_on_remote_node "[[ -f $MD_DIR/m0d-$REMOTE_IOS_FID/gen_id ]]"; then
            REMOTE_SEG_GEN_ID=$(run_cmd_on_remote_node  "cat $MD_DIR/m0d-$REMOTE_IOS_FID/gen_id | grep 'segment genid'")
            REMOTE_SEG_GEN_ID=$(echo $REMOTE_SEG_GEN_ID | cut -d "(" -f2 | cut -d ")" -f1)
        fi
    else
        if run_cmd_on_local_node "[[ -f $FAILOVER_MD_DIR/m0d-$REMOTE_IOS_FID/gen_id ]]"; then
            REMOTE_SEG_GEN_ID=$(run_cmd_on_local_node  "cat $FAILOVER_MD_DIR/m0d-$REMOTE_IOS_FID/gen_id | grep 'segment genid'")
            REMOTE_SEG_GEN_ID=$(echo $REMOTE_SEG_GEN_ID | cut -d "(" -f2 | cut -d ")" -f1)
        fi
    fi

    [[ $LOCAL_SEG_GEN_ID  -eq 0 ]] && [[ $REMOTE_SEG_GEN_ID -eq 0 ]] && die "Segment header not found"
    [[ $LOCAL_SEG_GEN_ID  -ne 0 ]] || LOCAL_SEG_GEN_ID=$REMOTE_SEG_GEN_ID
    [[ $REMOTE_SEG_GEN_ID -ne 0 ]] || REMOTE_SEG_GEN_ID=$LOCAL_SEG_GEN_ID
    return $exec_status
}

# This function unmount the Metadata device (if it is already mounted) and run fsck tool on it.
# After fsck, if mount has failed for Metadata Device then then we format
# Metadata device with mkfs.ext4 and mount it again.
run_fsck() {
    local exec_status=0

    if mountpoint -q $MD_DIR; then
        if ! umount $MD_DIR; then
            die "MD Device might be busy on local node, please try shutting \
                down cluster and retry"
        fi
        m0drlog "Running fsck on local node"
        run_cmd_on_local_node "timeout 5m fsck -y $LOCAL_MOTR_DEVICE"
        [[ $? -eq 0 ]] || m0drlog "ERROR: fsck command failed on local node"
    else
        m0drlog "MD device is not mounted on local node. Running fsck on local node."
        run_cmd_on_local_node "timeout 5m fsck -y $LOCAL_MOTR_DEVICE"
        [[ $? -eq 0 ]] || m0drlog "ERROR: fsck command failed on local node"
    fi

    if [[ $REMOTE_STORAGE_STATUS == 0 ]]; then
        if run_cmd_on_remote_node "mountpoint -q $MD_DIR"; then
            if ! run_cmd_on_remote_node "umount $MD_DIR"; then
                die "MD Device might be busy on remote node, please try shutting \
                    down cluster and retry"
            fi
            m0drlog "Running fsck on remote node"
            run_cmd_on_remote_node "timeout 5m fsck -y $REMOTE_MOTR_DEVICE"
            [[ $? -eq 0 ]] || m0drlog "ERROR : fsck command failed on remote node"
        else
            m0drlog "MD device is not mounted on remote node. Running fsck on remote node."
            run_cmd_on_remote_node "timeout 5m fsck -y $REMOTE_MOTR_DEVICE"
            [[ $? -eq 0 ]] || m0drlog "ERROR : fsck command failed on remote node"
        fi
     else
        if run_cmd_on_local_node "mountpoint -q $FAILOVER_MD_DIR"; then
            if ! run_cmd_on_local_node "umount $FAILOVER_MD_DIR"; then
                die "MD Device of remote node might be busy on local node, please try shutting \
                    down cluster and retry"
            fi
            m0drlog "Running fsck for remote node from local node."
            run_cmd_on_local_node "timeout 5m fsck -y $REMOTE_MOTR_DEVICE"
            [[ $? -eq 0 ]] || m0drlog "ERROR : fsck command failed for remote node"
        else
            m0drlog "MD device of remote not mounted on local node. Running fsck for remote node from local node."
            run_cmd_on_local_node "timeout 5m fsck -y $REMOTE_MOTR_DEVICE"
            [[ $? -eq 0 ]] || m0drlog "ERROR : fsck command failed for remote node"
        fi
    fi

    # Try to mount the metadata device on /var/motr on both nodes
    if run_cmd_on_local_node "mount $LOCAL_MOTR_DEVICE $MD_DIR"; then
        # create directory "datarecovery" in /var/motr if not exist to store traces
        run_cmd_on_local_node "mkdir -p $MD_DIR/datarecovery"
    else
        run_cmd_on_local_node "mkfs.ext4 $LOCAL_MOTR_DEVICE"
        run_cmd_on_local_node "mount $LOCAL_MOTR_DEVICE $MD_DIR"
        (( exec_status|=1));
    fi

    if [[ $REMOTE_STORAGE_STATUS == 0 ]] ; then
        if run_cmd_on_remote_node "mount $REMOTE_MOTR_DEVICE $MD_DIR"; then
            # create directory "datarecovery" in /var/motr if not exist to store traces
            run_cmd_on_remote_node "mkdir -p $MD_DIR/datarecovery"
        else
            run_cmd_on_remote_node "mkfs.ext4 $REMOTE_MOTR_DEVICE"
            run_cmd_on_remote_node "mount $REMOTE_MOTR_DEVICE $MD_DIR"
            (( exec_status|=2));
        fi
    else
        if run_cmd_on_local_node "mount $REMOTE_MOTR_DEVICE $FAILOVER_MD_DIR"; then
            # create directory "datarecovery" in /var/motr if not exist to store traces
            run_cmd_on_local_node "mkdir -p $FAILOVER_MD_DIR/datarecovery"
        else
            run_cmd_on_local_node "mkfs.ext4 $REMOTE_MOTR_DEVICE"
            run_cmd_on_local_node "mount $REMOTE_MOTR_DEVICE $FAILOVER_MD_DIR"
            (( exec_status|=2));
        fi
    fi

    return $exec_status
}

# This function remove the swap device on both nodes
remove_swap() {
    local exec_status=0
    if [[ $LOCAL_NODE_RECOVERY_STATE == $RSTATE3 ]]; then
        m0drlog "Running remove swap on local node"
        run_cmd_on_local_node "swapoff -a"
        run_cmd_on_local_node "lvremove -f $LOCAL_LV_SWAP_DEVICE"
        [[ $? -eq 0 ]] || { (( exec_status|=1)); }
    fi

    if [[ $REMOTE_STORAGE_STATUS == 0 ]] && [[ $REMOTE_NODE_RECOVERY_STATE == $RSTATE3 ]]; then
        m0drlog "Running remove swap on remote node"
        run_cmd_on_remote_node "swapoff -a"
        run_cmd_on_remote_node "lvremove -f $REMOTE_LV_SWAP_DEVICE"
        [[ $? -eq 0 ]] || { (( exec_status|=2)); }
    elif [[ $REMOTE_NODE_RECOVERY_STATE == $RSTATE3 ]]; then
        m0drlog "Running remove swap for remote node from local node"
        run_cmd_on_local_node "swapoff -a"
        run_cmd_on_local_node "lvremove -f $REMOTE_LV_SWAP_DEVICE"
        [[ $? -eq 0 ]] || { (( exec_status|=2)); }
    fi

    return $exec_status
}

# This function will create the snapshot of metadata on both nodes
create_snapshot() {
    local exec_status=0

    if [[ $LOCAL_NODE_RECOVERY_STATE == $RSTATE2 ]] || [[ $LOCAL_NODE_RECOVERY_STATE == $RSTATE3 ]]; then
        m0drlog "Running create snapshot on local node"
        run_cmd_on_local_node "lvcreate -pr -l 100%FREE \
                                    -s -n $SNAPSHOT $LOCAL_LV_MD_DEVICE"
        [[ $? -eq 0 ]] || { (( exec_status|=1)); }
    fi

    if [[ $REMOTE_STORAGE_STATUS == 0 ]] && ([[ $REMOTE_NODE_RECOVERY_STATE == $RSTATE2 ]] || [[ $REMOTE_NODE_RECOVERY_STATE == $RSTATE3 ]]); then
        m0drlog "Running create snapshot on remote node"
        run_cmd_on_remote_node "lvcreate -pr -l 100%FREE \
                                    -s -n $SNAPSHOT $REMOTE_LV_MD_DEVICE"
        [[ $? -eq 0 ]] || { (( exec_status|=2)); }
    elif [[ $REMOTE_NODE_RECOVERY_STATE == $RSTATE2 ]] || [[ $REMOTE_NODE_RECOVERY_STATE == $RSTATE3 ]]; then
        m0drlog "Running create snapshot for remote node from local node"
        run_cmd_on_local_node "lvcreate -pr -l 100%FREE \
                                    -s -n $SNAPSHOT $REMOTE_LV_MD_DEVICE"
        [[ $? -eq 0 ]] || { (( exec_status|=2)); }
    fi

    return $exec_status
}

# This function will remove the metadata snapshot on both nodes
remove_snapshot() {
    local exec_status=0

    m0drlog "Running remove snapshot on local node"
    run_cmd_on_local_node "pvscan --cache"; # Ensure that volumes configs are synced
    LV_SNAPSHOT="$( lvs -o path $LOCAL_MD_VOLUMEGROUP | grep $SNAPSHOT )"

    run_cmd_on_local_node "lvremove -f $LV_SNAPSHOT"
    [[ $? -eq 0 ]] || { (( exec_status|=1)); }

    if [[ $REMOTE_STORAGE_STATUS == 0 ]]; then
        m0drlog "Running remove snapshot on remote node"
        run_cmd_on_remote_node "pvscan --cache"; # Ensure that volumes configs are synced
        LV_SNAPSHOT=$(run_cmd_on_remote_node "lvs -o path $REMOTE_MD_VOLUMEGROUP | \
                        grep $SNAPSHOT" )
        run_cmd_on_remote_node "lvremove -f $LV_SNAPSHOT"
        [[ $? -eq 0 ]] || { (( exec_status|=2)); }
    else
        m0drlog "Running remove snapshot for remote node from local node"
        LV_SNAPSHOT=$(run_cmd_on_local_node "lvs -o path $REMOTE_MD_VOLUMEGROUP | \
                        grep $SNAPSHOT" )
        run_cmd_on_local_node "lvremove -f $LV_SNAPSHOT"
        [[ $? -eq 0 ]] || { (( exec_status|=2)); }
    fi

    return $exec_status
}

# This function will create the swap device on both nodes
# `pvscan --cache` is used repeatedly because we need to ensure system is in
# correct state after recovery.
create_swap() {
    local exec_status=0

    m0drlog "Running create swap on local node"
    run_cmd_on_local_node "pvscan --cache"; # Ensure that volumes configs are synced
    run_cmd_on_local_node "lvcreate -n $SWAP_DEVICE -l 100%FREE $LOCAL_MD_VOLUMEGROUP"
    run_cmd_on_local_node "pvscan --cache"; # Ensure that volumes configs are synced
    LOCAL_LV_SWAP_DEVICE="$(lvs -o path $LOCAL_MD_VOLUMEGROUP | grep $SWAP_DEVICE | awk '{ print $1 }')"
    run_cmd_on_local_node "mkswap -f $LOCAL_LV_SWAP_DEVICE"
    run_cmd_on_local_node "swapon -a"
    [[ $? -eq 0 ]] || { (( exec_status|=1)); }

    m0drlog "Running create swap on remote node"
    if [[ $REMOTE_STORAGE_STATUS == 0 ]]; then
        run_cmd_on_remote_node "pvscan --cache"; # Ensure that volumes configs are synced
        run_cmd_on_remote_node "lvcreate -n $SWAP_DEVICE -l 100%FREE $REMOTE_MD_VOLUMEGROUP"
        run_cmd_on_remote_node "pvscan --cache"; # Ensure that volumes configs are synced
        REMOTE_LV_SWAP_DEVICE=$(run_cmd_on_remote_node "lvs -o path $REMOTE_MD_VOLUMEGROUP | grep $SWAP_DEVICE | awk '{ print \$1 }'" )
        run_cmd_on_remote_node "mkswap -f $REMOTE_LV_SWAP_DEVICE"
        run_cmd_on_remote_node "swapon -a"
        [[ $? -eq 0 ]] || { (( exec_status|=2)); }
    else
        m0drlog "Running create swap for remote node from local node"
        run_cmd_on_local_node "pvscan --cache"; # Ensure that volumes configs are synced
        run_cmd_on_local_node "lvcreate -n $SWAP_DEVICE -l 100%FREE $REMOTE_MD_VOLUMEGROUP"
        run_cmd_on_local_node "pvscan --cache"; # Ensure that volumes configs are synced
        REMOTE_LV_SWAP_DEVICE=$(lvs -o path $REMOTE_MD_VOLUMEGROUP | grep $SWAP_DEVICE | awk '{ print $1 }')
        run_cmd_on_local_node "mkswap -f $REMOTE_LV_SWAP_DEVICE"
        run_cmd_on_local_node "swapon -a"
        [[ $? -eq 0 ]] || { (( exec_status|=2)); }
    fi

    return $exec_status
}

# Clean up /var/motr/m0d-IOS_FID/stobs/o directory in case of beck tool crash from both nodes
# and umount /var/motr
cleanup_stobs_dir() {
    sleep 10 # Wait is needed to ensure both nodes process are stopped
    run_cmd_on_local_node "rm -f /var/motr/m0d-$LOCAL_IOS_FID/stobs/o/*"
    run_cmd_on_remote_node "rm -f /var/motr/m0d-$REMOTE_IOS_FID/stobs/o/*"
    run_cmd_on_local_node "umount $MD_DIR" > /dev/null
    run_cmd_on_remote_node "umount $MD_DIR" > /dev/null
    [[ $REMOTE_STORAGE_STATUS -eq 0 ]] || run_cmd_on_local_node "umount $FAILOVER_MD_DIR" > /dev/null
}

#The following command gives us the file count on the particular node.
#It takes 3 parameters as input
#1. filetype : The type of file whose quantity we want to count. Example "m0trace"
#2. directory : The location where the files of filetype are stored. Example "/var/motr/datarecovery"
#3. start_time : The time in seconds since epoch when the script has started its execution.
get_file_count() {
    local file_type=$1
    local directory=$2
    local start_time=$3
    filecount="$(cd $directory; find . -type f -exec stat  -c "%n %Y" {} \;| sort -n | grep $file_type | awk '{if($2>"'$start_time'") print $2; }' | wc -l)"
    echo "$filecount"
}

#The following command is used to remove the latest file generated in the given directory
#It takes 2 parameters as input
#1. filetype : The type of file which is to be removed. Example "m0trace"
#2. directory : The location where the files of filetype are stored. Example "/var/motr/datarecovery"
remove_last_file_generated() {
    local file_type=$1
    local directory=$2
    filename="$(cd $directory; ls -ltr | grep $file_type | awk '{print $9}' | tail -n 1)"
    echo "$filename"
    rm -f "$directory/$filename"
}

# The return statements between { .. }& are to indicate the exit status of
# child/background process that is spawned not for the function exit status.
# This function will run beck tool on both nodes
run_becktool() {
    local exec_status=0
    max_core_file_count=2
    max_trace_file_count=3
    # m0betool and m0beck depend on motr-kernel service so we try to get service up;
    # if this service does not start in 3 attempt on local node and remote node
    # then we cannot proceed further in the recovery.
    cnt=0
    while [[ $cnt != 3 ]];do
        systemctl start motr-kernel.service; exit_code=$?;
        [[ $exit_code == 0 ]] && break; (( cnt+=1 ));
    done
    sleep 10
    [[ `systemctl is-active motr-kernel.service` == "active" ]] || \
    die "motr-kernel service is not starting on local node, cannot proceed recovery further."

    if [[ $REMOTE_STORAGE_STATUS == 0 ]]; then
        cnt=0
        while [[ $cnt != 3 ]];do
            run_cmd_on_remote_node "systemctl start motr-kernel.service"; exit_code=$?;
            [[ $exit_code == 0 ]] && break; (( cnt+=1 ));
        done
        sleep 10
        if ! run_cmd_on_remote_node "[[ \`systemctl is-active motr-kernel.service\` == 'active' ]]";then
        die "motr-kernel service is not starting on remote node, cannot proceed recovery further."
        fi
    fi

    # Before running beck tool make sure that metadata directories are mounted
    if ! mountpoint -q $MD_DIR; then
        run_cmd_on_local_node "mount $LOCAL_MOTR_DEVICE $MD_DIR";
        [[ $? -eq 0 ]] || die "Cannot mount metadata device on /var/motr on local node"
        # create directory "datarecovery" in /var/motr if not exist to store traces
        run_cmd_on_local_node "mkdir -p $MD_DIR/datarecovery"
    fi

    if [[ $REMOTE_STORAGE_STATUS == 0 ]]; then
        if ! run_cmd_on_remote_node "mountpoint -q $MD_DIR"; then
            run_cmd_on_remote_node "mount $REMOTE_MOTR_DEVICE $MD_DIR";
            [[ $? -eq 0 ]] || die "Cannot mount metadata device on /var/motr on remote node"
            # create directory "datarecovery" in /var/motr if not exist to store traces
            run_cmd_on_remote_node "mkdir -p $MD_DIR/datarecovery"
        fi
    else
        if ! mountpoint -q $FAILOVER_MD_DIR; then
            run_cmd_on_local_node "mount $REMOTE_MOTR_DEVICE $FAILOVER_MD_DIR"
            [[ $? -eq 0 ]] || die "Cannot mount metadata device on failover dir on local node"
            # create directory "datarecovery" in /var/motr if not exist to store traces
            run_cmd_on_local_node "mkdir -p $FAILOVER_MD_DIR/datarecovery"
        fi
    fi

    run_cmd_on_local_node "pvscan --cache"; # Ensure that volumes configs are synced
    # Running beck tool parallelly on both nodes as it is long running operation
    {
        local_beck_status=0
        LV_SNAPSHOT="$( lvs -o path $LOCAL_MD_VOLUMEGROUP | grep $SNAPSHOT )"
        SOURCE_IMAGE=$LV_SNAPSHOT
        DEST_DOMAIN_DIR="$MD_DIR/m0d-$LOCAL_IOS_FID"

        m0drlog "Running Becktool on local node"
        run_cmd_on_local_node "(cd $MD_DIR/datarecovery; $BECKTOOL -s $SOURCE_IMAGE \
                               -d $DEST_DOMAIN_DIR/db -a $DEST_DOMAIN_DIR/stobs -g $LOCAL_SEG_GEN_ID;)"
        cmd_exit_status=$?
        # restart the execution of command if exit code is ESEGV error
        while [[ $cmd_exit_status == $ESEGV ]];
        do
            #Following is the code to limit the number of core-m0beck and m0trace files, generated due to m0beck crash( receiving SEGV signal ), to 2 each.
            core_m0beck_file_count=$(get_file_count "core-m0beck" "$CRASH_DIR" "$SCRIPT_START_TIME")
            echo "File count value $core_m0beck_file_count"

            if [[ $core_m0beck_file_count -gt $max_core_file_count ]]; then
                    echo "Deleting core m0beck extra file $core_m0beck_file_count"
                    rem_file=$(remove_last_file_generated "core-m0beck" "$CRASH_DIR")
                    echo "$rem_file"
            fi
            
            m0trace_file_count=$(get_file_count "m0trace" "$MD_DIR/datarecovery" "$SCRIPT_START_TIME")
            echo "File count value $m0trace_file_count"
            if [[ $m0trace_file_count -gt $max_trace_file_count ]]; then
                    echo "Deleting core m0beck extra file $m0trace_file_count"
                    rem_file=$(remove_last_file_generated "m0trace" "$MD_DIR/datarecovery")
                    echo "$rem_file"
            fi
            #Code to limit the number of core-m0beck and m0trace files to 2 each ends here.

            m0drlog "Restarting Becktool on local node"
            run_cmd_on_local_node "(cd $MD_DIR/datarecovery; $BECKTOOL -s $SOURCE_IMAGE \
                                   -d $DEST_DOMAIN_DIR/db -a $DEST_DOMAIN_DIR/stobs -g $LOCAL_SEG_GEN_ID;)"
            cmd_exit_status=$?
        done

        [[ $cmd_exit_status == 0 ]] && local_beck_status=0 || local_beck_status=$cmd_exit_status

        return $local_beck_status
    }&
    local_cmd_pid="$!"

    {
        remote_beck_status=0
        if [[ $REMOTE_STORAGE_STATUS == 0 ]]; then
            run_cmd_on_remote_node "pvscan --cache"; # Ensure that volumes configs are synced
            LV_SNAPSHOT=$(run_cmd_on_remote_node "lvs -o path $REMOTE_MD_VOLUMEGROUP | grep $SNAPSHOT" )
            SOURCE_IMAGE=$LV_SNAPSHOT
            DEST_DOMAIN_DIR="$MD_DIR/m0d-$REMOTE_IOS_FID"

            m0drlog "Running Becktool on remote node"
            # Below statement is for logging purpose
            echo "Running '(cd $MD_DIR/datarecovery; $BECKTOOL -s $SOURCE_IMAGE -d $DEST_DOMAIN_DIR/db -a $DEST_DOMAIN_DIR/stobs -g $REMOTE_SEG_GEN_ID;)'" >> ${LOG_FILE}
            run_cmd_on_remote_node "bash -s" <<-EOF
            (cd $MD_DIR/datarecovery; $BECKTOOL -s $SOURCE_IMAGE -d $DEST_DOMAIN_DIR/db -a $DEST_DOMAIN_DIR/stobs -g $REMOTE_SEG_GEN_ID;)
EOF
            cmd_exit_status=$?
            # restart the execution of command if exit code is ESEGV error
            while [[ $cmd_exit_status == $ESEGV ]];
            do

                #Following is the code to limit the number of core-m0beck and m0trace files, generated due to m0beck crash ( receiving SEGV signal ), to 2 each.
                run_cmd_on_remote_node "bash -s" <<EOF
                $(typeset -f get_file_count)
                $(typeset -f remove_last_file_generated)
                export -f get_file_count
                export -f remove_last_file_generated
                $(declare -x CRASH_DIR)
                $(declare -x MD_DIR)
                $(declare -x SCRIPT_START_TIME)
                $(declare -x max_core_file_count)
                $(declare -x max_trace_file_count)

                core_m0beck_file_count=\$(get_file_count "core-m0beck" "$CRASH_DIR" "$SCRIPT_START_TIME")
                echo "File count value \$core_m0beck_file_count"

                if [[ \$core_m0beck_file_count -gt $max_core_file_count ]]; then
                        echo "Deleting core m0beck extra file \$core_m0beck_file_count"
                        rem_file=\$(remove_last_file_generated "core-m0beck" "$CRASH_DIR")
                        echo "\$rem_file"
                fi
                
                m0trace_file_count=\$(get_file_count "m0trace" "$MD_DIR/datarecovery" "$SCRIPT_START_TIME")
                echo "File count value \$m0trace_file_count"
                if [[ \$m0trace_file_count -gt $max_trace_file_count ]]; then
                        echo "Deleting core m0beck extra file \$m0trace_file_count"
                        rem_file=\$(remove_last_file_generated "m0trace" "$MD_DIR/datarecovery")
                        echo "\$rem_file"
                fi
EOF
            #Code to limit the number of core-m0beck and m0trace files to 2 each ends here.

                m0drlog "Restarting Becktool on remote node"
                run_cmd_on_remote_node "bash -s" <<-EOF
                (cd $MD_DIR/datarecovery; $BECKTOOL -s $SOURCE_IMAGE -d $DEST_DOMAIN_DIR/db -a $DEST_DOMAIN_DIR/stobs -g $REMOTE_SEG_GEN_ID;)
EOF
                cmd_exit_status=$?
            done

            [[ $cmd_exit_status == 0 ]] && remote_beck_status=0 || remote_beck_status=$cmd_exit_status

            return $remote_beck_status
        else
            LV_SNAPSHOT=$(run_cmd_on_local_node "lvs -o path $REMOTE_MD_VOLUMEGROUP | grep $SNAPSHOT" )
            SOURCE_IMAGE=$LV_SNAPSHOT
            DEST_DOMAIN_DIR="$FAILOVER_MD_DIR/m0d-$REMOTE_IOS_FID"

            m0drlog "Running Becktool for remote node from local node"
            # Below statement is for logging purpose
            echo "Running '(cd $FAILOVER_MD_DIR/datarecovery; $BECKTOOL -s $SOURCE_IMAGE -d $DEST_DOMAIN_DIR/db -a $DEST_DOMAIN_DIR/stobs -g $REMOTE_SEG_GEN_ID;)'" >> ${LOG_FILE}
            run_cmd_on_local_node "(cd $FAILOVER_MD_DIR/datarecovery; $BECKTOOL -s $SOURCE_IMAGE \
                                   -d $DEST_DOMAIN_DIR/db -a $DEST_DOMAIN_DIR/stobs -g $REMOTE_SEG_GEN_ID;)"
            cmd_exit_status=$?
            # restart the execution of command if exit code is ESEGV error
            while [[ $cmd_exit_status == $ESEGV ]];
            do
                #Following is the code to limit the number of core-m0beck and m0trace files, generated due to m0beck crash ( receiving SEGV signal ), to 2 each.
                core_m0beck_file_count=$(get_file_count "core-m0beck" "$CRASH_DIR" "$SCRIPT_START_TIME")
                echo "File count value $core_m0beck_file_count"

                if [[ $core_m0beck_file_count -gt $max_core_file_count ]]; then
                        echo "Deleting core m0beck extra file $core_m0beck_file_count"
                        rem_file=$(remove_last_file_generated "core-m0beck" "$CRASH_DIR")
                        echo "$rem_file"
                fi
                
                m0trace_file_count=$(get_file_count "m0trace" "$FAILOVER_MD_DIR/datarecovery" "$SCRIPT_START_TIME")
                echo "File count value $m0trace_file_count"
                if [[ $m0trace_file_count -gt $max_trace_file_count ]]; then
                        echo "Deleting core m0beck extra file $m0trace_file_count"
                        rem_file=$(remove_last_file_generated "m0trace" "$FAILOVER_MD_DIR/datarecovery")
                        echo "$rem_file"
                fi
                #Code to limit the number of core-m0beck and m0trace files to 2 each ends here.

                m0drlog "Restarting Becktool for remote node from local node"
                run_cmd_on_local_node "(cd $FAILOVER_MD_DIR/datarecovery; $BECKTOOL -s $SOURCE_IMAGE \
                                       -d $DEST_DOMAIN_DIR/db -a $DEST_DOMAIN_DIR/stobs -g $REMOTE_SEG_GEN_ID;)"
                cmd_exit_status=$?
            done

            [[ $cmd_exit_status == 0 ]] && remote_beck_status=0 || remote_beck_status=$cmd_exit_status

            return $remote_beck_status
        fi
    }&
    remote_cmd_pid="$!"

    # If beck command failed on either of nodes, kill command running on both nodes
    # If either beck command passed, then wait for other command to complete execution.
    # Monitor both commands using their pids if they running or not
    while [ -d /proc/$local_cmd_pid ] && [ -d /proc/$remote_cmd_pid ]; do
        sleep 5
    done

    if !([ -d /proc/$local_cmd_pid ]);then
        local local_cmd_exit_code=0
        wait ${local_cmd_pid} || { local_cmd_exit_code=$?;}
        if [[ $local_cmd_exit_code == 0 ]]; then
            m0drlog "Becktool completed on local node"
            wait ${remote_cmd_pid} || { ((exec_status|=2));
                                        cleanup_stobs_dir
                                        die "ERROR: Becktool failed on remote node";
                                      }
            m0drlog "Becktool completed on remote node"
            cleanup_stobs_dir
            return $exec_status
        else
            pkill -9 m0beck; run_cmd_on_remote_node "pkill -9 m0beck";
            ((exec_status|=1));
            cleanup_stobs_dir
            die "ERROR: Becktool failed on local node";
        fi
    fi

    if !([ -d /proc/$remote_cmd_pid ]);then
        local remote_cmd_exit_code=0
        wait ${remote_cmd_pid} || { remote_cmd_exit_code=$?;}
        if [[ $remote_cmd_exit_code == 0 ]]; then
            m0drlog "Becktool completed on remote node"
            wait ${local_cmd_pid} || { ((exec_status|=1));
                                        cleanup_stobs_dir
                                        die "ERROR: Becktool failed on local node";
                                     }
            m0drlog "Becktool completed on local node"
            cleanup_stobs_dir
            return $exec_status
        else
            pkill -9 m0beck; ((exec_status|=2));
            cleanup_stobs_dir
            die "ERROR: Becktool failed on remote node";
        fi
    fi

    cleanup_stobs_dir

    return $exec_status
}
#flush the any outstanding IO
flush_io(){

    m0drlog "flush outstanding IO from buffers"

    LV_SNAPSHOT="$( lvs -o path $LOCAL_MD_VOLUMEGROUP | grep $SNAPSHOT )"
    LV_MD_DEVICE="$( lvs -o path $LOCAL_MD_VOLUMEGROUP | grep $MD_DEVICE )"

    run_cmd_on_local_node "blockdev --flushbufs $LV_MD_DEVICE"
    run_cmd_on_local_node "blockdev --flushbufs $LV_SNAPSHOT"

    if [[ $REMOTE_STORAGE_STATUS == 0 ]]; then

        LV_SNAPSHOT=$(run_cmd_on_remote_node "lvs -o path $REMOTE_MD_VOLUMEGROUP | \
                        grep $SNAPSHOT" )
        LV_MD_DEVICE=$(run_cmd_on_remote_node "lvs -o path $REMOTE_MD_VOLUMEGROUP | \
                        grep $MD_DEVICE" )
        run_cmd_on_remote_node "blockdev --flushbufs $LV_MD_DEVICE"
        run_cmd_on_remote_node "blockdev --flushbufs $LV_SNAPSHOT"
    else
        LV_SNAPSHOT=$(run_cmd_on_local_node "lvs -o path $REMOTE_MD_VOLUMEGROUP | \
                        grep $SNAPSHOT" )
        LV_MD_DEVICE=$(run_cmd_on_local_node "lvs -o path $REMOTE_MD_VOLUMEGROUP | \
                        grep $MD_DEVICE" )
        run_cmd_on_local_node "blockdev --flushbufs $LV_MD_DEVICE"
        run_cmd_on_local_node "blockdev --flushbufs $LV_SNAPSHOT"
    fi

    m0drlog "Wait 5 minutes to flush any outstanding IO"
    sleep 5m
}
# ------------------------- script start --------------------------------

is_user_root_user # check the script is running with root access
[[ $? -eq 0 ]] || { die "Please run script as a root user"; }

# Check for the files needed by prov-m0-reset script as argument
[[ -f $CDF_FILENAME ]] || die "ERROR: File not found $CDF_FILENAME"
[[ -f $HA_ARGS_FILENAME ]] || die "ERROR: File not found $HA_ARGS_FILENAME"

# First we try to get the current cluster configuration. After that we check
# for the snapshot presence on both nodes.
# If snapshot is available then we are restarting this process as result of
# either power failure or some other unknown tool termination during previous
# run in which case we just continue to use previously created snapshot.
# If snapshot not available then we create the snapshot now, based on the
# nodes current state for snaphot functions will execute the commands.

shutdown_services

get_cluster_configuration

# Execute becktool only in the script if option --beck-only is set.
# Use --beck-only option only when snapshot is already present on the system.
if $beck_only ; then
    m0drlog "Running only becktool in the recovery script"
    run_becktool
    exit $?
fi

# Only remove MD_Snapshot if present and create lv_main_swap again
if $clean_snapshot ; then
    m0drlog "Removing MD_Snapshot if present and create lv_main_swap again"
    remove_snapshot                # remove snapshot on both nodes
    remove_snapshot_status=$?
    [[ remove_snapshot_status -eq 0 ]] || { die "ERROR: Remove snapshot failed with code $remove_snapshot_status"; }

    create_swap                    # recreate swap on both nodes
    create_swap_status=$?
    [[ create_swap_status -eq 0 ]] || { die "ERROR: Create swap failed with code $create_swap_status"; }
    exit 0
fi

run_fsck                       # run fsck on both nodes
run_fsck_status=$?
[[ run_fsck_status -eq 0 ]] || { m0drlog "ERROR: fsck failed with code $run_fsck_status"; }

replay_logs_and_get_gen_id_of_seg0 $run_fsck_status   # replay logs on both nodes
replay_logs_and_get_gen_id_of_seg0_status=$?
[[ replay_logs_and_get_gen_id_of_seg0_status -eq 0 ]] || { m0drlog "ERROR: Replay logs failed with code $replay_logs_and_get_gen_id_of_seg0_status"; }

remove_swap                    # remove swap on both nodes
remove_swap_status=$?
[[ remove_swap_status -eq 0 ]] || { die "ERROR: Remove swap failed with code $remove_swap_status"; }

create_snapshot                # create snapshot on both node
create_snapshot_status=$?
[[ create_snapshot_status -eq 0 ]] || { die "ERROR: Create snapshot failed with code $create_snapshot_status"; }

reinit_mkfs                    # reinit mkfs only on both nodes

run_becktool                   # run becktool on both nodes
run_becktool_status=$?
[[ run_becktool_status -eq 0 ]] || { die "ERROR: Run becktool failed with code $run_becktool_status"; }

flush_io                        # flush io on both the nodes

remove_snapshot                # remove snapshot on both nodes
remove_snapshot_status=$?
[[ remove_snapshot_status -eq 0 ]] || { die "ERROR: Remove snapshot failed with code $remove_snapshot_status"; }

create_swap                    # recreate swap on both nodes
create_swap_status=$?
[[ create_swap_status -eq 0 ]] || { die "ERROR: Create swap failed with code $create_swap_status"; }

exit 0 # script exit code
