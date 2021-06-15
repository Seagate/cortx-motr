#!/bin/bash
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


SANDBOX_DIR=/var/motr/sns_sandbox/

IOS_EP=""
LOCAL_NID=""
state_recv=""

SRC_FILE=/tmp/src_file

M0_REPAIR_UTILS=/usr/sbin/m0repair
M0_HAM_UTILS=/usr/sbin/m0ham
M0_HAGEN_UTILS=/usr/sbin/m0hagen
MOTR_M0CP=/usr/bin/m0cp
MOTR_COPY=/usr/bin/m0cp
MOTR_CAT=/usr/bin/m0cat

SERVICE_TYPE=0   # SNS = 0, DIX = 1

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

SNS_CLI_EP="12345:33:1000"
M0HAM_CLI_EP="12345:33:1001"
SNS_QUIESCE_CLI_EP="12345:33:1002"
stride=32
src_bs=10M
src_count=2

declare -A ha_states=(
        [unknown]=$M0_NC_UNKNOWN
        [online]=$M0_NC_ONLINE
        [failed]=$M0_NC_FAILED
        [transient]=$M0_NC_TRANSIENT
        [repair]=$M0_NC_REPAIR
        [repaired]=$M0_NC_REPAIRED
        [rebalance]=$M0_NC_REBALANCE
)

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

# Get luster n/w lid of current host.
get_lnet_nid()
{
        LOCAL_NID=$(sudo lctl list_nids | head -1)
}

sandbox_init()
{
        # Remove exiting sandbox directory, if present
        [[ -d $SANDBOX_DIR ]] && rm -rf $SANDBOX_DIR

        # Create new sandbox directory,
        [[ -d $SANDBOX_DIR ]] || mkdir -p $SANDBOX_DIR

        # Change CWD to SANDBOX_DIR
        cd $SANDBOX_DIR
}

sandbox_fini()
{
        # Remove sandbox directory
        rm -rf $SANDBOX_DIR/
}

report_and_exit() {
        [ $# -eq 2 ] || die "${FUNCNAME[0]}: Invalid usage"
        local name=$1
        local rc=$2

        if [ $rc -eq 0 ]; then
            echo "$name: test status: SUCCESS"
        else
            echo "$name: FAILURE $rc" >&2
        fi
        exit $rc
}

 md5sum_generate()
 {
        local FILE=$1
        md5sum $FILE | tee -a $SANDBOX_DIR/md5
 }

_md5sum()
{
        local FILE=$1
        md5sum_generate $SANDBOX_DIR/$FILE
}

md5sum_check()
{
        local rc

        md5sum -c < $SANDBOX_DIR/md5
        rc=$?
        if [ $rc != 0 ]; then
                echo "md5 sum does not match: $rc"
        fi
        return $rc
}

prepare_datafiles_and_objects()
{
        local rc=0
        HA_EP=$(hctl status | grep hax | grep $LOCAL_NID | awk '{print $4}' | head -1)
        LOCAL_EP=$( hctl status | grep m0_client | grep $LOCAL_NID |  grep unknown | awk '{print $4}' | head -1)
        PROF_ID=$(hctl status | grep Profile | awk '{print $2}')
        PROC_ID=$(hctl status | grep $LOCAL_EP | awk '{print $3}')

        echo "HA_EP : $HA_EP"
        echo "LOCAL_EP : $LOCAL_EP"
        echo "PROF_ID : $PROF_ID"
        echo "PROC_ID : $PROC_ID"

        dd if=/dev/urandom bs=$src_bs count=$src_count \
           of="$SANDBOX_DIR/srcfile" || return $?

        for ((i=0; i < ${#file[*]}; i++)); do
                local lid=9
                local us=$((${unit_size[$i]} * 1024))

                MOTR_PARAM=" -l $LOCAL_EP -H $HA_EP -p $PROF_ID \
                               -P $PROC_ID  -L ${lid} -s ${us} "
                echo "MOTR_PARAMS : $MOTR_PARAM"

                echo "creating object ${file[$i]} bs=${us} * c=${file_size[$i]}"
                dd bs=${us} count=${file_size[$i]}            \
                   if="$SANDBOX_DIR/srcfile"         \
                   of="$SANDBOX_DIR/src${file[$i]}"

                $MOTR_COPY ${MOTR_PARAM} -c ${file_size[$i]} \
                                             -o ${file[$i]}  \
                                "$SANDBOX_DIR/srcfile" ||  {
                                rc=$?
                                echo "writing object ${file[$i]} failed"
                        }
        done
        return $rc
}

motr_read_verify()
{
        local start_file=$1
        local rc=0
        HA_EP=$(hctl status | grep hax | grep $LOCAL_NID | awk '{print $4}' | head -1)
        LOCAL_EP=$( hctl status | grep m0_client | grep $LOCAL_NID | grep unknown | awk '{print $4}' | head -1)
        PROF_ID=$(hctl status | grep Profile | awk '{print $2}')
        PROC_ID=$(hctl status | grep $LOCAL_EP | awk '{print $3}')

        echo " ======check motr ======"
        echo "HA_EP : $HA_EP"
        echo "LOCAL_EP : $LOCAL_EP"
        echo "PROF_ID : $PROF_ID"
        echo "PROC_ID : $PROC_ID"

        for ((i=$start_file; i < ${#file[*]}; i++)); do
                local lid=9
                local us=$((${unit_size[$i]} * 1024))

                MOTR_PARAM=" -l $LOCAL_EP -H $HA_EP -p $PROF_ID -P \
                                $PROC_ID  -L 9 -s ${us} "
                echo "MOTR_PARAM : $MOTR_PARAM"

                echo "Reading object ${file[$i]} ... and diff ..."
                rm -f "$SANDBOX_DIR/${file[$i]}"
                $MOTR_CAT ${MOTR_PARAM} -c ${file_size[$i]} \
                            -o ${file[$i]} $SANDBOX_DIR/${file[$i]} || {
                        rc=$?
                        echo "reading ${file[$i]} failed"
                }

                diff "$SANDBOX_DIR/${file[$i]}" \
                     "$SANDBOX_DIR/src${file[$i]}" || {
                        rc=$?
                        echo "comparing ${file[$i]} with src${file[$i]} failed"
                }
        done

        if [[ $rc -eq 0 ]]; then
                echo "file verification success"
        else
                echo "file verification failed"
        fi

        return $rc
}

# input parameters:
# (i) ha_msg_nvec in yaml format (see utils/m0hagen)
# (ii) list of remote endpoints
# (iii) local endpoint
send_ha_msg_nvec()
{
        local ha_msg_nvec=$1
        local remote_eps=($2)
        local local_ep=$3
        state_recv=""

        # Complete the message for m0hagen
        local ha_msg_yaml='!ha_msg
fid: !fid [0,0]
process: ^r|0.0
service: ^s|0.0
data:'
        ha_msg_nvec=`echo "$ha_msg_nvec" | sed 's/^/  /g'`
        ha_msg_yaml+="$ha_msg_nvec"

        # Convert a yaml message to its xcode representation
        cmd_xcode="echo \"$ha_msg_yaml\" | $M0_HAGEN_UTILS"
        # Check for errors in the message format
        (eval "$cmd_xcode > /dev/null")
        if [ $? -ne 0 ]; then
                echo "m0hagen can not convert message:"
                echo "$ha_msg_yaml"
                return 1
        fi

        for ep in "${remote_eps[@]}"; do
                cmd="$cmd_xcode | $M0_HAM_UTILS -v -s $local_ep $ep"
                echo "$cmd"
                local xcode=`eval "$cmd"`
                if [ $? -ne 0 ]; then
                        echo "m0ham failed to send a message"
                        return 1
                fi
                if [ ! -z "$xcode" ]; then
                        echo "Got reply (xcode):"
                        echo "$xcode" | head -c 100
                        echo
                        echo "Decoded reply:"
                        echo "$xcode" | $M0_HAGEN_UTILS -d
                        if [ -z "$state_recv" ]; then
                                state_recv=$(echo $xcode | $M0_HAGEN_UTILS -d | grep ' state:' | awk '{print $2}')
                        fi
                fi
        done
}

# input parameters:
# (i) list of fids
# (ii) state in string representation
# (iii) list of remote endpoints
# (iv) local endpoint
send_ha_events()
{
        local fids=($1)
        local state=$2
        local remote_eps=($3)
        local local_ep=$4

        local yaml='!ha_nvec_set
ignore_same_state: false
notes:'
        for fid in "${fids[@]}"; do
                # Convert fid to m0hagen's format
                fid=`echo "$fid" | tr : .`
                yaml="$yaml
  - {fid: $fid, state: $state}"
        done

        send_ha_msg_nvec "$yaml" "${remote_eps[*]}" "$local_ep"
}

# input parameters:
# (i) list of fids
# (ii) list of remote endpoints
# (iii) local endpoint
request_ha_state()
{
        local fids=($1)
        local remote_eps=($2)
        local local_ep=$3

        local yaml='!ha_nvec_get
get_id: 100
ignore_same_state: false
fids:'
        for fid in "${fids[@]}"; do
                # Convert fid to m0hagen's format
                fid=`echo "$fid" | tr : .`
                yaml="$yaml
  - $fid"
        done

        send_ha_msg_nvec "$yaml" "${remote_eps[*]}" "$local_ep"
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
        local service_eps=$(service_eps_with_hctl_get)
        local local_ep="$LOCAL_NID:$M0HAM_CLI_EP"

        echo "service eps : $service_eps"
        echo "fids : $fids"
        echo "state : $state "
        echo "local ep : $local_ep"

        echo "setting devices { ${fids[*]} } to $state"
        send_ha_events "${fids[*]}" "$state" "$service_eps" "$local_ep"
        rc=$?
        if [ $rc -ne 0 ]; then
                echo "HA note set failed: $rc"
                return $rc
        fi
}

disk_state_get()
{
        local fids=()
        local nr=0

        for d in "$@"; do
                fids[$nr]="^k|1:$d"
                nr=$((nr + 1))
        done

        local service_eps=$(service_eps_with_hctl_get)
        local local_ep="$LOCAL_NID:$M0HAM_CLI_EP"
        echo "service eps : $service_eps"
        echo "fids : $fids"
        echo "local ep : $local_ep"

        echo "getting device { ${fids[*]} }'s HA state"
        request_ha_state "${fids[*]}" "$service_eps" "$local_ep"
        rc=$?
        if [ $rc != 0 ]; then
                echo "HA state get failed: $rc"
                return $rc
        fi
}

check_cluster_status()
{
        hctl status  >/dev/null
        if [ $? -ne 0 ]; then
                echo "Cluster is not running"
                return 1
        else
                echo "Cluster is running"
                return 0
        fi
}

# Get io-service end-points of all the nodes in cluster
get_ios_endpoints()
{
        for i in `hctl status | grep ioservice | awk '{print $4}'`; do
                IOS_EP="$IOS_EP -S $i"
        done
}

# Get io-service and hax endpoints from all the nodes in cluster
service_eps_with_hctl_get()
{
        local service_eps=""

        for i in `hctl status | grep ioservice | awk '{print $4}'`; do
                service_eps+=("$i")
        done
        for i in `hctl status | grep hax | awk '{print $4}'`; do
                service_eps+=("$i")
        done

        echo "${service_eps[*]}"
}

sns_repair_or_rebalance_status()
{
        local rc=0
        local op=32
 
        [ "$1" == "repair" ] && op=$CM_OP_REPAIR_STATUS
        [ "$1" == "rebalance" ] && op=$CM_OP_REBALANCE_STATUS

        repair_status="$M0_REPAIR_UTILS -O $op -t 0 -C ${LOCAL_NID}:${SNS_QUIESCE_CLI_EP} $IOS_EP"
        echo $repair_status
        eval $repair_status
        rc=$?
        if [ $rc != 0 ]; then
                echo "SNS Repair status query failed"
        fi

        return $rc
}

wait_for_sns_repair_or_rebalance()
{
        local rc=0
        local op=32

        [ "$1" == "repair" ] && op=$CM_OP_REPAIR_STATUS
        [ "$1" == "rebalance" ] && op=$CM_OP_REBALANCE_STATUS
        while true ; do
                sleep 5
                repair_status="$M0_REPAIR_UTILS -O $op -t 0 -C ${LOCAL_NID}:${SNS_QUIESCE_CLI_EP} $IOS_EP"
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

sns_repair()
{
        local rc=0
        local pid=0

        repair_trigger="$M0_REPAIR_UTILS -O $CM_OP_REPAIR -t 0 -C ${LOCAL_NID}:${SNS_CLI_EP} $IOS_EP "
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

        rebalance_trigger="$M0_REPAIR_UTILS -O $CM_OP_REBALANCE -t 0 -C ${LOCAL_NID}:${SNS_CLI_EP} $IOS_EP"
        echo $rebalance_trigger
        eval $rebalance_trigger
        rc=$?
        if [ $rc != 0 ]; then
                echo "SNS Re-balance failed"
        fi

        return $rc
}
