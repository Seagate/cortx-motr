#!/bin/bash

# set -x
motr_st_util_dir=$(dirname $(readlink -f $0))
m0t1fs_dir="$motr_st_util_dir/../../../m0t1fs/linux_kernel/st"

. $m0t1fs_dir/common.sh
. $m0t1fs_dir/m0t1fs_common_inc.sh
. $m0t1fs_dir/m0t1fs_client_inc.sh
. $m0t1fs_dir/m0t1fs_server_inc.sh
. $motr_st_util_dir/motr_local_conf.sh
. $motr_st_util_dir/motr_st_inc.sh

proc_state_change()
{
    local lnet_nid=`sudo lctl list_nids | head -1`
    local c_endpoint="$lnet_nid:$M0HAM_CLI_EP"
    local s_endpoint="$lnet_nid:$1"
    local fid=$2
    local state=$3

    # Generate HA event
    echo "Send HA event for motr"
    echo "c_endpoint is : $c_endpoint"
    echo "s_endpoint is : $s_endpoint"

    send_ha_events "$fid" "$state" "$s_endpoint" "$c_endpoint"
}

# {0x72| ((^r|1:14), ..., "192.168.122.122@tcp:12345:2:2", ...
# {0x72| ((^r|1:10), ..., "192.168.122.122@tcp:12345:2:1", ...

# EXAMPLE: ha_msg_send.sh "12345:2:2" "^r|1:26" "transient"
# EXAMPLE: ha_msg_send.sh "12345:2:3" "^r|1:12" "online"
proc_state_change $1 $2 $3 > /dev/null
