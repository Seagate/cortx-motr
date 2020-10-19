#!/usr/bin/env bash
# set -x

current_time=$(date +"%Y_%m_%d__%H_%M_%S")
LOG_DIR="/var/log/seagate/motr"
mkdir -p $LOG_DIR
LOG_FILE="$LOG_DIR/be_log_resize_$current_time.log"
touch $LOG_FILE
MOTR_CONF_FILE="/etc/sysconfig/motr"
M0BETOOL="m0betool"
MD_DIR="/var/motr"
BE_REPLY="be_recovery_run"
BE_RESIZE="be_log_resize"

local_md_volume=""
remote_md_volume=""
new_be_log_size=""
current_be_log_size=""
local_node=""
remote_node=""
loca_ios_fid=""
remote_ios_fid=""

be_resize_log() {
    echo "[`date`]: $@" 2>&1 | tee -a ${LOG_FILE}
}

be_resize_soft_exit() { be_resize_log "$@" ; exit 0; }

be_resize_hard_exit() { be_resize_log "$@" ; exit 1; }

# Verifying cluster status
check_cluster_status() {
	if  [[ $((hctl status) 2>&1 | grep "Cluster is not running") ]]; then
		be_resize_log "cluster is not running"
	else
		be_resize_hard_exit "cluster should be stop for BE log resize"
	fi
}

# Extract current BE Log file size
get_be_log_size() {
	new_be_log_size=`grep -w MOTR_M0D_BELOG_SIZE $MOTR_CONF_FILE | grep -v '#' | cut -d"=" -f2`
	if [[ -z $new_be_log_size ]]; then
		be_resize_soft_exit "BELOG Size is not configured"
	fi
	be_resize_log "Given BE log size: $new_be_log_size"
}

# The function will execute command on the local node
run_cmd_on_local_node() {
    local cmd=$1
    be_resize_log "[`date`]: Running '$cmd'" >> ${LOG_FILE}
    eval "$cmd" 2>&1 | tee -a ${LOG_FILE}
    return ${PIPESTATUS[0]} # return exit status of previous command
}

# The function will execute command on the remote node
run_cmd_on_remote_node() {
    local cmd=$1
    be_resize_log "[`date`]: Running 'ssh root@$remote_node $cmd'" >> ${LOG_FILE}
    ssh root@$remote_node "$cmd" 2>&1 | tee -a ${LOG_FILE}
    return ${PIPESTATUS[0]} # return exit status of previous command
}

# Mount metadata devices on local and remote nodes
mount_md_device() {

	if [[ $(cat /var/lib/hare/node-name) == "srvnode-1" ]]; then
		local_node="srvnode-1"
		remote_node="srvnode-2"
		local_md_volume=$(grep left-volume /opt/seagate/cortx/ha/conf/build-ees-ha-args.yaml |\
				    awk '{ print $2 }')
		remote_md_volume=$(grep right-volume /opt/seagate/cortx/ha/conf/build-ees-ha-args.yaml |\
				    awk '{ print $2 }')

	elif [[ $(cat /var/lib/hare/node-name) == "srvnode-2" ]]; then
		local_node="srvnode-2"
		remote_node="srvnode-1"
		local_md_volume=$(grep right-volume /opt/seagate/cortx/ha/conf/build-ees-ha-args.yaml |\
				    awk '{ print $2 }')
		remote_md_volume=$(grep left-volume /opt/seagate/cortx/ha/conf/build-ees-ha-args.yaml |\
				    awk '{ print $2 }')
	else
		be_resize_hard_exit "ERROR: node-name is not avaialble"
	fi

	if ! run_cmd_on_local_node "mount $local_md_volume $MD_DIR"; then
		be_resize_hard_exit "ERROR: local node metadata mount is failed"
	fi

	if ! run_cmd_on_remote_node "mount $remote_md_volume $MD_DIR"; then
		unmount_md_device
		be_resize_hard_exit "ERROR: remote node metadata mount is failed"
	fi
	be_resize_log "Completed mount_md_device"
}

# This function will initialize values of local and remote ioservices fid
get_ios_fid() {
	local_ios_fid=$(cat /etc/sysconfig/m0d-0x7200000000000001\:0x* | grep "$local_node" -B 1 | grep FID | cut -f 2 -d "="| tr -d \')
	remote_ios_fid=$(cat /etc/sysconfig/m0d-0x7200000000000001\:0x* | grep "$remote_node" -B 1 | grep FID | cut -f 2 -d "="| tr -d \')

	if [[ $local_ios_fid == "" ]] || [[ $remote_ios_fid == "" ]]; then
		unmount_md_device
		be_resize_hard_exit "Failed to get ioservice FIDs"
	fi
	be_resize_log "Completed get_ios_fid"
}

# Validating current BELOG file size and New BELOG file size
verify_be_log_size() {
	current_be_log_size=$(ls -l $MD_DIR/m0d-$local_ios_fid/db-log/o/0\:28 | awk '{ print $5 }')

	if [[ $current_be_log_size == "" ]]; then
		unmount_md_device
		be_resize_hard_exit "Failed to get existing be log size"
	fi

	if [[ $current_be_log_size -eq $new_be_log_size ]]; then
		unmount_md_device
		be_resize_soft_exit "No need to modify due to both sizes are equal"
	fi
	be_resize_log "Completed verify_be_log_size"
}

# Reply exist BE log
be_log_reply() {
	be_resize_log "Reply: $M0BETOOL $BE_REPLY $MD_DIR/m0d-$local_ios_fid/db"

	if ! run_cmd_on_local_node "$M0BETOOL $BE_REPLY $MD_DIR/m0d-$local_ios_fid/db"; then
		unmount_md_device
		be_resize_hard_exit "ERROR: Log reply is failed in local node"
	fi

	if ! run_cmd_on_remote_node "$M0BETOOL $BE_REPLY $MD_DIR/m0d-$remote_ios_fid/db"; then
		unmount_md_device
		be_resize_hard_exit "ERROR: Log reply is failed in remote node"
	fi
	be_resize_log "Completed be_log_reply"
}

# Resizing BE log file size with new BE log size
be_log_resize() {
	be_resize_log "Resize: $M0BETOOL $BE_RESIZE $MD_DIR/m0d-$local_ios_fid/db $new_be_log_size"

	if ! run_cmd_on_local_node "$M0BETOOL $BE_RESIZE $MD_DIR/m0d-$local_ios_fid/db $new_be_log_size"; then
		unmount_md_device
		be_resize_hard_exit "ERROR: Log resize is failed in local node"
	fi

	if ! run_cmd_on_remote_node "$M0BETOOL $BE_RESIZE $MD_DIR/m0d-$remote_ios_fid/db $new_be_log_size"; then
		unmount_md_device
		be_resize_hard_exit "ERROR: Log resize is failed in remote node"
	fi
	be_resize_log "Completed be_log_resize"
}

# Verifying BE log file size after resizing
verify_be_log_resize() {
	current_be_log_size=$(ls -l $MD_DIR/m0d-$local_ios_fid/db-log/o/0\:28 | awk '{ print $5 }')

	if [[ $current_be_log_size == "" ]]; then
		unmount_md_device
		be_resize_hard_exit "Failed to get existing be log size"
	fi

	if [[ $current_be_log_size -ne $new_be_log_size ]]; then
		be_resize_soft_exit "No need to modify due to both sizes are equal"
	fi
	be_resize_log "Completed verify_be_log_resize"
}

#Unmount metadata devices
unmount_md_device() {
	if ! run_cmd_on_local_node "umount $MD_DIR "; then
		be_resize_hard_exit "ERROR: Unmount MD is failed in local node"
	fi

	if ! run_cmd_on_remote_node "umount $MD_DIR"; then
		be_resize_hard_exit "ERROR: Unmount MD is failed in remote node"
	fi
	be_resize_log "Completed unmount_md_device"
}

check_cluster_status
get_be_log_size
mount_md_device
get_ios_fid
verify_be_log_size
be_log_reply
be_log_resize
verify_be_log_resize
unmount_md_device
