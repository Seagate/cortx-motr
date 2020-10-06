#!/usr/bin/env bash
# set -x
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
HA_ARGS_FILENAME="/opt/seagate/cortx/ha/conf/build-ees-ha-args.yaml"
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

# The function will return 0 if ioservice is started on the specified node,
# else this will return 1
is_ios_running() {
    local ios_fid=$1
    if $(run_cmd "$LOCAL_NODE" "hctl status | grep -i ioservice | grep $ios_fid | grep -i started"); then
        return 0
    else
        return 1
    fi
}

# This function will initialize values of local and remote ioservices
# fid variable.
get_ios_fid() {
    LOCAL_IOS_FID=$(cat /etc/sysconfig/m0d-0x7200000000000001\:0x* | grep "$LOCAL_NODE" -B 1 | grep FID | cut -f 2 -d "="| tr -d \')
    REMOTE_IOS_FID=$(cat /etc/sysconfig/m0d-0x7200000000000001\:0x* | grep "$REMOTE_NODE" -B 1 | grep FID | cut -f 2 -d "="| tr -d \')

    if [[ $LOCAL_IOS_FID == "" ]] || [[ $REMOTE_IOS_FID == "" ]];then
        die "Failed to get ioservice FIDs."
    fi
}

# The function will execute command on the specified node
run_cmd() {
    local node=$1; shift
    local cmd=$*
    if [[ "$node" == "$LOCAL_NODE" ]];then
        echo "[`date`]: Running '$cmd'" >> ${LOG_FILE}
        eval "$cmd" 2>&1 | tee -a ${LOG_FILE} # execute the command on current/local node
        return ${PIPESTATUS[0]} # return exit status of first command
    else
        echo "[`date`]: Running 'ssh root@$REMOTE_NODE $cmd'" >> ${LOG_FILE}
        ssh root@$REMOTE_NODE "$cmd" 2>&1 | tee -a ${LOG_FILE} # execute the command
        return ${PIPESTATUS[0]} # return exit status of first command
    fi
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

# This function will provide the local volumes info for the specified node
get_lvs_info() {
    local node=$1
    local infotype=$2
    local volgrp=$3
    local dev=$4
    local printparam=$5
    if [[ "$print_param" == "" ]]; then
        return $(run_cmd "$node" "lvs -o $infotype $volgrp | grep $dev")
    else
        return $(run_cmd "$node" "lvs -o $infotype $volgrp | grep $dev | awk '{ print $printparam }'")
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

    test=$(get_lvs_info "$LOCAL_NODE" "name" "$LOCAL_MD_VOLUMEGROUP" "$SNAPSHOT" "$1")
    if [ "$test" != "$SNAPSHOT" ]; then
        test=$(get_lvs_info "$LOCAL_NODE" "name" "$LOCAL_MD_VOLUMEGROUP" "$SWAP_DEVICE" "$1")
        if [ "$test" = "$SWAP_DEVICE" ]; then
            MD_SIZE=$(get_lvs_info "$LOCAL_NODE" "lvname,size" "$LOCAL_MD_VOLUMEGROUP --units K --nosuffix" "$MD_DEVICE" "$2")

            SWAP_SIZE=$(get_lvs_info "$LOCAL_NODE" "lvname,size" "$LOCAL_MD_VOLUMEGROUP --units K --nosuffix" "$SWAP_DEVICE" "$2")

            # If the comparison is true then bc return 1 otherwise return 0
            if [ $(bc <<< "$MD_SIZE > $SWAP_SIZE") = 1 ]; then
                die "ERROR: Metadata size is not less than swap size on local node !!"
            fi
            (( state|=1 )); # run_fsck, replay_logs_and_get_gen_id_of_seg0, remove_swap_all_nodes
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
    test=$(get_lvs_info "$REMOTE_NODE" "name" "$REMOTE_MD_VOLUMEGROUP" "$SNAPSHOT" "\$1") || \
    test=$(get_lvs_info "$LOCAL_NODE" "name" "$REMOTE_MD_VOLUMEGROUP" "$SNAPSHOT" "$1")

    if [ "$test" != "$SNAPSHOT" ]; then
        [[ $REMOTE_STORAGE_STATUS == 0 ]] && \
        test=$(get_lvs_info "$REMOTE_NODE" "name" "$REMOTE_MD_VOLUMEGROUP" "$SWAP_DEVICE" "\$1") || \
        test=$(get_lvs_info "$LOCAL_NODE" "name" "$REMOTE_MD_VOLUMEGROUP" "$SWAP_DEVICE" "$1")

        if [ "$test" = "$SWAP_DEVICE" ]; then
            [[ $REMOTE_STORAGE_STATUS == 0 ]] && \
            MD_SIZE=$(get_lvs_info "$REMOTE_NODE" "lvname,size" "$REMOTE_MD_VOLUMEGROUP --units K --nosuffix" "$MD_DEVICE" "\$2") || \
            MD_SIZE=$(get_lvs_info "$LOCAL_NODE" "lvname,size" "$REMOTE_MD_VOLUMEGROUP --units K --nosuffix" "$MD_DEVICE" "$2")

            [[ $REMOTE_STORAGE_STATUS == 0 ]] && \
            SWAP_SIZE=$(get_lvs_info "$REMOTE_NODE" "lvname,size $REMOTE_MD_VOLUMEGROUP --units K --nosuffix" "$SWAP_DEVICE" "\$2") || \
            SWAP_SIZE=$(get_lvs_info "$LOCAL_NODE" "lvname,size $REMOTE_MD_VOLUMEGROUP --units K --nosuffix" "$SWAP_DEVICE" "$2")

            # If the comparison is true then bc return 1 otherwise return 0
            if [ $(bc <<< "$MD_SIZE > $SWAP_SIZE") = 1 ]; then
                die "ERROR: Metadata size is not less than swap size on remote node !!"
            fi
            (( state|=1 )); # run_fsck, replay_logs_and_get_gen_id_of_seg0, remove_swap_all_nodes
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

    # run_cmd "$LOCAL_NODE" "pvscan --cache"; # Ensure that volumes configs are synced
    echo "[`date`]: Running '$cmd'" >> ${LOG_FILE}
    eval "pvscan --cache" 2>&1 | tee -a ${LOG_FILE}

    # Set global variables depending on which node you are on
    if [[ $(cat /var/lib/hare/node-name) == "srvnode-1" ]]; then

        LOCAL_NODE="srvnode-1"
        REMOTE_NODE="srvnode-2"
        # Path for metadata device for local node
        LOCAL_MOTR_DEVICE=$(run_cmd "$LOCAL_NODE" "grep left-volume /opt/seagate/cortx/ha/conf/build-ees-ha-args.yaml |\
                            awk '{ print $2 }'")
        # Path for metadata device for remote node
        REMOTE_MOTR_DEVICE=$(run_cmd "$LOCAL_NODE" "grep right-volume /opt/seagate/cortx/ha/conf/build-ees-ha-args.yaml |\
                            awk '{ print $2 }'")

        FAILOVER_MD_DIR="/var/motr2"

    elif [[ $(cat /var/lib/hare/node-name) == "srvnode-2" ]]; then

        LOCAL_NODE="srvnode-2"
        REMOTE_NODE="srvnode-1"
        # Path for metadata device for local node
        LOCAL_MOTR_DEVICE=$(run_cmd "$LOCAL_NODE" "grep right-volume /opt/seagate/cortx/ha/conf/build-ees-ha-args.yaml |\
                            awk '{ print $2 }'")
        # Path for metadata device for remote node
        REMOTE_MOTR_DEVICE=$(run_cmd "$LOCAL_NODE" "grep left-volume /opt/seagate/cortx/ha/conf/build-ees-ha-args.yaml |\
                            awk '{ print $2 }'")

        FAILOVER_MD_DIR="/var/motr1"
    fi

    LOCAL_MD_VOLUMEGROUP="vg_metadata_"$LOCAL_NODE
    REMOTE_MD_VOLUMEGROUP="vg_metadata_"$REMOTE_NODE

    LOCAL_LV_SWAP_DEVICE=$(get_lvs_info "$LOCAL_NODE" "path" "$LOCAL_MD_VOLUMEGROUP" "$SWAP_DEVICE" "$1")
    LOCAL_LV_MD_DEVICE=$(get_lvs_info "$LOCAL_NODE" "path" "$LOCAL_MD_VOLUMEGROUP" "$MD_DEVICE" "$1")

    get_ios_fid
    get_utility_path

    # Check the ios health status and verify that storage is accessible or not
    if is_ios_running "$LOCAL_IOS_FID"; then
        m0drlog "Proceeding to perform recovery of storage on local node."
    elif run_cmd "$LOCAL_NODE" "dd if=$LOCAL_MOTR_DEVICE of=/dev/null bs=4096 count=1"; then
        m0drlog "Local ioservice is not running, but we can access the local storage, performing recovery of storage on local node"
    else
        # local ios is failed, can't access the local storage recovery not possible on this node
        die "***ERROR: Local ioservice is not running and cannot access the local storage \
            recovery not possible for the storage of local node, please try to run recovery from $REMOTE_NODE***"
    fi

    if is_ios_running "$REMOTE_IOS_FID"; then
        m0drlog "Proceeding to perform recovery on remote node."
        run_cmd "$REMOTE_NODE" "pvscan --cache"; # Ensure that volumes configs are synced
        REMOTE_LV_SWAP_DEVICE=$(get_lvs_info "$REMOTE_NODE" "path" "$REMOTE_MD_VOLUMEGROUP" "$SWAP_DEVICE" "\$1")
        REMOTE_LV_MD_DEVICE=$(get_lvs_info "$REMOTE_NODE" "path" "$REMOTE_MD_VOLUMEGROUP" "$MD_DEVICE" "\$1")
        REMOTE_STORAGE_STATUS=0 # We can run commands on remote node
    elif run_cmd "$REMOTE_NODE" "dd if=$REMOTE_MOTR_DEVICE of=/dev/null bs=4096 count=1"; then
        m0drlog "Remote ioservice is not running, but we can access the storage on remote node, \
                performing recovery of storage on remote node"
        run_cmd "$REMOTE_NODE" "pvscan --cache"; # Ensure that volumes configs are synced
        REMOTE_LV_SWAP_DEVICE=$(get_lvs_info "$REMOTE_NODE" "path" "$REMOTE_MD_VOLUMEGROUP" "$SWAP_DEVICE" "\$1")
        REMOTE_LV_MD_DEVICE=$(get_lvs_info "$REMOTE_NODE" "path" "$REMOTE_MD_VOLUMEGROUP" "$MD_DEVICE" "\$1")
        REMOTE_STORAGE_STATUS=0
    elif run_cmd "$LOCAL_NODE" "dd if=$REMOTE_MOTR_DEVICE of=/dev/null bs=4096 count=1"; then
        m0drlog "Remote ioservice is not running and remote node is not reachable, \
                but can access remote storage through local node, performing recovery of storage on remote node, from local node"
        REMOTE_STORAGE_STATUS=1
        REMOTE_LV_SWAP_DEVICE=$(get_lvs_info "$LOCAL_NODE" "path" "$REMOTE_MD_VOLUMEGROUP" "$SWAP_DEVICE" "$1")
        REMOTE_LV_MD_DEVICE=$(get_lvs_info "$LOCAL_NODE" "path" "$REMOTE_MD_VOLUMEGROUP" "$MD_DEVICE" "$1")
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

# This function will unmount the specified device from the specified node
unmount_device() {
    local node=$1
    local dir=$2
    local failure_msg=$3
    local log_msg=$4
    if run_cmd "$node" "mountpoint -q $dir"; then
        if ! run_cmd "$node" "umount $dir"; then
            die "$failure_msg"
        fi
    else
        if [[ "$log_msg" != "" ]]; then
            m0drlog "$log_msg"
        fi
    fi
}

# This function return 0 if all steps in function run successfully
# else this will return 1
reinit_mkfs() {

    # Unmount the /var/motr if mounted on both nodes
    unmount_device "$LOCAL_NODE" "$MD_DIR" "MD Device might be busy on local node, \
                    please try shutting down cluster and retry"

    if [[ $REMOTE_STORAGE_STATUS == 0 ]]; then
        unmount_device "$REMOTE_NODE" "$MD_DIR" "MD Device might be busy on remote node, \
                        please try shutting down cluster and retry"
    else
        unmount_device "$LOCAL_NODE" "$FAILOVER_MD_DIR" "MD Device of remote node might be busy on local node, \
                        please try shutting down cluster and retry"
    fi

    # command line for reinit cluster with mkfs
    m0drlog "Reinitializing the cluster with mkfs, Please wait may take few minutes"
    run_cmd "$LOCAL_NODE" "set M0_RESET_LOG_FILE=$LOG_FILE; prov-m0-reset $CDF_FILENAME $HA_ARGS_FILENAME --mkfs-only $SINGLE_NODE_RUNNING; unset M0_RESET_LOG_FILE"
    [[ $? -eq 0 ]] || die "***ERROR: Cluster reinitialization with mkfs failed***"
    m0drlog "Cluster reinitialization with mkfs is completed"
    sleep 3
}

# This function will try to start the motr-kernel service on the specified node
start_motr_kernel_service() {
    local node=$1
    local failure_msg=$2
    local cnt=0
    while [[ $cnt != 3 ]];do
        run_cmd "$node" "systemctl start motr-kernel.service"; exit_code=$?;
        [[ $exit_code == 0 ]] && break; (( cnt+=1 ));
    done
    if ! run_cmd "$node" "[[ `systemctl is-active motr-kernel.service` == \"active\" ]]";then
        die "$failure_msg"
    fi
}

# This function will replay journal logs on the specified node
replay_journal_logs() {
    local node=$1
    local dir=$2
    local ios_fid=$3
    return $(run_cmd "$node" "(cd $dir/datarecovery; $M0BETOOL be_recovery_run $dir/m0d-$ios_fid/db;)")
}

# This function will fetch the generation id
get_gen_id() {
    local node=$1
    local dir=$2
    local ios_fid=$3
    if [[ "$ios_fid" == "$LOCAL_IOS_FID" ]];then
        m0drlog "Get generation id of local node from segment 1"
        LOCAL_SEG_GEN_ID=$(run_cmd "$node" "$BECKTOOL -s $dir/m0d-$ios_fid/db/o/100000000000000:2a -p | cut -d \"(\" -f2 | cut -d \")\" -f1")
        if [[ $LOCAL_SEG_GEN_ID -eq 0 ]]; then
            m0drlog "Get generation id of local node from segment 0"
            LOCAL_SEG_GEN_ID=$(run_cmd "$node" "$BECKTOOL -s $dir/m0d-$ios_fid/db/o/100000000000000:29 -p | cut -d \"(\" -f2 | cut -d \")\" -f1")
        fi
        run_cmd "$node" "echo \"segment genid : ($LOCAL_SEG_GEN_ID)\"  >  $dir/m0d-$ios_fid/gen_id"
    else
        m0drlog "Get generation id of remote node from segment 1"
        REMOTE_SEG_GEN_ID=$(run_cmd "$node" "$BECKTOOL -s $dir/m0d-$ios_fid/db/o/100000000000000:2a -p | cut -d \"(\" -f2 | cut -d \")\" -f1")
        if [[ $REMOTE_SEG_GEN_ID -eq 0 ]]; then
            m0drlog "Get generation id of remote node from segment 0"
            REMOTE_SEG_GEN_ID=$(run_cmd "$node" "$BECKTOOL -s $dir/m0d-$ios_fid/db/o/100000000000000:29 -p | cut -d \"(\" -f2 | cut -d \")\" -f1")
        fi
        run_cmd "$node" "echo \"segment genid : ($REMOTE_SEG_GEN_ID)\"  >  $dir/m0d-$ios_fid/gen_id"
    fi
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
    local fsck_result=$1
    local exec_status=0

    # m0betool and m0beck depend on motr-kernel service so we try to get service up;
    # if this service does not start in 3 attempt on local node and remote node
    # then we cannot proceed further in the recovery.
    if [[ $LOCAL_NODE_RECOVERY_STATE == $RSTATE3 ]]; then
        start_motr_kernel_service "$LOCAL_NODE" "motr-kernel service is not starting on local node, cannot proceed recovery further."
    fi

    if [[ $REMOTE_STORAGE_STATUS == 0 ]] && [[ $REMOTE_NODE_RECOVERY_STATE == $RSTATE3 ]]; then
        start_motr_kernel_service "$REMOTE_NODE" "motr-kernel service is not starting on remote node, cannot proceed recovery further."
    fi

    # Check if the fsck is passed on both local node successfully or not
    # Here arg1 value as 0 states fsck passed on local and remote node
    # Here arg1 value as 2 states fsck failed on remote node but passed on local node
    if [[ $LOCAL_NODE_RECOVERY_STATE == $RSTATE3 ]]; then
        if [[ $fsck_result == 0 || $fsck_result == 2 ]]; then
            m0drlog "Replaying journal logs on local node"
            if replay_journal_logs "$LOCAL_NODE" "$MD_DIR" "$LOCAL_IOS_FID";then
                m0drlog "Journal logs replayed successfully on local node"
            else
                m0drlog "ERROR: Journal logs replay failed on local node"
                (( exec_status|=1));
            fi

            get_gen_id "$LOCAL_NODE" "$MD_DIR" "$LOCAL_IOS_FID"
        else
            m0drlog "ERROR: Mount failed! Can't replay journal logs on local node..."
            (( exec_status|=1));
        fi
    else
        if run_cmd "$LOCAL_NODE" "[[ -f $MD_DIR/m0d-$LOCAL_IOS_FID/gen_id ]]"; then
            LOCAL_SEG_GEN_ID=$(run_cmd "$LOCAL_NODE"  "cat $MD_DIR/m0d-$LOCAL_IOS_FID/gen_id | grep 'segment genid' | cut -d \"(\" -f2 | cut -d \")\" -f1")
        fi
    fi
    # Check if the fsck is passed on remote node successfully or not
    # Here arg1 value as 0, states fsck passed on both local and remote node
    # Here arg1 value as 1, states fsck failed on local node but passed on remote node
    if [[ $REMOTE_STORAGE_STATUS == 0 ]] && [[ $REMOTE_NODE_RECOVERY_STATE == $RSTATE3 ]]; then
        if [[ $fsck_result == 0 || $fsck_result == 1 ]]; then
            m0drlog "Replaying journal logs on remote node"
            if replay_journal_logs "$REMOTE_NODE" "$MD_DIR" "$REMOTE_IOS_FID"; then
                m0drlog "Journal logs replayed successfully on remote node"
            else
                m0drlog "ERROR: Journal logs replay failed on remote node"
                (( exec_status|=2));
            fi

            get_gen_id "$REMOTE_NODE" "$MD_DIR" "$REMOTE_IOS_FID"
        else
            m0drlog "ERROR: Mount failed! Can't replay journal logs on remote node..."
            (( exec_status|=2));
        fi
    elif [[ $REMOTE_NODE_RECOVERY_STATE == $RSTATE3 ]]; then
        if [[ $fsck_result == 0 || $fsck_result == 1 ]]; then
            m0drlog "Replaying journal logs for remote node from local node"
            if replay_journal_logs "$LOCAL_NODE" "$FAILOVER_MD_DIR" "$REMOTE_IOS_FID"; then
                m0drlog "Journal logs replayed successfully for remote node from local node"
            else
                m0drlog "ERROR: Journal logs replay failed for remote node"
                (( exec_status|=2));
            fi

            get_gen_id "$LOCAL_NODE" "$FAILOVER_MD_DIR" "$REMOTE_IOS_FID"
        else
            m0drlog "ERROR: Mount failed! Can't replay journal logs on remote node..."
            (( exec_status|=2));
        fi
     elif [[ $REMOTE_STORAGE_STATUS == 0 ]]; then
        if run_cmd "$REMOTE_NODE" "[[ -f $MD_DIR/m0d-$REMOTE_IOS_FID/gen_id ]]"; then
            REMOTE_SEG_GEN_ID=$(run_cmd "$REMOTE_NODE" "cat $MD_DIR/m0d-$REMOTE_IOS_FID/gen_id | grep 'segment genid' | cut -d \"(\" -f2 | cut -d \")\" -f1")
        fi
     else
        if run_cmd "$LOCAL_NODE" "[[ -f $FAILOVER_MD_FDIR/m0d-$REMOTE_IOS_FID/gen_id ]]"; then
            REMOTE_SEG_GEN_ID=$(run_cmd "$LOCAL_NODE" "cat $FAILOVER_MD_DIR/m0d-$REMOTE_IOS_FID/gen_id | grep 'segment genid' | cut -d \"(\" -f2 | cut -d \")\" -f1")
        fi
    fi

    [[ $LOCAL_SEG_GEN_ID  -eq 0 ]] && [[ $REMOTE_SEG_GEN_ID -eq 0 ]] && die "Segment header not found"
    [[ $LOCAL_SEG_GEN_ID  -ne 0 ]] || LOCAL_SEG_GEN_ID=$REMOTE_SEG_GEN_ID
    [[ $REMOTE_SEG_GEN_ID -ne 0 ]] || REMOTE_SEG_GEN_ID=$LOCAL_SEG_GEN_ID
    return $exec_status
}

# This function will do a mkfs and mount the specified device on the specified node
makefs_and_mount() {
    local node=$1
    local dir=$2
    local device=$3
    run_cmd "$node" "mkfs.ext4 $device"
    run_cmd "$node" "mount $device $dir"
}

# This function unmount the Metadata device (if it is already mounted) and run fsck tool on it.
# After fsck, if mount has failed for Metadata Device then then we format
# Metadata device with mkfs.ext4 and mount it again.
run_fsck() {
    local exec_status=0

    unmount_device "$LOCAL_NODE" "$MD_DIR" "MD Device might be busy on local node, please try shutting \
                    down cluster and retry" "MD device is not mounted on local node."
    m0drlog "Running fsck on local node"
    run_cmd "$LOCAL_NODE" "timeout 5m fsck -y $LOCAL_MOTR_DEVICE"
    [[ $? -eq 0 ]] || m0drlog "ERROR: fsck command failed on local node"

    if [[ $REMOTE_STORAGE_STATUS == 0 ]]; then
        unmount_device "$REMOTE_NODE" "$MD_DIR" "MD Device might be busy on remote node, please try shutting \
                        down cluster and retry" "MD device is not mounted on remote node."
        m0drlog "Running fsck on remote node"
        run_cmd "$REMOTE_NODE" "timeout 5m fsck -y $REMOTE_MOTR_DEVICE"
        [[ $? -eq 0 ]] || m0drlog "ERROR : fsck command failed on remote node"
    else
        unmount_device "$LOCAL_NODE" "$FAILOVER_MD_DIR" "MD Device of remote node might be busy on local node, please try shutting \
                        down cluster and retry" "MD device of remote not mounted on local node."
        m0drlog "Running fsck for remote node from local node."
        run_cmd "$LOCAL_NODE" "timeout 5m fsck -y $REMOTE_MOTR_DEVICE"
        [[ $? -eq 0 ]] || m0drlog "ERROR : fsck command failed for remote node"
    fi

    # Try to mount the metadata device on /var/motr on both nodes
    if run_cmd "$LOCAL_NODE" "mount $LOCAL_MOTR_DEVICE $MD_DIR"; then
        # create directory "datarecovery" in /var/motr if not exist to store traces
        run_cmd "$LOCAL_NODE" "mkdir -p $MD_DIR/datarecovery"
    else
        makefs_and_mount "$LOCAL_NODE" "$MD_DIR" "$LOCAL_MOTR_DEVICE"
        (( exec_status|=1));
    fi

    if [[ $REMOTE_STORAGE_STATUS == 0 ]]; then
        if run_cmd "$REMOTE_NODE" "mount $REMOTE_MOTR_DEVICE $MD_DIR"; then
            # create directory "datarecovery" in /var/motr if not exist to store traces
            run_cmd "$REMOTE_NODE" "mkdir -p $MD_DIR/datarecovery"
        else
            makefs_and_mount "$REMOTE_NODE" "$MD_DIR" "$REMOTE_MOTR_DEVICE"
            (( exec_status|=2));
        fi
    else
        if run_cmd "$LOCAL_NODE" "mount $REMOTE_MOTR_DEVICE $FAILOVER_MD_DIR"; then
            # create directory "datarecovery" in /var/motr if not exist to store traces
            run_cmd "$LOCAL_NODE" "mkdir -p $FAILOVER_MD_DIR/datarecovery"
        else
            makefs_and_mount "$LOCAL_NODE" "$FAILOVER_MD_DIR" "$REMOTE_MOTR_DEVICE"
            (( exec_status|=2));
        fi
    fi

    return $exec_status
}

# This function remove the swap device on specified node
remove_swap() {
    local node=$1
    local device=$2
    run_cmd "$node" "swapoff -a"
    run_cmd "$node" "lvremove -f $device"
}

# This function remove the swap device on both nodes
remove_swap_all_nodes() {
    local exec_status=0
    if [[ $LOCAL_NODE_RECOVERY_STATE == $RSTATE3 ]]; then
        m0drlog "Running remove swap on local node"
        remove_swap "$LOCAL_NODE" "$LOCAL_SWAP_DEVICE"
        [[ $? -eq 0 ]] || { (( exec_status|=1)); }
    fi

    if [[ $REMOTE_STORAGE_STATUS == 0 ]] && [[ $REMOTE_NODE_RECOVERY_STATE == $RSTATE3 ]]; then
        m0drlog "Running remove swap on remote node"
        remove_swap "$REMOTE_NODE" "$REMOTE_LV_SWAP_DEVICE"
        [[ $? -eq 0 ]] || { (( exec_status|=2)); }
    elif [[ $REMOTE_NODE_RECOVERY_STATE == $RSTATE3 ]]; then
        m0drlog "Running remove swap for remote node from local node"
        remove_swap "$LOCAL_NODE" "$REMOTE_LV_SWAP_DEVICE"
        [[ $? -eq 0 ]] || { (( exec_status|=2)); }
    fi

    return $exec_status
}

# This function will create the snapshot on specified node
create_snapshot() {
    local node=$1
    local snap=$2
    local device=$3
    run_cmd "$node" "lvcreate -pr -l 100%FREE -s -n $snap $device"
}

# This function will create the snapshot of metadata on both nodes
create_snapshot_all_nodes() {
    local exec_status=0

    if [[ $LOCAL_NODE_RECOVERY_STATE == $RSTATE2 ]] || [[ $LOCAL_NODE_RECOVERY_STATE == $RSTATE3 ]]; then
        m0drlog "Running create snapshot on local node"
        create_snapshot "$LOCAL_NODE" "$SNAPSHOT" "$LOCAL_LV_MD_DEVICE"
        [[ $? -eq 0 ]] || { (( exec_status|=1)); }
    fi

    if [[ $REMOTE_STORAGE_STATUS == 0 ]] && ([[ $REMOTE_NODE_RECOVERY_STATE == $RSTATE2 ]] || [[ $REMOTE_NODE_RECOVERY_STATE == $RSTATE3 ]]); then
        m0drlog "Running create snapshot on remote node"
        create_snapshot "$REMOTE_NODE" "$SNAPSHOT" "$REMOTE_LV_MD_DEVICE"
        [[ $? -eq 0 ]] || { (( exec_status|=2)); }
    elif [[ $REMOTE_NODE_RECOVERY_STATE == $RSTATE2 ]] || [[ $REMOTE_NODE_RECOVERY_STATE == $RSTATE3 ]]; then
        m0drlog "Running create snapshot for remote node from local node"
        create_snapshot "$LOCAL_NODE" "$SNAPSHOT" "$REMOTE_LV_MD_DEVICE"
        [[ $? -eq 0 ]] || { (( exec_status|=2)); }
    fi

    return $exec_status
}

# This function will remove the snapshot from the specified node
remove_snapshot() {
    local node=$1
    local vol_grp=$2
    local snap=$3
    LV_SNAPSHOT=$(run_cmd "$node" "lvs -o path $vol_grp | grep $SNAPSHOT ")
    run_cmd "$node" "lvremove -f $LV_SNAPSHOT"
}

# This function will remove the metadata snapshot on both nodes
remove_snapshot_all_nodes() {
    local exec_status=0

    m0drlog "Running remove snapshot on local node"
    run_cmd "$LOCAL_NODE" "pvscan --cache"; # Ensure that volumes configs are synced
    remove_snapshot "$LOCAL_NODE" "$LOCAL_MD_VOLUMEGROUP"
    [[ $? -eq 0 ]] || { (( exec_status|=1)); }

    if [[ $REMOTE_STORAGE_STATUS == 0 ]]; then
        m0drlog "Running remove snapshot on remote node"
        run_cmd "$REMOTE_NODE" "pvscan --cache"; # Ensure that volumes configs are synced
        remove_snapshot "$REMOTE_NODE" "$REMOTE_MD_VOLUMEGROUP"
        [[ $? -eq 0 ]] || { (( exec_status|=2)); }
    else
        m0drlog "Running remove snapshot for remote node from local node"
        remove_snapshot "$LOCAL_NODE" "$REMOTE_MD_VOLUMEGROUP"
        [[ $? -eq 0 ]] || { (( exec_status|=2)); }
    fi

    return $exec_status
}

# This function will create swap device on the specified node
# `pvscan --cache` is used repeatedly because we need to ensure system is in
# correct state after recovery.
create_swap() {
    local node=$1
    local volgrp=$2
    local print_param=$3
    run_cmd "$node" "pvscan --cache"; # Ensure that volumes configs are synced
    run_cmd "$node" "lvcreate -n $SWAP_DEVICE -l 100%FREE $volgrp"
    run_cmd "$node" "pvscan --cache"; # Ensure that volumes configs are synced
    return $(run_cmd "$node" "lvs -o path $volgrp | grep $SWAP_DEVICE | awk '{ print $print_param }'")
}

# This function will enable the swap device on the specified node
enable_swap() {
    local node=$1
    local swap_device=$2
    run_cmd "$node" "mkswap -f $swap_device"
    run_cmd "$node" "swapon -a"
}

# This function will create the swap device on both nodes
# `pvscan --cache` is used repeatedly because we need to ensure system is in
# correct state after recovery.
create_swap_all_nodes() {
    local exec_status=0

    m0drlog "Running create swap on local node"
    LOCAL_SWAP_DEVICE=$(create_swap "$LOCAL_NODE" "$LOCAL_MD_VOLUMEGROUP" "$1")
    enable_swap "$LOCAL_NODE" "$LOCAL_LV_SWAP_DEVICE"
    [[ $? -eq 0 ]] || { (( exec_status|=1)); }

    m0drlog "Running create swap on remote node"
    if [[ $REMOTE_STORAGE_STATUS == 0 ]]; then
        REMOTE_LV_SWAP_DEVICE=$(create_swap "$REMOTE_NODE" "$REMOTE_MD_VOLUMEGROUP" "\$1")
        enable_swap "$REMOTE_NODE" "$REMOTE_LV_SWAP_DEVICE"
        [[ $? -eq 0 ]] || { (( exec_status|=2)); }
    else
        m0drlog "Running create swap for remote node from local node"
        REMOTE_LV_SWAP_DEVICE=$(create_swap "$LOCAL_NODE" "$REMOTE_MD_VOLUMEGROUP" "\$1")
        enable_swap "$LOCAL_NODE" "$REMOTE_LV_SWAP_DEVICE"
        [[ $? -eq 0 ]] || { (( exec_status|=2)); }
    fi

    return $exec_status
}

# Clean up /var/motr/m0d-IOS_FID/stobs/o directory in case of beck tool crash from both nodes
# and umount /var/motr
cleanup_stobs_dir() {
    sleep 10 # Wait is needed to ensure both nodes process are stopped
    run_cmd "$LOCAL_NODE" "rm -f /var/motr/m0d-$LOCAL_IOS_FID/stobs/o/*"
    run_cmd "$REMOTE_NODE" "rm -f /var/motr/m0d-$REMOTE_IOS_FID/stobs/o/*"
    run_cmd "$LOCAL_NODE" "umount $MD_DIR" > /dev/null
    run_cmd "$REMOTE_NODE" "umount $MD_DIR" > /dev/null
    [[ $REMOTE_STORAGE_STATUS -eq 0 ]] || run_cmd "$LOCAL_NODE" "umount $FAILOVER_MD_DIR" > /dev/null
}

check_and_mount() {
    local node=$1
    local dir=$2
    local device=$3
    local failure_msg=$4
    if ! run_cmd "$node" "mountpoint -q $dir"; then
        run_cmd "$node" "mount $device $dir";
        [[ $? -eq 0 ]] || die "$failure_msg"
        # create directory "datarecovery" in /var/motr if not exist to store traces
        run_cmd "$node" "mkdir -p $dir/datarecovery"
    fi
}

becktool() {
    local node=$1
    local dir=$2
    local src=$3
    local dest=$4
    local genid=$5
    return $(run_cmd "$node" "(cd $dir/datarecovery; $BECKTOOL -s $src -d $dest/db -a $dest/stobs -g $genid;)")
}

# The return statements between { .. }& are to indicate the exit status of
# child/background process that is spawned not for the function exit status.
# This function will run beck tool on both nodes
run_becktool() {
    local exec_status=0

    # m0betool and m0beck depend on motr-kernel service so we try to get service up;
    # if this service does not start in 3 attempt on local node and remote node
    # then we cannot proceed further in the recovery.
    start_motr_kernel_service "$LOCAL_NODE" "motr-kernel service is not starting on local node, cannot proceed recovery further."

    if [[ $REMOTE_STORAGE_STATUS == 0 ]]; then
        start_motr_kernel_service "$REMOTE_NODE" "motr-kernel service is not starting on remote node, cannot proceed recovery further."
    fi

    # Before running beck tool make sure that metadata directories are mounted
    check_and_mount "$LOCAL_NODE" "$MD_DIR" "$LOCAL_MOTR_DEVICE $MD_DIR" "Cannot mount metadata device on /var/motr on local node"

    if [[ $REMOTE_STORAGE_STATUS == 0 ]]; then
        check_and_mount "$REMOTE_NODE" "$MD_DIR" "$REMOTE_MOTR_DEVICE" "Cannot mount metadata device on /var/motr on remote node"
    else
        check_and_mount "$LOCAL_NODE" "$FAILOVER_MD_DIR" "$REMOTE_MOTR_DEVICE" "Cannot mount metadata device on failover dir on local node"
    fi

    run_cmd "$LOCAL_NODE" "pvscan --cache"; # Ensure that volumes configs are synced
    # Running beck tool parallelly on both nodes as it is long running operation
    {
        local_beck_status=0
        SOURCE_IMAGE=$(get_lvs_info "$LOCAL_NODE" "path" "$LOCAL_MD_VOLUMEGROUP" "$SNAPSHOT ")
        DEST_DOMAIN_DIR="$MD_DIR/m0d-$LOCAL_IOS_FID"

        m0drlog "Running Becktool on local node"
        becktool "$LOCAL_NODE" "$MD_DIR" "$SOURCE_IMAGE" "$DEST_DOMAIN_DIR" "$LOCAL_SEG_GEN_ID"
        cmd_exit_status=$?
        # restart the execution of command if exit code is ESEGV error
        while [[ $cmd_exit_status == $ESEGV ]];
        do
            m0drlog "Restarting Becktool on local node"
            becktool "$LOCAL_NODE" "$MD_DIR" "$SOURCE_IMAGE" "$DEST_DOMAIN_DIR" "$LOCAL_SEG_GEN_ID"
            cmd_exit_status=$?
        done

        [[ $cmd_exit_status == 0 ]] && local_beck_status=0 || local_beck_status=$cmd_exit_status

        return $local_beck_status
    }&
    local_cmd_pid="$!"

    {
        remote_beck_status=0
        if [[ $REMOTE_STORAGE_STATUS == 0 ]]; then
            run_cmd "$REMOTE_NODE" "pvscan --cache"; # Ensure that volumes configs are synced
            SOURCE_IMAGE=$(get_lvs_info "$REMOTE_NODE" "path" "$REMOTE_MD_VOLUMEGROUP" "$SNAPSHOT")
            DEST_DOMAIN_DIR="$MD_DIR/m0d-$REMOTE_IOS_FID"

            m0drlog "Running Becktool on remote node"
            becktool "$REMOTE_NODE" "$MD_DIR" "$SOURCE_IMAGE" "$DEST_DOMAIN_DIR" "$REMOTE_SEG_GEN_ID"
            cmd_exit_status=$?
            # restart the execution of command if exit code is ESEGV error
            while [[ $cmd_exit_status == $ESEGV ]];
            do
                m0drlog "Restarting Becktool on remote node"
                becktool "$REMOTE_NODE" "$MD_DIR" "$SOURCE_IMAGE" "$DEST_DOMAIN_DIR" "$REMOTE_SEG_GEN_ID"
                cmd_exit_status=$?
            done

            [[ $cmd_exit_status == 0 ]] && remote_beck_status=0 || remote_beck_status=$cmd_exit_status

            return $remote_beck_status
        else
            SOURCE_IMAGE=$(get_lvs_info "$LOCAL_NODE" "path" "$REMOTE_MD_VOLUMEGROUP" "$SNAPSHOT" )
            DEST_DOMAIN_DIR="$FAILOVER_MD_DIR/m0d-$REMOTE_IOS_FID"

            m0drlog "Running Becktool for remote node from local node"
            becktool "$LOCAL_NODE" "$FAILOVER_MD_DIR" "$SOURCE_IMAGE" "$DEST_DOMAIN_DIR" "$REMOTE_SEG_GEN_ID"
            cmd_exit_status=$?
            # restart the execution of command if exit code is ESEGV error
            while [[ $cmd_exit_status == $ESEGV ]];
            do
                m0drlog "Restarting Becktool for remote node from local node"
                becktool "$LOCAL_NODE" "$FAILOVER_MD_DIR" "$SOURCE_IMAGE" "$DEST_DOMAIN_DIR" "$REMOTE_SEG_GEN_ID"
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
            pkill -9 m0beck; run_cmd "$REMOTE_NODE" "pkill -9 m0beck";
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
    remove_snapshot_all_nodes      # remove snapshot on both nodes
    remove_snapshot_status=$?
    [[ remove_snapshot_status -eq 0 ]] || { die "ERROR: Remove snapshot failed with code $remove_snapshot_status"; }

    create_swap_all_nodes          # recreate swap on both nodes
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

remove_swap_all_nodes          # remove swap on both nodes
remove_swap_status=$?
[[ remove_swap_status -eq 0 ]] || { die "ERROR: Remove swap failed with code $remove_swap_status"; }

create_snapshot_all_nodes      # create snapshot on both node
create_snapshot_status=$?
[[ create_snapshot_status -eq 0 ]] || { die "ERROR: Create snapshot failed with code $create_snapshot_status"; }

reinit_mkfs                    # reinit mkfs only on both nodes

run_becktool                   # run becktool on both nodes
run_becktool_status=$?
[[ run_becktool_status -eq 0 ]] || { die "ERROR: Run becktool failed with code $run_becktool_status"; }

remove_snapshot_all_nodes      # remove snapshot on both nodes
remove_snapshot_status=$?
[[ remove_snapshot_status -eq 0 ]] || { die "ERROR: Remove snapshot failed with code $remove_snapshot_status"; }

create_swap_all_nodes          # recreate swap on both nodes
create_swap_status=$?
[[ create_swap_status -eq 0 ]] || { die "ERROR: Create swap failed with code $create_swap_status"; }

exit 0 # script exit code
