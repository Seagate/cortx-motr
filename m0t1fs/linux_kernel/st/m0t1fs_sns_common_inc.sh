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

M0_SRC_DIR=$(readlink -f ${BASH_SOURCE[0]})
M0_SRC_DIR=${M0_SRC_DIR%/*/*/*/*}

. $M0_SRC_DIR/utils/functions  # m0_local_nid_get

M0_NC_UNKNOWN=1
M0_NC_ONLINE=1
M0_NC_FAILED=2
M0_NC_TRANSIENT=3
M0_NC_REPAIR=4
M0_NC_REPAIRED=5
M0_NC_REBALANCE=6

CM_OP_REPAIR=1
CM_OP_REBALANCE=2
CM_OP_REPAIR_QUIESCE=3
CM_OP_REBALANCE_QUIESCE=4
CM_OP_REPAIR_RESUME=5
CM_OP_REBALANCE_RESUME=6
CM_OP_REPAIR_STATUS=7
CM_OP_REBALANCE_STATUS=8
CM_OP_REPAIR_ABORT=9
CM_OP_REBALANCE_ABORT=10

export N=2
export K=1
export S=1
export P=4
stride=32
src_bs=10M
src_count=2

file=(
	10000:10000
	10001:10001
	10002:10002
)

file_size=(
	50
	70
	30
)

unit_size=(
	$stride
	$stride
	$stride
)

declare -A ha_states=(
	[unknown]=$M0_NC_UNKNOWN
	[online]=$M0_NC_ONLINE
	[failed]=$M0_NC_FAILED
	[transient]=$M0_NC_TRANSIENT
	[repair]=$M0_NC_REPAIR
	[repaired]=$M0_NC_REPAIRED
	[rebalance]=$M0_NC_REBALANCE
)

ha_events_post()
{
	local service_eps=($1)
		local state=$2
		local state_num=${ha_states[$state]}
	if [ -z "$state_num" ]; then
		echo "Unknown state: $state"
		return 1
	fi

	shift 2

	local fids=()
	local nr=0
	for d in "$@"; do
		fids[$nr]="^k|1:$d"
		nr=$((nr + 1))
	done

	local local_ep="$lnet_nid$M0HAM_CLI_EP"

	echo "ha_events_post: ${service_eps[*]}"
	echo "setting devices { ${fids[*]} } to $state"
	send_ha_events "${fids[*]}" "$state" "${service_eps[*]}" "$local_ep"
	rc=$?
	if [ $rc -ne 0 ]; then
		echo "HA note set failed: $rc"
		unmount_and_clean &>> $MOTR_TEST_LOGFILE
		return $rc
	fi
}

# input parameters:
# (i) state name
# (ii) disk1
# (iii) ...
disk_state_set()
{
	local state=$1
	local state_num=${ha_states[$state]}
	if [ -z "$state_num" ]; then
		echo "Unknown state: $state"
		return 1
	fi

	shift

	local fids=()
	local nr=0
	for d in "$@"; do
		fids[$nr]="^k|1:$d"
		nr=$((nr + 1))
	done

	# Dummy HA doesn't broadcast messages. Therefore, send ha_msg to the
	# services directly.
	local service_eps=$(service_eps_with_m0t1fs_get)
	local local_ep="$lnet_nid$M0HAM_CLI_EP"

	echo "setting devices { ${fids[*]} } to $state"
	send_ha_events "${fids[*]}" "$state" "$service_eps" "$local_ep"
	rc=$?
	if [ $rc -ne 0 ]; then
		echo "HA note set failed: $rc"
		unmount_and_clean &>> $MOTR_TEST_LOGFILE
		return $rc
	fi
}

cas_disk_state_set()
{
	local local_ep="$lnet_nid$M0HAM_CLI_EP"
	local state=$1
	local state_num=${ha_states[$state]}
	if [ -z "$state_num" ]; then
		echo "Unknown state: $state"
		return 1
	fi
	shift
	local service_eps=$(service_cas_eps_with_m0tifs_get)
	local fids=()
	local nr=0

	echo "Setting CAS device { $@ } to $state (HA state=$state_num)"

	for d in "$@";  do
		fids[$nr]="^k|20:$d"
		nr=$((nr + 1))
	done
	echo "setting devices { ${fids[*]} } to $state"
	send_ha_events "${fids[*]}" "$state" "$service_eps" "$local_ep"
	rc=$?
	if [ $rc -ne 0 ]; then
		echo "HA note set failed: $rc"
		unmount_and_clean &>> $MOTR_TEST_LOGFILE
		return $rc
	fi
	return 0
}

disk_state_get()
{
	local fids=()
	local nr=0
	for d in "$@"; do
		fids[$nr]="^k|1:$d"
		nr=$((nr + 1))
	done

	local service_eps=$(service_eps_with_m0t1fs_get)
	local local_ep="$lnet_nid$M0HAM_CLI_EP"

	echo "getting device { ${fids[*]} }'s HA state"
	request_ha_state "${fids[*]}" "$service_eps" "$local_ep"
	rc=$?
	if [ $rc != 0 ]; then
		echo "HA state get failed: $rc"
		unmount_and_clean &>> $MOTR_TEST_LOGFILE
		return $rc
	fi
}

cas_disk_state_get()
{
	local service_eps=$(service_cas_eps_with_m0tifs_get)
	local local_ep="$lnet_nid$M0HAM_CLI_EP"
	local nr=0

	echo "getting device { $@ }'s HA state"

	for d in "$@";	do
		fids[$nr]="^k|20:$d"
		nr=$((nr + 1))
	done
	echo "getting device { ${fids[*]} }'s HA state"
	request_ha_state "${fids[*]}" "$service_eps" "$local_ep"
	rc=$?
	if [ $rc != 0 ]; then
		echo "HA tate get failed: $rc"
		unmount_and_clean &>> $MOTR_TEST_LOGFILE
		return $rc
	fi
	return 0
}

sns_repair_mount()
{
	mount_m0t1fs $MOTR_M0T1FS_MOUNT_DIR "oostore,verify" || {
		echo "mount failed"
		return 1
	}
}

sns_repair()
{
	local rc=0

	repair_trigger="$M0_SRC_DIR/sns/cm/st/m0repair -O $CM_OP_REPAIR -t 0 -C ${lnet_nid}:${SNS_CLI_EP} $ios_eps"
	echo $repair_trigger
	eval $repair_trigger
	rc=$?
	if [ $rc != 0 ]; then
		echo "SNS Repair failed"
	fi

	return $rc
}

sns_rebalance()
{
	local rc=0

        rebalance_trigger="$M0_SRC_DIR/sns/cm/st/m0repair -O $CM_OP_REBALANCE -t 0 -C ${lnet_nid}:${SNS_CLI_EP} $ios_eps"
        echo $rebalance_trigger
	eval $rebalance_trigger
	rc=$?
        if [ $rc != 0 ] ; then
                echo "SNS Re-balance failed"
        fi

	return $rc
}

sns_repair_quiesce()
{
	local rc=0

	repair_quiesce_trigger="$M0_SRC_DIR/sns/cm/st/m0repair -O $CM_OP_REPAIR_QUIESCE -t 0 -C ${lnet_nid}:${SNS_QUIESCE_CLI_EP} $ios_eps"
	echo $repair_quiesce_trigger
	eval $repair_quiesce_trigger
	rc=$?
	if [ $rc != 0 ]; then
		echo "SNS Repair quiesce failed"
	fi

	return $rc
}

sns_rebalance_quiesce()
{
	local rc=0

	rebalance_quiesce_trigger="$M0_SRC_DIR/sns/cm/st/m0repair -O $CM_OP_REBALANCE_QUIESCE -t 0 -C ${lnet_nid}:${SNS_QUIESCE_CLI_EP} $ios_eps"
	echo $rebalance_quiesce_trigger
	eval $rebalance_quiesce_trigger
	rc=$?
	if [ $rc != 0 ] ; then
		echo "SNS Re-balance quiesce failed"
	fi

	return $rc
}

sns_repair_resume()
{
	local rc=0

	repair_resume_trigger="$M0_SRC_DIR/sns/cm/st/m0repair -O $CM_OP_REPAIR_RESUME -t 0 -C ${lnet_nid}:${SNS_CLI_EP} $ios_eps"
	echo $repair_resume_trigger
	eval $repair_resume_trigger
	rc=$?
	if [ $rc != 0 ]; then
		echo "SNS Repair failed"
	fi

	return $rc
}

sns_rebalance_resume()
{
	local rc=0

	repair_resume_trigger="$M0_SRC_DIR/sns/cm/st/m0repair -O $CM_OP_REBALANCE_RESUME -t 0 -C ${lnet_nid}:${SNS_CLI_EP} $ios_eps"
	echo $repair_resume_trigger
	eval $repair_resume_trigger
	rc=$?
	if [ $rc != 0 ]; then
		echo "SNS Repair failed"
	fi

	return $rc
}

sns_rebalance_abort()
{
	local rc=0

	rebalance_abort_trigger="$M0_SRC_DIR/sns/cm/st/m0repair -O $CM_OP_REBALANCE_ABORT -t 0 -C ${lnet_nid}:${SNS_QUIESCE_CLI_EP} $ios_eps"
	echo $rebalance_abort_trigger
	eval $rebalance_abort_trigger
	rc=$?
	if [ $rc != 0 ] ; then
		echo "SNS Re-balance abort failed"
	fi

	return $rc
}

sns_repair_abort()
{
	local rc=0

	repair_abort_trigger="$M0_SRC_DIR/sns/cm/st/m0repair -O $CM_OP_REPAIR_ABORT -t 0 -C ${lnet_nid}:${SNS_QUIESCE_CLI_EP} $ios_eps"
	echo $repair_abort_trigger
	eval $repair_abort_trigger
	rc=$?
	echo "SNS abort cmd sent: rc=$rc"
	if [ $rc != 0 ]; then
		echo "SNS Repair abort failed"
	fi

	return $rc
}

sns_repair_abort_skip_4()
{
	local ios_eps_not_4=''
	local rc=0

	for ((i=0; i < 3; i++)) ; do
		ios_eps_not_4="$ios_eps_not_4 -S ${lnet_nid}:${IOSEP[$i]}"
	done

	repair_abort_trigger="$M0_SRC_DIR/sns/cm/st/m0repair -O $CM_OP_REPAIR_ABORT -C ${lnet_nid}:${SNS_QUIESCE_CLI_EP} $ios_eps_not_4"
	echo $repair_abort_trigger
	eval $repair_abort_trigger
	rc=$?
	echo "SNS abort cmd sent: rc=$rc"
	if [ $rc != 0 ]; then
		echo "SNS Repair abort failed"
	fi

	return $rc
}

sns_repair_or_rebalance_status_not_4()
{
	local ios_eps_not_4=''
	local rc=0
	local op=32
	[ "$1" == "repair" ] && op=$CM_OP_REPAIR_STATUS
	[ "$1" == "rebalance" ] && op=$CM_OP_REBALANCE_STATUS

	for ((i=0; i < 3; i++)) ; do
		ios_eps_not_4="$ios_eps_not_4 -S ${lnet_nid}:${IOSEP[$i]}"
	done

	repair_status="$M0_SRC_DIR/sns/cm/st/m0repair -O $op -t 0 -C ${lnet_nid}:${SNS_QUIESCE_CLI_EP} $ios_eps_not_4"
	echo $repair_status
	eval $repair_status
	rc=$?
	if [ $rc != 0 ]; then
		echo "SNS Repair status query failed"
	fi

	return $rc
}


sns_repair_or_rebalance_status()
{
	local rc=0
	local op=32
	[ "$1" == "repair" ] && op=$CM_OP_REPAIR_STATUS
	[ "$1" == "rebalance" ] && op=$CM_OP_REBALANCE_STATUS

	repair_status="$M0_SRC_DIR/sns/cm/st/m0repair -O $op -t 0 -C ${lnet_nid}:${SNS_QUIESCE_CLI_EP} $ios_eps"
	echo $repair_status
	eval $repair_status
	rc=$?
	if [ $rc != 0 ]; then
		echo "SNS Repair status query failed"
	fi

	return $rc
}

wait_for_sns_repair_or_rebalance_not_4()
{
	local ios_eps_not_4=''
	local rc=0
	local op=32
	[ "$1" == "repair" ] && op=$CM_OP_REPAIR_STATUS
	[ "$1" == "rebalance" ] && op=$CM_OP_REBALANCE_STATUS
	for ((i=0; i < 3; i++)) ; do
		ios_eps_not_4="$ios_eps_not_4 -S ${lnet_nid}:${IOSEP[$i]}"
	done
	while true ; do
		sleep 5
		repair_status="$M0_SRC_DIR/sns/cm/st/m0repair -O $op -t 0 -C ${lnet_nid}:${SNS_QUIESCE_CLI_EP} $ios_eps_not_4"

		echo $repair_status
		status=`eval $repair_status`
		rc=$?
		if [ $rc != 0 ]; then
			echo "SNS Repair status query failed"
			return $rc
		fi

		echo $status | grep status=2 && continue #sns repair is active, continue waiting
		break;
	done
	return 0
}

wait_for_sns_repair_or_rebalance()
{
	local rc=0
	local op=32
	[ "$1" == "repair" ] && op=$CM_OP_REPAIR_STATUS
	[ "$1" == "rebalance" ] && op=$CM_OP_REBALANCE_STATUS
	while true ; do
		sleep 5
		repair_status="$M0_SRC_DIR/sns/cm/st/m0repair -O $op -t 0 -C ${lnet_nid}:${SNS_QUIESCE_CLI_EP} $ios_eps"
		echo $repair_status
		status=`eval $repair_status`
		rc=$?
		if [ $rc != 0 ]; then
			echo "SNS Repair status query failed"
			return $rc
		fi

		echo $status | grep status=2 && continue #sns repair is active, continue waiting
		break;
	done

	op=`echo $status | grep status=3`
	[[ !  -z  "$op"  ]] && return 1

	return 0
}

_dd()
{
	local FILE=$1
	local BS=$2
	local COUNT=$3

	dd if=$MOTR_M0T1FS_TEST_DIR/srcfile bs=$BS count=$COUNT \
	   of=$MOTR_M0T1FS_MOUNT_DIR/$FILE &>> $MOTR_TEST_LOGFILE || {
		echo "Failed: dd failed.."
		unmount_and_clean &>> $MOTR_TEST_LOGFILE
		return 1
	}
}

_rm()
{
	local FILE=$1

	echo "rm -f $MOTR_M0T1FS_MOUNT_DIR/$FILE"
	rm -f $MOTR_M0T1FS_MOUNT_DIR/$FILE &>> $MOTR_TEST_LOGFILE || {
		echo "$FILE delete failed"
		unmount_and_clean &>> $MOTR_TEST_LOGFILE
		return 1
	}
}

local_write()
{
	local BS=$1
	local COUNT=$2

	dd if=/dev/urandom bs=$BS count=$COUNT \
		of=$MOTR_M0T1FS_TEST_DIR/srcfile &>> $MOTR_TEST_LOGFILE || {
			echo "local write failed"
			unmount_and_clean &>> $MOTR_TEST_LOGFILE
			return 1
	}
}

local_read()
{
	local BS=$1
	local COUNT=$2

	dd if=$MOTR_M0T1FS_TEST_DIR/srcfile of=$MOTR_M0T1FS_TEST_DIR/file-$BS-$COUNT \
		bs=$BS count=$COUNT &>> $MOTR_TEST_LOGFILE || {
                        echo "local read failed"
                        unmount_and_clean &>> $MOTR_TEST_LOGFILE
                        return 1
        }
}

read_and_verify()
{
	local FILE=$1
	local BS=$2
	local COUNT=$3

	dd if=$MOTR_M0T1FS_MOUNT_DIR/$FILE of=$MOTR_M0T1FS_TEST_DIR/$FILE \
		bs=$BS count=$COUNT &>> $MOTR_TEST_LOGFILE || {
                        echo "m0t1fs read failed"
                        unmount_and_clean &>> $MOTR_TEST_LOGFILE
                        return 1
        }

	diff $MOTR_M0T1FS_TEST_DIR/file-$BS-$COUNT $MOTR_M0T1FS_TEST_DIR/$FILE &>> $MOTR_TEST_LOGFILE || {
		echo "files differ"
		unmount_and_clean &>>$MOTR_TEST_LOGFILE
		return 1
	}
	rm -f $FILE
}

_md5sum()
{
	local FILE=$1

	md5sum_generate "$MOTR_M0T1FS_MOUNT_DIR/$FILE"
}

md5sum_check()
{
	local rc

	md5sum -c < "$MOTR_M0T1FS_TEST_DIR/md5"
	rc=$?
	if [ $rc != 0 ] ; then
		echo "md5 sum does not match: $rc"
	fi
	return $rc
}

md5sum_generate()
{
	local FILE=$1
	md5sum "$FILE" | tee -a "$MOTR_M0T1FS_TEST_DIR/md5"
}

ha_notify_ios4_failure_or_online()
{
	# m0t1fs rpc end point, multiple mount on same
	# node have different endpoint
	local ctrl_fid="^c|1:8"
	local process_fid="^r|1:3"
	local ios4_fid="^s|1:3"
	local lnet_nid=$(m0_local_nid_get)
	local console_ep="$lnet_nid:$POOL_MACHINE_CLI_EP"
	local client_endpoint="$lnet_nid:12345:33:1"
	local eplist=()
	local failure_or_online=$1

	[ $MOTR_CLIENT_ONLY -eq 1 ] && client_endpoint=""

	for (( i=0; i < ${#IOSEP[*]}; i++)) ; do
		# Don't send message to IOS4 after the process is killed.
		if [ $failure_or_online != "failed" -o $i -ne 3 ]
		then
			eplist[$i]="$lnet_nid:${IOSEP[$i]}"
		fi
	done

	eplist=("${eplist[@]}" "$lnet_nid:${HA_EP}" "$client_endpoint")

	### It is expected that on failure/online HA dispatch events of some components,
	### HA dispatch events to it's descendants.
	echo "*** Mark process(m0d) on ios4 as: $failure_or_online"
	send_ha_events "$process_fid" "$failure_or_online" "${eplist[*]}" "$console_ep"
	echo "*** Mark ios4 as: $failure_or_online"
	send_ha_events "$ios4_fid" "$failure_or_online" "${eplist[*]}" "$console_ep"
}

kill_ios4_ioservice()
{
	echo "finding ios4 ..."
	echo pgrep -fn ${prog_exec}.+${IOSEP[3]}
	ios4_pid=`pgrep -fn ${prog_exec}.+${IOSEP[3]}`
	echo === pid of ios4: $ios4_pid ===
	kill -KILL $ios4_pid >/dev/null 2>&1
	echo "finding ios4 again..."
	ps ax | grep ${prog_exec} | grep ${IOSEP[3]}
}

start_ios4_ioservice()
{
	echo === cmd of starting ios4: $IOS4_CMD ===
	(eval $IOS4_CMD) &
}
