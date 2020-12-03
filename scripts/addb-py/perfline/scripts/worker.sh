#!/bin/bash

set -e
set -x

SCRIPT_NAME=`echo $0 | awk -F "/" '{print $NF}'`
SCRIPT_PATH="$(readlink -f $0)"
SCRIPT_DIR="${SCRIPT_PATH%/*}"

TOOLS_DIR="$SCRIPT_DIR/../../chronometry"
PERFLINE_DIR="$SCRIPT_DIR/../"
STAT_DIR="$SCRIPT_DIR/../stat"

# NODES="ssc-vm-c-1042.colo.seagate.com,ssc-vm-c-1043.colo.seagate.com"
# EX_SRV="pdsh -S -w $NODES"
# HA_TYPE="hare"
MKFS=

function validate() {
    local leave=
    if [[ -z "$RESULTS_DIR" ]]; then
	echo "Result dir parameter is not passed"
	leave="1"
    fi

    if [[ -z "$WORKLOADS" ]]; then
	echo "Application workload is not specified"
	leave="1"
    fi

    if [[ -z "$NODES" ]]; then
	echo "Nodes are not specified"
	leave="1"
    fi

    case $HA_TYPE in
	"hare") echo "HA type: HARE" ;;
	"pcs") echo "HA type: Pacemaker" ;;
	*) 
	    echo "Unknown HA type: $HA_TYPE"
	    leave="1"
	    ;;
    esac

    if [[ -n $leave ]]; then
	exit 1
    fi
}

function stop_hare() {
    hctl shutdown || true    
}

function stop_pcs() {
    pcs resource disable motr-ios-c{1,2}
    pcs resource disable s3server-c{1,2}-{1,2,3,4,5,6,7,8,9,10,11}
    sleep 30
}

function stop_cluster() {
    echo "Stop cluster"

    case $HA_TYPE in
	"hare") stop_hare ;;
	"pcs") stop_pcs ;;
    esac
}

function cleanup_hare() {
    echo "Remove /var/motr/*"
    pdsh -S -w $NODES 'rm -rf /var/motr/*' || true
}

function cleanup_pcs() {
    echo "Remove /var/motr/s3server*"
    pdsh -S -w $NODES 'rm -rf /var/motr/s3server*' || true

    local ioservice_list=$(hctl status \
        | grep ioservice | sed 's/\[.*\]//' | awk '{print $2}')

    echo "Remove ioservice(s)"
    for ios_fid in $ioservice_list; do
        local ios_dir="/var/motr/m0d-$ios_fid"
        local srv_node_cmd="if [ -e $ios_dir ]; then rm -rf $ios_dir; fi"
        $EX_SRV $srv_node_cmd
    done
}

function cleanup_cluster() {
    case $HA_TYPE in
	"hare") cleanup_hare ;;
	"pcs") cleanup_pcs ;;
    esac
}

function restart_hare() {
    hctl bootstrap --mkfs /var/lib/hare/cluster.yaml
}

function restart_pcs() {
    ssh srvnode-1 'systemctl start motr-mkfs@0x7200000000000001:0xc'
    ssh srvnode-2 'systemctl start motr-mkfs@0x7200000000000001:0x55'
    pcs resource enable motr-ios-c{1,2}
    sleep 10
    pcs resource enable s3server-c{1,2}-{1,2,3,4,5,6,7,8,9,10,11}
    sleep 30
}

function restart_cluster() {
    echo "Restart cluster"
    
    case $HA_TYPE in
	"hare") restart_hare ;;
	"pcs") restart_pcs ;;
    esac
}

function run_workloads()
{
    local client="client"
    mkdir -p $client
    pushd $client

    START_TIME=`date +%s000000000`
    for ((i = 0; i < $((${#WORKLOADS[*]})); i++)); do
        echo "workload $i"
        local cmd=${WORKLOADS[((i))]}
        eval $cmd | tee workload-$i.log
	STATUS=${PIPESTATUS[0]}
    done

    STOP_TIME=`date +%s000000000`
    popd			# $client
}

function create_results_dir() {
    echo "Create results folder"
    mkdir -p $RESULTS_DIR
    pushd $RESULTS_DIR
}

function save_motr_artifacts() {
    local ios_m0trace_dir="m0trace_ios"
    local configs_dir="configs"
    local dumps_dir="dumps"

    # local variables for Hare cluster
    local ioservice_list=$(cat $RESULTS_DIR/hctl-status.stop \
        | grep ioservice | sed 's/\[.*\]//' | awk '{print $2}')

    local ios_l=""
    for zzz in $ioservice_list; do
        ios_l="$ios_l $zzz"
    done

    mkdir -p $configs_dir
    pushd $configs_dir
    cp /etc/sysconfig/motr ./
    cp /var/lib/hare/cluster.yaml ./
    popd

    mkdir -p $ios_m0trace_dir
    pushd $ios_m0trace_dir
    if [[ -z $NO_M0TRACE_FILES ]]; then
        $EX_SRV $SCRIPT_DIR/save_m0traces $(hostname) $(pwd) "motr" "\"$ios_l\""
    fi
    popd # $ios_motrace_dir

    mkdir -p $dumps_dir
    pushd $dumps_dir
    if [[ -z $NO_ADDB_STOBS ]] && [[ -z $NO_ADDB_DUMPS ]]; then
        if [[ -n "$NO_M0PLAY_DB" ]]; then
            local no_m0play_option="--no-m0play-db"
        fi
        $EX_SRV $SCRIPT_DIR/process_addb $no_m0play_option --host $(hostname) --dir $(pwd) --app "motr" --io-services "\"$ios_l\"" --start $START_TIME --stop $STOP_TIME
    fi
    popd # $dumps_dir
}


function save_s3srv_artifacts() {
    local auth_dir="auth"
    local haproxy_dir="haproxy"
    local log_dir="log"
    local cfg_dir="cfg"
    local crash_dir="crash"

    for srv in $(echo $NODES | tr ',' ' '); do
        mkdir -p $srv
        pushd $srv

        # Fetch list of folders per server
        dirs=`ssh $srv -T "ls /var/motr/ | grep s3server | xargs -n1 basename"`
        echo $dirs
        mkdir $dirs

        for s3d in s3server*; do
            # Copy logs
            scp -r $srv:/var/log/seagate/s3/$s3d ./$s3d/$log_dir
        done

        mkdir -p $auth_dir
        scp -r $srv:/var/log/seagate/auth/* $auth_dir || true
        mv $auth_dir/server/app.log $auth_dir/server/app.$srv.log || true

        mkdir -p $haproxy_dir
        mkdir -p $haproxy_dir/$log_dir
        scp -r $srv:/var/log/haproxy* $haproxy_dir/$log_dir || true

        mkdir -p $haproxy_dir/$cfg_dir
        scp -r $srv:/etc/haproxy/* $haproxy_dir/$cfg_dir

        scp -r $srv:/opt/seagate/cortx/s3/conf/s3config.yaml ./
        scp -r $srv:/opt/seagate/cortx/s3/s3startsystem.sh ./
        scp -r $srv:/etc/hosts ./

        popd
    done

    if [[ -z $NO_M0TRACE_FILES ]]; then
        $EX_SRV $SCRIPT_DIR/save_m0traces $(hostname) $(pwd) "s3server"
    fi

    if [[ -z $NO_ADDB_STOBS ]]; then
        if [[ -n "$NO_M0PLAY_DB" ]]; then
            local no_m0play_option="--no-m0play-db"
        fi
        $EX_SRV $SCRIPT_DIR/process_addb $no_m0play_option --host $(hostname) --dir $(pwd) --app "s3server" --start $START_TIME --stop $STOP_TIME
    fi
}

function save_stats() {
    for srv in $(echo $NODES | tr ',' ' '); do
        mkdir -p $srv
        pushd $srv
	scp -r $srv:/var/perfline/iostat* iostat || true
	scp -r $srv:/var/perfline/blktrace* blktrace || true
	scp -r $srv:/var/perfline/dstat* dstat || true
	scp -r $srv:/var/perfline/hw* hw || true
	scp -r $srv:/var/perfline/network* network || true
	popd
    done
}

function collect_artifacts() {
    local m0d="m0d"
    local s3srv="s3server"
    local stats="stats"

    echo "Collect artifacts"

    mkdir -p $stats
    pushd $stats
    save_stats
    popd

    mkdir -p $m0d
    pushd $m0d
    save_motr_artifacts
    popd			# $m0d

    mkdir -p $s3srv
    pushd $s3srv
    save_s3srv_artifacts
    popd			# $s3server
    
    if [[ -z $NO_ADDB_STOBS ]] && [[ -z $NO_ADDB_DUMPS ]] && [[ -z $NO_M0PLAY_DB ]]; then
        $SCRIPT_DIR/merge_m0playdb $m0d/dumps/m0play* $s3srv/*/m0play*
    fi
}

function close_results_dir() {
    echo "Close results dir"
    popd
}

function start_stat_utils()
{
    echo "Start stat utils"

    echo "Start iostat"
    $EX_SRV "$STAT_DIR/iostat_start.sh" &

    # echo "Start blktrace"
    # $EX_SRV "$STAT_DIR/blktrace_start.sh" &

    echo "Start dstat"
    $EX_SRV "$STAT_DIR/dstat_start.sh" &
}

function stop_stat_utils()
{
    echo "Stop stat utils"

    echo "Stop iostat"
    $EX_SRV "$STAT_DIR/iostat_stop.sh"

    # echo "Stop blktrace"
    # $EX_SRV "$STAT_DIR/blktrace_stop.sh"
    
    echo "Stop dstat"
    $EX_SRV "$STAT_DIR/dstat_stop.sh"

    echo "Gather static info"
    $EX_SRV "$STAT_DIR/collect_static_info.sh"

    # wait
}

function start_measuring_workload_time()
{
    WORKLOAD_TIME_START_SEC=$(date +'%s')
}

function stop_measuring_workload_time()
{
    WORKLOAD_TIME_STOP_SEC=$(date +'%s')

    local stats="stats"
    mkdir -p $stats
    pushd $stats
    # Save workload time
    echo "workload_start_time: $WORKLOAD_TIME_START_SEC" > common_stats.yaml
    echo "workload_stop_time: $WORKLOAD_TIME_STOP_SEC" >> common_stats.yaml
    popd
}

function start_measuring_test_time()
{
    TEST_TIME_START_SEC=$(date +'%s')
}

function stop_measuring_test_time()
{
    TEST_TIME_STOP_SEC=$(date +'%s')
    
    local stats="stats"
    mkdir -p $stats
    pushd $stats
    # Save test time
	echo "test_start_time: $TEST_TIME_START_SEC" >> common_stats.yaml
    echo "test_stop_time: $TEST_TIME_STOP_SEC" >> common_stats.yaml
    popd
}

function generate_report()
{
    python3 $STAT_DIR/report_generator/gen_report.py . $STAT_DIR/report_generator &
}

function main() {

    start_measuring_test_time

    if [[ -n $MKFS ]]; then
	# Stop cluster 
	stop_cluster
	cleanup_cluster
	
	# Restart cluster -- do mkfs, whatever...
	restart_cluster
    fi

    # Create artifacts folder
    create_results_dir

    # @artem -- place code below
    # Start stat utilities
    echo "Start stat utilities"
    start_stat_utils

    # Start workload time execution measuring
    start_measuring_workload_time

    # Start workload
    run_workloads

    # Stop workload time execution measuring
    stop_measuring_workload_time

    # @artem -- place code below
    # Stop stat utilities
    echo "Stop stat utilities"
    stop_stat_utils

    # Collect stat artifacts
    echo "Collect stat artifacts"

    hctl status > hctl-status.stop
    # if [[ -n $MKFS ]]; then
    # 	# Stop cluster
    # 	stop_cluster
    # fi

    # Collect ADDBs/m0traces/m0play.db
    collect_artifacts

    # Generate plots, hists, etc
    echo "Generate plots, hists, etc"

    # if [[ -n $MKFS ]]; then
    # 	cleanup_cluster
    # 	restart_cluster
    # fi

    stop_measuring_test_time
    
    generate_report

    # Close results dir
    close_results_dir

    exit $STATUS
}

echo "parameters: $@"

while [[ $# -gt 0 ]]; do
    case $1 in
        -w|--workload_config)
            WORKLOADS+=("$2")
            shift
            ;;
        -p|--result-path)
            RESULTS_DIR=$2
            shift
            ;;
        --nodes)
            NODES=$2
            EX_SRV="pdsh -S -w $NODES"
            shift
            ;;
        --ha_type)
            HA_TYPE=$2
            shift
            ;;
        --no-m0trace-files)
            NO_M0TRACE_FILES="1"
            ;;
        --no-m0trace-dumps)
            NO_M0TRACE_DUMPS="1"
            ;;
        --no-addb-stobs)
            NO_ADDB_STOBS="1"
            ;;
        -d|--no-addb-dumps)
            NO_ADDB_DUMPS="1"
            ;;
        --no-m0play-db)
            NO_M0PLAY_DB="1"
            ;;
        --no-motr-trace)
            NO_MOTR_TRACE="1"
            ;;
	--mkfs)
	    MKFS="1"
	    ;;
        *)
            echo -e "Invalid option: $1\nUse --help option"
            exit 1
            ;;
    esac
    shift
done

validate

# int main()
main
