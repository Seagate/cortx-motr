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

SANDBOX_DIR=${SANDBOX_DIR:-/var/motr/sandbox.dix-repair-st}
M0_SRC_DIR=`readlink -f $0`
M0_SRC_DIR=${M0_SRC_DIR%/*/*/*/*}
[ `id -u` -eq 0 ] || die 'Must be run by superuser'

. $M0_SRC_DIR/spiel/st/m0t1fs_spiel_dix_common_inc.sh #dix spiel
. $M0_SRC_DIR/motr/st/utils/motr_local_conf.sh

do_motr_start=1
do_dixinit=1
verbose=0
fids_file="${SANDBOX_DIR}/fids.txt"
keys_file="${SANDBOX_DIR}/keys.txt"
vals_file="${SANDBOX_DIR}/vals.txt"
out_file="${SANDBOX_DIR}/m0kv_st.log"
rc_file="${SANDBOX_DIR}/m0kv_st.cod.log"
erc_file="${SANDBOX_DIR}/m0kv_st.ecod.log"
res_out_file="${SANDBOX_DIR}/m0kv_st.results.log"
num=10
fail_device=2

interrupt() { echo "Interrupted by user" >&2; stop 2; }
error() { echo "$@" >&2; stop 1; }

function usage()
{
	echo "dix_repair_st.sh [-n -i -v -h] all|test1 test2 ..."
	echo "Options:"
	echo "    '-n|--no-setup'        don't start motr service"
	echo "    '-i|--no-dixinit'      don't create dix meta-indices"
	echo "                           note: ignored without --no-setup"
	echo "    '-v|--verbose'         output additional info into console"
	echo "    '-h|--help'            show this help"
	echo "Available tests:"
	echo "    'lookup'               Check indices after repair/rebalance"
	echo "    'insert'               Check indices and records after repair/rebalance"
	echo "    'get_during'           Check records after repair and before rebalance"
	echo "    'drop_during'          Drop indices after repair and before rebalance"
	echo "    'put_during'           Put records after repair and before rebalance"
	echo "    'del_repair'           Del records from index in case repair is in active state"
	echo "    'del_rebalance'        Del records from index in case rebalance is in active state"
	echo "    empty list or 'all'    enable all tests"
	echo "Examples:"
	echo "./dix_repair_st.sh --no-setup all"
	echo "./dix_repair_st.sh lookup --verbose"
}

function load_item()
{
	local resultvar=$3
	local fil=$1
	local n=$2
	local t=${n}p

	local  myresult=$(sed -n "${t}" < ${fil})
	if [[ "$resultvar" ]]; then
		eval $resultvar="'$myresult'"
	else
		echo "$myresult"
	fi
}

device_fail() {
	local rc
	cas_disk_state_set "failed" $fail_device
	rc=$?
	return $rc
}

device_query() {
	local rc
	cas_disk_state_get $fail_device
	rc=$?
	return $rc
}


dix_repair() {
	local rc
	cas_disk_state_set "repair" $fail_device
	rc=$?
	[ $rc != 0 ] && return $rc
	spiel_dix_repair_start
	rc=$?
	return $rc
}

dix_rebalance() {
	local rc
	cas_disk_state_set "rebalance" $fail_device
	rc=$?
	[ $rc != 0 ] && return $rc
	spiel_dix_rebalance_start
	rc=$?
	return $rc
}

dix_rep_wait() {
	local rc
	spiel_wait_for_dix_repair
	rc=$?
	[ $rc != 0 ] && return $rc
	cas_disk_state_set "repaired" $fail_device
	rc=$?
	return $rc
}

dix_reb_wait() {
	local rc
	spiel_wait_for_dix_rebalance
	rc=$?
	[ $rc != 0 ] && return $rc
	cas_disk_state_set "online" $fail_device
	rc=$?
	return $rc
}
dix_repair_wait() {
	local rc
	dix_repair
	rc=$?
	[ $rc != 0 ] && return $rc
	dix_rep_wait
	rc=$?
	return $rc
}


dix_rebalance_wait() {
	local rc
	dix_rebalance
	rc=$?
	[ $rc != 0 ] && return $rc
	dix_reb_wait
	rc=$?
	return $rc
}

dix_repair_rebalance_wait() {
	local rc
	dix_repair_wait
	rc=$?
	[ $rc != 0 ] && return $rc
	dix_rebalance_wait
	rc=$?
	return $rc
}

lookup() {
	local rc
	local lookup="lookup @${fids_file}"
	local create="create @${fids_file}"
	echo "Check existing indices after repair and rebalance"
	${MOTRTOOL} ${create}
	rc=$?
	[ $rc != 0 ] && return $rc
	device_fail
	rc=$?
	[ $rc != 0 ] && return $rc
	dix_repair_rebalance_wait
	rc=$?
	[ $rc != 0 ] && return $rc
	${MOTRTOOL} ${lookup} >${out_file}
	rc=$?
	[ $rc != 0 ] && return $rc
	grep    "found rc"   ${out_file} | tee ${rc_file}
	grep -v "found rc 0" ${rc_file}  | tee ${erc_file}
	[ -s "${erc_file}" ] && return 1
	device_query
	echo "lookup test case done"
	return $rc
}

insert() {
	local rc
	local fid=$(load_item $fids_file 1)
	local lookup="lookup \"${fid}\""
	local create="create \"${fid}\""
	local put="put \"${fid}\" @${keys_file} @${vals_file}"
	local get="get \"${fid}\" @${keys_file}"
	echo "Check existing records after repair and rebalance"
	${MOTRTOOL} ${create} ${put}
	rc=$?
	[ $rc != 0 ] && return $rc
	device_fail
	rc=$?
	[ $rc != 0 ] && return $rc
	dix_repair_rebalance_wait
	rc=$?
	[ $rc != 0 ] && return $rc
	${MOTRTOOL} ${lookup} >${out_file}
	rc=$?
	[ $rc != 0 ] && return $rc
	grep    "found rc"   ${out_file} | tee ${rc_file}
	grep -v "found rc 0" ${rc_file}  | tee ${erc_file}
	[ -s "${erc_file}" ] && return 1
	${MOTRTOOL} ${get} >${out_file}
	rc=$?
	[ $rc != 0 ] && return $rc
	grep -A 1 "KEY" ${out_file} | tee ${res_out_file}
	local lines
	let lines=$(($num*3-1))
	[ $(cat ${res_out_file} | wc -l) == $lines ] || return 1
	while read key_line ; do
		read val_line
		key_line=${key_line#"/KEY/"}
		val_line=${val_line#"/VAL/"}
		[ "${val_line}" != "${key_line}" ] && return 1
		read tmp
	done < ${res_out_file}

	echo "insert test case done"
	return $rc
}

get_during() {
	local rc
	local fid=$(load_item $fids_file 1)
	local lookup="lookup \"${fid}\""
	local create="create \"${fid}\""
	local put="put \"${fid}\" @${keys_file} @${vals_file}"
	local get="get \"${fid}\" @${keys_file}"

	echo "Execute GET record operations between repair and rebalance"
	${MOTRTOOL} ${create} ${put}
	rc=$?
	[ $rc != 0 ] && return $rc
	device_fail
	rc=$?
	[ $rc != 0 ] && return $rc
	# Execute Repair.
	dix_repair_wait
	rc=$?
	[ $rc != 0 ] && return $rc
	# Check results.
	${MOTRTOOL} ${lookup} >${out_file}
	rc=$?
	[ $rc != 0 ] && return $rc
	grep    "found rc"   ${out_file} | tee ${rc_file}
	grep -v "found rc 0" ${rc_file}  | tee ${erc_file}
	[ -s "${erc_file}" ] && return 1
	${MOTRTOOL} ${get} >${out_file}
	rc=$?
	[ $rc != 0 ] && return $rc
	grep -A 1 "KEY" ${out_file} | tee ${res_out_file}
	local lines
	let lines=$num*3-1
	[ $(cat ${res_out_file} | wc -l) == $lines ] || return 1
	while read key_line ; do
		read val_line
		key_line=${key_line#"/KEY/"}
		val_line=${val_line#"/VAL/"}
		[ "${val_line}" != "${key_line}" ] && return 1
		read tmp
	done < ${res_out_file}
	# Execute Rebalance.
	dix_rebalance_wait
	rc=$?
	[ $rc != 0 ] && return $rc
	# Check results.
	${MOTRTOOL} ${lookup} >${out_file}
	rc=$?
	[ $rc != 0 ] && return $rc
	grep    "found rc"   ${out_file} | tee ${rc_file}
	grep -v "found rc 0" ${rc_file}  | tee ${erc_file}
	[ -s "${erc_file}" ] && return 1
	${MOTRTOOL} ${get} >${out_file}
	rc=$?
	[ $rc != 0 ] && return $rc
	grep -A 1 "KEY" ${out_file} | tee ${res_out_file}
	local lines
	let lines=$(($num*3-1))
	[ $(cat ${res_out_file} | wc -l) == $lines ] || return 1
	while read key_line ; do
		read val_line
		key_line=${key_line#"/KEY/"}
		val_line=${val_line#"/VAL/"}
		[ "${val_line}" != "${key_line}" ] && return 1
		read tmp
	done < ${res_out_file}

	echo "get_during test case done"
	return $rc
}

drop_during() {
	local rc
	local lookup="lookup @${fids_file}"
	local create="create @${fids_file}"
	local put="put @${fids_file} @${keys_file} @${vals_file}"
	local drop="drop @${fids_file}"

	echo "Execute drop index operations between repair and rebalance"
	${MOTRTOOL} ${create} ${put}
	rc=$?
	[ $rc != 0 ] && return $rc
	device_fail
	rc=$?
	[ $rc != 0 ] && return $rc
	# Execute Repair.
	dix_repair_wait
	rc=$?
	[ $rc != 0 ] && return $rc
	# Drop index
	${MOTRTOOL} ${drop} >${out_file}
	rc=$?
	[ $rc != 0 ] && return $rc
	grep    "operation rc:"   ${out_file} | tee ${rc_file}
	grep -v "operation rc: 0" ${rc_file}  | tee ${erc_file}
	[ -s "${erc_file}" ] && return 1
	# Execute Rebalance.
	dix_rebalance_wait
	rc=$?
	[ $rc != 0 ] && return $rc
	# Check results.
	${MOTRTOOL} ${lookup} >${out_file}
	rc=$?
	[ $rc != 0 ] && return $rc
	grep    "found rc"    ${out_file} | tee ${rc_file}
	grep -v "found rc -2" ${rc_file}  | tee ${erc_file}
	[ -s "${erc_file}" ] && return 1

	echo "drop_during test case done"
	return $rc
}

put_during() {
	local rc
	local fid=$(load_item $fids_file 1)
	local lookup="lookup \"${fid}\""
	local create="create \"${fid}\""
	local put="put \"${fid}\" @${keys_file} @${vals_file}"
	local get="get \"${fid}\" @${keys_file}"

	echo "Put record into empty index between repair and rebalance"
	${MOTRTOOL} ${create}
	rc=$?
	[ $rc != 0 ] && return $rc
	device_fail
	rc=$?
	[ $rc != 0 ] && return $rc
	# Execute Repair.
	dix_repair_wait
	rc=$?
	[ $rc != 0 ] && return $rc
	# Put several records
	${MOTRTOOL} ${put} >${out_file}
	rc=$?
	[ $rc != 0 ] && return $rc
	grep    "operation rc:"   ${out_file} | tee ${rc_file}
	grep -v "operation rc: 0" ${rc_file}  | tee ${erc_file}
	[ -s "${erc_file}" ] && return 1
	# Execute Rebalance.
	dix_rebalance_wait
	rc=$?
	[ $rc != 0 ] && return $rc
	# Try to get all records.
	${MOTRTOOL} ${get} >${out_file}
	rc=$?
	[ $rc != 0 ] && return $rc
	grep -A 1 "KEY" ${out_file} | tee ${res_out_file}
	local lines
	let lines=$(($num*3-1))
	[ $(cat ${res_out_file} | wc -l) == $lines ] || return 1
	while read key_line ; do
		read val_line
		key_line=${key_line#"/KEY/"}
		val_line=${val_line#"/VAL/"}
		[ "${val_line}" != "${key_line}" ] && return 1
		read tmp
	done < ${res_out_file}

	echo "put_during test case done"
	return $rc
}

del_repair() {
	local fid=$(load_item $fids_file 1)
	local create="create \"${fid}\""
	local del="del \"${fid}\" @${keys_file}"
	local put="put \"${fid}\" @${keys_file} @${vals_file}"
	local get="get \"${fid}\" @${keys_file}"
	echo "Del records from index in case repair is in active state"
	${MOTRTOOL} ${create} ${put}
	rc=$?
	[ $rc != 0 ] && return $rc
	device_fail
	rc=$?
	[ $rc != 0 ] && return $rc
	dix_repair
	rc=$?
	[ $rc != 0 ] && return $rc
	${MOTRTOOL} ${del} >${out_file}
	rc=$?
	[ $rc != 0 ] && return $rc
	grep    "operation rc:"   ${out_file} | tee ${rc_file}
	grep -v "operation rc: 0" ${rc_file}  | tee ${erc_file}
	[ -s "${erc_file}" ] && return 1
	dix_rep_wait
	rc=$?
	[ $rc != 0 ] && return $rc
	${MOTRTOOL} ${get} >${out_file}
	rc=$?
	[ $rc != 0 ] && return $rc
	grep    "operation rc:" ${out_file}  | tee ${rc_file}
	grep -v "operation rc: 0" ${rc_file} | tee ${erc_file}
	[ -s "${erc_file}" ] && return 1

	echo "del_repair test case done"
	return $rc
}

del_rebalance() {
	local fid=$(load_item $fids_file 1)
	local create="create \"${fid}\""
	local del="del \"${fid}\" @${keys_file}"
	local put="put \"${fid}\" @${keys_file} @${vals_file}"
	local get="get \"${fid}\" @${keys_file}"

	echo "Del records from index in case repair is in active state"
	${MOTRTOOL} ${create} ${put}
	rc=$?
	[ $rc != 0 ] && return $rc
	device_fail
	rc=$?
	[ $rc != 0 ] && return $rc
	dix_repair_wait
	rc=$?
	[ $rc != 0 ] && return $rc
	dix_rebalance
	${MOTRTOOL} ${del} >${out_file}
	rc=$?
	[ $rc != 0 ] && return $rc
	grep    "operation rc:"   ${out_file} | tee ${rc_file}
	grep -v "operation rc: 0" ${rc_file}  | tee ${erc_file}
	[ -s "${erc_file}" ] && return 1
	dix_reb_wait
	rc=$?
	[ $rc != 0 ] && return $rc
	${MOTRTOOL} ${get} >${out_file}
	rc=$?
	[ $rc != 0 ] && return $rc
	grep    "operation rc:"   ${out_file} | tee ${rc_file}
	grep -v "operation rc: 0" ${rc_file}  | tee ${erc_file}
	[ -s "${erc_file}" ] && return 1

	echo "del_rebalance test case done"
	return $rc
}

clear_kvs() {
	local drop="drop @${fids_file}"
	${MOTRTOOL} ${drop}
}

st_init() {
	local size=20
	local gen="genf ${num} ${fids_file} genv ${num} ${size} ${vals_file}"

	# generate source files for KEYS, VALS, FIDS
	${MOTRTOOL} ${gen}
	[ $? != 0 ] && return 1

	# We use this file because it contains human-readable values.
	cp $M0_SRC_DIR/dix/cm/st/src_vals.txt ${vals_file}

	cp ${vals_file} ${keys_file}
	ls -l ${fids_file} ${vals_file}
	echo ${fids_file}
	cat ${fids_file}
	echo ${vals_file}
	cat ${vals_file}
	return 0
}

execute_tests() {
	local rc
	local vflag=''

	if [ $verbose == 1 ] ; then
		vflag="-v"
	fi
	MOTRTOOL="$M0_SRC_DIR/motr/st/utils/m0kv_start.sh local $vflag index"

	st_init
	rc=$?
	if [ $rc -ne 0 ]; then
		echo "Failed to init test $rc"
		return $rc
	fi
	for f in ${tests_list[@]}; do
		clear_kvs
		echo "------------ Start to exec $f -------------"
		(${f})
		rc=$?
		echo "------------ Completed $f -----------------"
		if [ $rc != 0 ]; then
			echo "Failed: $f"
			return $rc
		fi
	done
	return $rc
}

motr_start() {
	local rc=0
	if [ $verbose == 1 ]; then
		motr_service start $multiple_pools $stride $N $K $P
	else
		motr_service start $multiple_pools $stride $N $K $P >/dev/null 2>&1
	fi
	rc=$?
	if [ $rc -ne "0" ]
	then
		echo "Failed to start Motr Service."
	fi
	return $rc
}

motr_stop() {
	local rc=0
	echo "Motr service stopping..."
	if [ $verbose == 1 ]; then
		motr_service stop
	else
		motr_service stop >/dev/null 2>&1
	fi
	rc=$?
	if [ $rc -ne "0" ]
	then
		echo "Failed to stop Motr Service."
	fi
	return $rc
}

dixinit_exec() {
	local m0dixinit="$M0_SRC_DIR/dix/utils/m0dixinit"
	local cmd
	local pverid=$(echo $DIX_PVERID | tr -d ^)

	if [ ! -f $dixinit] ; then
		echo "Can't find m0dixinit"
		return 1
	fi
	cmd="$m0dixinit -l $MOTR_LOCAL_EP -H $MOTR_HA_EP \
	     -p '$MOTR_PROF_OPT' -I '$pverid' -d '$pverid' -a create"
	echo $cmd
	eval "$cmd"
}

start() {
	local rc=0
	sandbox_init
	NODE_UUID=`uuidgen`
	local multiple_pools=0

	if [ ${do_motr_start} == 1 ]; then
		motr_start || error "Failed to start Motr service"
		do_dixinit=1
	fi
	spiel_prepare
	if [ ${do_dixinit} == 1 ]; then
#		dixinit_exec || error "m0dixinit failed ($?)!"
		echo "*** m0dixinit is omitted. Mkfs creates meta indices now."
	fi
	return $rc
}

stop() {
	spiel_cleanup
	if [ ${do_motr_start} == 1 ]; then
		motr_stop || error "Failed to stop Motr service"
	fi
}

main() {
	local rc
	start
	rc=$?
	[ $rc != 0 ] && return $rc
	execute_tests
	rc=$?
	[ $rc != 0 ] && "echo test cases failed"
	stop
	if [ $rc -eq 0 ]; then
		sandbox_fini
	else
		echo "Test log available at $MOTR_TEST_LOGFILE."
	fi
	report_and_exit dix-repair-st $rc
}

OPTS=`getopt -o vhni --long verbose,help,no-setup,no-dixinit -n 'parse-options' -- "$@"`
if [ $? != 0 ] ; then echo "Failed parsing options." >&2 ; exit 1 ; fi
eval set -- "$OPTS"
while true; do
	case "$1" in
	-v | --verbose )    verbose=1; shift ;;
	-h | --help )       usage ; exit 0; shift ;;
	-n | --no-setup )   do_motr_start=0; shift ;;
	-i | --no-dixinit ) do_dixinit=0; shift ;;
	-- )                shift; break ;;
	* )                 break ;;
	esac
done

arg_list=("$@")

for arg in "${arg_list[@]}"; do
	if [ "${arg:0:2}" != "--" -a "${arg:0:1}" != "-" ]; then
		tests_list+=(${arg})
	fi
done

if [ ${#tests_list[@]} -eq 0 ]; then
	declare -a tests_list=(
				lookup
				insert
#				get_during
#				drop_during
#				put_during
#				del_repair
#				del_rebalance
			)
fi

echo "We are going to test: ${tests_list[@]}"
trap interrupt SIGINT SIGTERM
main
