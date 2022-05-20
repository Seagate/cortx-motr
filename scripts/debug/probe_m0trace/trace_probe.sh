#!/bin/bash

export PS4='+ ${FUNCNAME[0]:+${FUNCNAME[0]}():}line ${LINENO}: '

SCRIPT_NAME=`echo $0 | awk -F "/" '{print $NF}'`

function _log()
{
	echo "$@"
}

function exit_with_error()
{
	msg=$1
	exit_code=$2
	>&2 echo -e $msg
	[[ -e $STATUS_FILE ]] && rm $STATUS_FILE
	exit $exit_code
}

function usage() {
    cat << EOF

Usage: $SCRIPT_NAME [options]
    -t, --trace            client endpoint address
    -d, --db               server endpoint address
    -p  --probes           probes ex: ha, rpc
    -m  --m0trace_path     m0trace binary path 
    -h, --help             this help

Example:
    $SCRIPT_NAME  
    -t /var/motr/m0d-0x7200000000000001:0x3/m0trace.1703586
    -t /var/motr/m0d-0x7200000000000001:0x3/m0trace.1703586
    -d test.db
    -p ha
    -p rpc

EOF
}

m0d_pid=""

function get_pid()
{
	local t_file=$1
	local trace_out_file=$2
	local trace_format='--format="custom:%T%t%p%t%m"'
	$M0_TRACE_CMD --format="custom:%T%t%p%t%f%t%m" -i $t_file  > $trace_out_file
	#echo "pid = $(cat $pid_file)"
}

function main()
{
	if [[ -z "$trace_files" || -z "$db_file" || -z "$probes" ]]; then
		exit_with_error "Invalid Arguments" 1
	fi
	echo "traces = $trace_files db = $db_file probe = $probes"
	rm -f $db_file
	for i in $trace_files
	do
		m0d_pid="/tmp/m0d.pid"
		rm -f $m0d_pid
		get_pid $i $m0d_pid
		pid=$(cat /tmp/m0d.pid | head -1)
		echo "Reading trace file $i pid = $pid"
		cat $m0d_pid | grep m0_ha_msg_accept | grep nv_note > /tmp/ha_trace.txt
		while read -r line; 
		do 
			fid=$(echo $line | grep -o no_id=.* | awk -F"[<>]" '{print $2}'); 
			status=$(echo $line | grep -o no_state=.* | awk -F'=' '{print $2}' | sed 's/[^0-9]//g'); 
			ts=$(echo $line | awk '{print $1}'); pid=$(echo $line | awk '{print $2}');  
			echo "ts=$ts pid=$pid fid=$fid status=$status" >> /tmp/trace_filter.txt;  
		done < /tmp/ha_trace.txt 
	done
	python3 create_db.py --db $db_file --trace_txt /tmp/trace_filter.txt
}

function check_arg_value() {
    [[ $# -gt 1 ]] || {
        exit_with_error "Incorrect use of the option $1\nUse --help option" 1
    }
}

trace_files=""
db_file="test_analyse.db"
probes=""

while [[ $# -gt 0 ]]; do
    case $1 in
        -t| --trace)
            check_arg_value $1 $2
	    trace_files="$trace_files $2"
            shift
            ;;
        -d| --db)
            check_arg_value $1 $2
            db_file="$2"
            shift
            ;;
        -p| --probes)
            check_arg_value $1 $2
            probes="$probes $2"
            shift
            ;;
        -m| --m0trace_path)
            check_arg_value $1 $2
            M0_TRACE_CMD="$2"
            shift
            ;;
        -h| --help)
            usage
            exit 1
            ;;
    esac
    shift
done

main
exit 0
