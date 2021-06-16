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


#set -x
motr_st_util_dir=$( cd "$(dirname "$0")" ; pwd -P )
motr_dir="$motr_st_util_dir/../../.."
m0t1fs_dir="$motr_dir/m0t1fs/linux_kernel/st"

# Re-use as many m0t1fs system scripts as possible
. $m0t1fs_dir/common.sh
. $m0t1fs_dir/m0t1fs_common_inc.sh
. $m0t1fs_dir/m0t1fs_client_inc.sh
. $m0t1fs_dir/m0t1fs_server_inc.sh
. $m0t1fs_dir/m0t1fs_sns_common_inc.sh

. $motr_st_util_dir/motr_local_conf.sh
. $motr_st_util_dir/motr_st_inc.sh

motr_sandbox=$SANDBOX_DIR
m0crate_trace_dir=$motr_sandbox/motr
m0crate_logfile=$m0crate_trace_dir/m0crate_`date +"%Y-%m-%d_%T"`.log
m0crate_workload_yaml=$motr_st_util_dir/m0crate_iem_freespace_st_workloads.yaml

m0crate_src_size=16 # in MB
m0crate_src_file=$motr_sandbox/m0crate_"$m0crate_src_size"MB

customise_motr_configs()
{
	local yaml=$1

	echo $yaml
	sed -i "s/^\([[:space:]]*MOTR_LOCAL_ADDR: *\).*/\1$MOTR_LOCAL_EP/" $yaml
	sed -i "s/^\([[:space:]]*MOTR_HA_ADDR: *\).*/\1$MOTR_HA_EP/" $yaml
	sed -i "s/^\([[:space:]]*MOTR_PROF: *\).*/\1$MOTR_PROF_OPT/" $yaml
	sed -i "s/^\([[:space:]]*MOTR_PROCESS_FID: *\).*/\1$MOTR_PROC_FID/" $yaml
	sed -i "s#^\([[:space:]]*SOURCE_FILE: *\).*#\1$m0crate_src_file#" $yaml
}

run_m0crate()
{
	local cmd=$motr_dir/motr/m0crate/m0crate
	local cmd_arg="-S $m0crate_workload_yaml"

	if [ ! -f $cmd ] ; then
		echo "Can't find m0crate at $cmd"
		return 1
	fi

	local cwd=`pwd`
	cd $m0crate_trace_dir
	eval $cmd $cmd_arg &
	wait $!
	if [ $? -ne 0 ]
	then
		echo "  Failed to run command $cmd $cmd_arg"
		cd $cwd
		return 1
	fi
	cd $cwd
	return 0
}

calc()
{
    awk "BEGIN{print $*}";
}

main()
{

	sandbox_init
	$motr_dir/scripts/install-motr-service -u
	$motr_dir/scripts/install-motr-service -l

	NODE_UUID=`uuidgen`
	mkdir $m0crate_trace_dir || {
		echo "Failed to create trace directory"
		return 1
	}
	customise_motr_configs $m0crate_workload_yaml
	rc=0

	# Start motr services.
	motr_service_start 1 1 1 4 4
	dix_init

	# Prepare and and run.
	block_size=4096
	block_count=`expr $m0crate_src_size \* 1024 \* 1024 / $block_size`
	echo "dd if=/dev/urandom bs=$block_size count=$block_count of=$m0crate_src_file"
	dd if=/dev/urandom bs=$block_size count=$block_count of=$m0crate_src_file 2> $m0crate_logfile || {
		echo "Failed to create a source file"
		motr_service_stop
		return 1
	}
    	profile=$(echo $MOTR_PROF_OPT | sed 's\:\,\')
	libmotr_path=${motr_dir}/motr/.libs/libmotr.so
	systemd_file='/usr/lib/systemd/system/motr-free-space-monitor.service'
	bin_args="-s $MOTR_HA_EP -c ${MOTR_HA_EP}101 -p $profile"
	systemd_args="-S \"$bin_args\" -l $libmotr_path"
	alert_freq=5
	thr_list="1 2 3 4"
	motr_conf_file='/etc/sysconfig/motr-free-space-monitor'

	sed -i "s#^\([[:space:]]*MOTR_FREESPACE_ARGS= *\).*#\1$systemd_args#" $motr_conf_file
	sed -i "s#^\([[:space:]]*MOTR_FREESPACE_TRIGGER_IEM_FREQ= *\).*#\1$alert_freq#" $motr_conf_file
	sed -i "s#^\([[:space:]]*MOTR_FREESPACE_TRIGGER_IEM_THRESHOLD= *\).*#\1$thr_list#" $motr_conf_file
	cat $motr_conf_file
    	systemctl daemon-reload
    	systemctl start motr-free-space-monitor.service
    	systemctl status motr-free-space-monitor.service
	journalctl -u motr-free-space-monitor | tail -5
    	for ((i=0;i<2;i++))
    	do
    		echo "Check system size and run test"
		run_m0crate || {
			rc=$?
			echo "m0crate IO workload failed."
			error_handling $rc
		}
    	done
	# Stop services and clean up.
    	systemctl status motr-free-space-monitor.service
 	systemctl stop motr-free-space-monitor.service
	journalctl -u motr-free-space-monitor | tail -10
	$motr_dir/scripts/install-motr-service -u
	motr_service_stop || rc=1
	if [ $rc -eq 0 ]; then
		sandbox_fini
	else
		error_handling $rc
	fi
	return $rc
}

echo "Free space monitor service TEST ... "
trap unprepare EXIT
main
report_and_exit m0crate $?
