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


# System test based on m0kv tool.
# Checks motr index commands only for now.

#set -x

SANDBOX_DIR=${SANDBOX_DIR:-/var/motr/sandbox.m0kv-st}

emsg="---"
M0_SRC_DIR=`realpath $0`
M0_SRC_DIR=${M0_SRC_DIR%/*/*/*/*}
num=10
large_size=30 #131072
small_size=20

. $M0_SRC_DIR/utils/functions # sandbox_init, report_and_exit
# following scripts are mandatory for start the motr service (motr_start/stop)
. $M0_SRC_DIR/m0t1fs/linux_kernel/st/common.sh
. $M0_SRC_DIR/m0t1fs/linux_kernel/st/m0t1fs_common_inc.sh
. $M0_SRC_DIR/m0t1fs/linux_kernel/st/m0t1fs_server_inc.sh
. $M0_SRC_DIR/motr/st/utils/motr_local_conf.sh

out_file="${SANDBOX_DIR}/m0kv_st.log"
rc_file="${SANDBOX_DIR}/m0kv_st.cod.log"
erc_file="${SANDBOX_DIR}/m0kv_st.ecod.log"
res_out_file="${SANDBOX_DIR}/m0kv_st.results.log"

keys_file="${SANDBOX_DIR}/keys.txt"
keys_bulk_file="${SANDBOX_DIR}/keys_bulk.txt"
vals_file="${SANDBOX_DIR}/vals.txt"
vals_bulk_file="${SANDBOX_DIR}/vals_bulk.txt"
fids_file="${SANDBOX_DIR}/fids.txt"

genf="genf ${num} ${fids_file}"
genv_bulk="genv ${num} ${large_size} ${vals_bulk_file}"
genv="genv ${num} ${small_size} ${vals_file}"

do_motr_start=1
do_dixinit=1
verbose=0

NODE_UUID=`uuidgen`

interrupt() { echo "Interrupted by user" >&2; stop 2; }
error() { echo "$@" >&2; stop 1; }

m0kv_st_pre()
{
	sandbox_init
}

m0kv_st_post()
{
	local rc=${1:-$?}

	trap - SIGINT SIGTERM
	if [ $do_motr_start == 1 ]; then
		stop_motr || rc=$?
	fi

	if [ $rc -eq 0 ]; then
		sandbox_fini
	fi
	report_and_exit m0kv $rc
}

function usage()
{
	echo "m0kv_st.sh [-n -i -v -h] all|test1 test2 ..."
	echo "Options:"
	echo "    '-n|--no-setup'        don't start motr service"
	echo "    '-i|--no-dixinit'      don't create dix meta-indices"
	echo "                           note: ignored without --no-setup"
	echo "    '-v|--verbose'         output additional info into console"
	echo "    '-h|--help'            show this help"
	echo "Available tests:"
	echo "    'creates'       create index"
	echo "    'createm'       create several indices"
	echo "    'drops'         drop index"
	echo "    'dropm'         drop several indices"
	echo "    'list'          get list of indices"
	echo "    'lookup'        lookup indices"
	echo "    'puts1'         put key into index"
	echo "    'putsN_exist'   put existing key into index"
	echo "    'bputsN_exist'  put existing key (batch)"
	echo "    'putsN'         put several keys into index"
	echo "    'putmN'         put several keys into several indices"
	echo "    'putsNb'        put several keys into index (use bulk)"
	echo "    'gets1'         get key-value from index"
	echo "    'bgets1'        get key-value from index (batch)"
	echo "    'getsN'         get several key-values from index"
	echo "    'bgetsN'        get several key-values from index (batch)"
	echo "    'nextsN'        get next key-values from index"
	echo "    'bnextsN'       get next key-values from index (batch)"
	echo "    'dels1'         delete key from index"
	echo "    'bdels1'        delete key from index (batch)"
	echo "    'delsN'         delete keys from index"
	echo "    'bdelsN'        delete keys from index (batch)"
	echo "    'delmN'         delete keys from several indices"
	echo "    'bdelmN'        delete keys from several indices (batch)"
	echo "    empty list or 'all'  enable all tests"
	echo "Examples:"
	echo "./m0kv_st.sh --no-setup all"
	echo "./m0kv_st.sh creates putsN --verbose"
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

function rm_logs()
{
	rm -rf ${out_file} ${rc_file} ${erc_file} ${res_out_file} >/dev/null
}

creates()
{
	local rc=0
	echo "Test:Create"
	emsg="FAILED to create index by fid:${fid}"
	${MOTRTOOL} ${drop} >/dev/null
	${MOTRTOOL} ${create} >${out_file}
	rc=$?
	[ $rc != 0 ] && return $rc
	#check output: should be rc code 0
	exec grep "operation rc:" ${out_file} | grep -v "operation rc: 0" >${erc_file}
	[ -s "${erc_file}" ] && return 1
	rm_logs
	return $rc
}

createm()
{
	local rc=0
	echo "Test:Create FIDS"
	emsg="FAILED to create index by @fids:${fids_file}"
	${MOTRTOOL} ${dropf} >/dev/null
	${MOTRTOOL} ${create} >/dev/null
	rc=$?
	[ $rc != 0 ] && return $rc
	${MOTRTOOL} ${createf} >>${out_file}
	rc=$?
	[ $rc != 0 ] && return $rc
	grep "operation rc:" ${out_file} >${rc_file}
	grep -v "operation rc: 0" ${rc_file} >${erc_file}
	[ $(cat ${erc_file} | wc -l) == 1 ] || return 1
	rm_logs
	return $rc
}

drops()
{
	local rc=0
	echo "Test:Drop"
	emsg="FAILED to drop index by fid:${fid}"
	${MOTRTOOL} ${create} >/dev/null
	rc=$?
	[ $rc != 0 ] && return $rc
	${MOTRTOOL} ${drop} >${out_file}
	rc=$?
	[ $rc != 0 ] && return $rc
	grep "operation rc:" ${out_file} >${rc_file}
	grep -v "operation rc: 0" ${rc_file} >${erc_file}
	[ -s "${erc_file}" ] && return 1
	rm_logs
	return $rc
}

dropm()
{
	local rc=0
	echo "Test:Drop FIDS"
	emsg="FAILED to drop index by @fids:${fids_file}"
	${MOTRTOOL} ${createf} >/dev/null
	rc=$?
	[ $rc != 0 ] && return $rc
	${MOTRTOOL} ${drop} >${out_file}
	[ $rc != 0 ] && return $rc
	grep "operation rc:" ${out_file} >${rc_file}
	grep -v "operation rc: 0" ${rc_file} >${erc_file}
	[ -s "${erc_file}" ] && return 1
	rm_logs
	${MOTRTOOL} ${dropf} >${out_file}
	rc=$?
	[ $rc != 0 ] && return $rc
	grep "operation rc:" ${out_file} >${rc_file}
	grep -v "operation rc: 0" ${rc_file} >${erc_file}
	[ $(cat ${erc_file} | wc -l) == 1 ] || return 1
	rm_logs
	return $rc
}

list()
{
	local rc=0
	echo "Test:List"
	emsg="FAILED to get List"
	${MOTRTOOL} ${dropf} >/dev/null
	${MOTRTOOL} ${createf} >/dev/null
	${MOTRTOOL} ${list3} >${out_file}
	grep "<" ${out_file} >${res_out_file}
	[ $(cat ${res_out_file} | wc -l) == 3 ] || return 1
	${MOTRTOOL} ${list} >${out_file}
	grep "<" ${out_file} >${res_out_file}
	[ $(cat ${res_out_file} | wc -l) == ${num} ] || return 1
	rm_logs
	return $rc
}

lookup()
{
	local rc=0
	echo "Test:Lookup"
	emsg="FAILED to check lookup functionality"
	${MOTRTOOL} ${dropf} >/dev/null
	${MOTRTOOL} ${create} >/dev/null
	${MOTRTOOL} ${lookup} >${out_file}
	grep "found rc" ${out_file} >${rc_file}
	grep -v "found rc 0" ${rc_file} >${erc_file}
	[ -s "${erc_file}" ] && return 1

	${MOTRTOOL} ${drop} >/dev/null
	${MOTRTOOL} ${lookup} >${out_file}
	grep "found rc" ${out_file} >${rc_file}
	grep -v "found rc -2" ${rc_file} >${erc_file}
	[ -s "${erc_file}" ] && return 1
	return $rc
}

puts1()
{
	local rc=0
	echo "Test:Put KEY"
	emsg="FAILED to put key ${key} val ${val} into ${fid}"
	${MOTRTOOL} ${dropf} >/dev/null
	rc=$?
	[ $rc != 0 ] && return $rc
	${MOTRTOOL} ${create} >/dev/null
	rc=$?
	[ $rc != 0 ] && return $rc
	${MOTRTOOL} ${put_fid_key} >${out_file}
	[ $rc != 0 ] && return $rc
	grep "operation rc:" ${out_file} >${rc_file}
	grep -v "operation rc: 0" ${rc_file} >${erc_file}
	[ -s "${erc_file}" ] && return 1
	rm_logs
	return $rc
}

putsN_exist()
{
	local rc=0
	echo "Test:Put existing KEYS"
	emsg="FAILED to put existing @keys:${keys_file} @vals:${vals_file} into ${fid}"
	${MOTRTOOL} ${dropf} >/dev/null
	rc=$?
	[ $rc != 0 ] && return $rc
	${MOTRTOOL} ${create} >/dev/null
	rc=$?
	[ $rc != 0 ] && return $rc
	${MOTRTOOL} ${put_fid_key} >/dev/null
	rc=$?
	[ $rc != 0 ] && return $rc
	${MOTRTOOL} ${put_fid_keys} >${out_file}
	rc=$?
	rm_logs
	return $rc
}

bputsN_exist()
{
	local rc=0
	echo "Test:Put existing KEYS by one command"
	emsg="FAILED to put existing @keys:${keys_file} @vals:${vals_file} into ${fid}"
	${MOTRTOOL} ${dropf} ${create} ${put_fid_key} ${put_fid_keys} >${out_file}
	rc=$?
	rm_logs
	return $rc
}

putsN()
{
	local rc=0
	echo "Test:Put KEYS"
	emsg="FAILED to put new @keys:${keys_file} @vals:${vals_file} into ${fid}"
	${MOTRTOOL} ${dropf} >/dev/null
	rc=$?
	[ $rc != 0 ] && return $rc
	${MOTRTOOL} ${create} >/dev/null
	rc=$?
	[ $rc != 0 ] && return $rc
	${MOTRTOOL} ${put_fid_keys} >${out_file}
	rc=$?
	[ $rc != 0 ] && return $rc
	grep "operation rc:" ${out_file} >${rc_file}
	grep -v "operation rc: 0" ${rc_file} >${erc_file}
	[ -s "${erc_file}" ] && return 1
	rm_logs
	return $rc
}

putsNb()
{
	local rc=0
	echo "Test:Put KEYS (bulk)"
	emsg="FAILED to put new @keys:${keys_bulk_file} @vals:${vals_bulk_file} into ${fid}"
	${MOTRTOOL} ${dropf} >/dev/null
	rc=$?
	[ $rc != 0 ] && return $rc
	${MOTRTOOL} ${create} >/dev/null
	rc=$?
	[ $rc != 0 ] && return $rc
	${MOTRTOOL} ${put_fid_keys_bulk} >${out_file}
	rc=$?
	[ $rc != 0 ] && return $rc
	grep "operation rc:" ${out_file} >${rc_file}
	grep -v "operation rc: 0" ${rc_file} >${erc_file}
	[ -s "${erc_file}" ] && return 1
	rm_logs
	return $rc
}

putmN()
{
	local rc=0
	echo "Test:Put KEYS into FIDS"
	emsg="FAILED to put new @keys:${keys_file} @vals:${vals_file} into @fids:${fid_file}"
	${MOTRTOOL} ${dropf} >/dev/null
	rc=$?
	[ $rc != 0 ] && return $rc
	${MOTRTOOL} ${createf} >/dev/null
	rc=$?
	[ $rc != 0 ] && return $rc
	${MOTRTOOL} ${put_fids_keys} >${out_file}
	[ $rc != 0 ] && return $rc
	grep "operation rc:" ${out_file} >${rc_file}
	grep -v "operation rc: 0" ${rc_file} >${erc_file}
	[ -s "${erc_file}" ] && return 1
	rm_logs
	return $rc
}

gets1()
{
	local rc=0
	echo "Test:Get KEY"
	emsg="FAILED to get key:${key} from fid:${fid}"
	${MOTRTOOL} ${dropf} >/dev/null
	rc=$?
	[ $rc != 0 ] && return $rc
	${MOTRTOOL} ${create} >/dev/null
	rc=$?
	[ $rc != 0 ] && return $rc
	${MOTRTOOL} ${put_fid_key} >/dev/null
	rc=$?
	[ $rc != 0 ] && return $rc
	${MOTRTOOL} ${get_fid_key} >${out_file}
	rc=$?
	[ $rc != 0 ] && return $rc
	grep "operation rc:" ${out_file} >${rc_file}
	grep -v "operation rc: 0" ${rc_file} >${erc_file}
	[ -s "${erc_file}" ] && return 1
	grep -A 1 "KEY" ${out_file} > ${res_out_file}
	key_line=$(load_item ${res_out_file} 1)
	val_line=$(load_item ${res_out_file} 2)
	key_line=${key_line#"/KEY/"}
	val_line=${val_line#"/VAL/"}
	[ "${val_line}" != "${key_line}" ] && return 1
	[ $(cat ${res_out_file} | wc -l) == 2 ] || return 1
	rm_logs
	return $rc
}

bgets1()
{
	local rc=0
	echo "Test:Get KEY by one command"
	emsg="FAILED to get key:${key} from fid:${fid} by one command"
	${MOTRTOOL} ${dropf} ${create} ${put_fid_key} ${get_fid_key} >${out_file}
	rc=$?
	[ $rc != 0 ] && return $rc
	grep -A 1 "KEY" ${out_file} > ${res_out_file}
	key_line=$(load_item ${res_out_file} 1)
	val_line=$(load_item ${res_out_file} 2)
	key_line=${key_line#"/KEY/"}
	val_line=${val_line#"/VAL/"}
	[ "${val_line}" != "${key_line}" ] && return 1
	[ $(cat ${res_out_file} | wc -l) == 2 ] || return 1
	rm_logs
	return $rc
}

getsN()
{
	local rc=0
	echo "Test:Get KEYS"
	emsg="FAILED to get @keys:${keys_file} from fid:${fid}"
	${MOTRTOOL} ${dropf} ${create} ${put_fid_keys} >/dev/null
	rc=$?
	[ $rc != 0 ] && return $rc
	${MOTRTOOL} ${get_fid_keys} >${out_file}
	grep -A 1 "KEY" ${out_file} > ${res_out_file}
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
	rm_logs
	return $rc
}

bgetsN()
{
	local rc=0
	echo "Test:Get KEYS by one command"
	emsg="FAILED to get @keys:${keys_file} from fid:${fid} by one command"
	${MOTRTOOL} ${dropf} ${create} ${put_fid_keys} ${get_fid_keys} >${out_file}
	rc=$?
	[ $rc != 0 ] && return $rc
	grep -A 1 "KEY" ${out_file} > ${res_out_file}
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
	rm_logs
	return $rc
}

nextsN()
{
	local rc=0
	echo "Test:Get next keys"
	emsg="FAILED to get next keys from ${fid} start key ${key}"
	${MOTRTOOL} ${dropf} ${create} ${put_fid_keys} >/dev/null
	rc=$?
	[ $rc != 0 ] && return $rc
	${MOTRTOOL} ${next} >${out_file}
	rc=$?
	[ $rc != 0 ] && return $rc
	grep -A 1 "KEY" ${out_file} > ${res_out_file}
	while read key_line ; do
		read val_line
		key_line=${key_line#"/KEY/"}
		val_line=${val_line#"/VAL/"}
		[ "${val_line}" != "${key_line}" ] && return 1
		read tmp
	done < ${res_out_file}
	return $rc
}

bnextsN()
{
	local rc=0
	echo "Test:Get next keys by one command"
	emsg="FAILED to get next keys from ${fid} start key ${key} by one command"
	${MOTRTOOL} ${dropf} ${create} ${put_fid_keys} ${next} >${out_file}
	rc=$?
	[ $rc != 0 ] && return $rc
	grep -A 1 "KEY" ${out_file} > ${res_out_file}
	while read key_line ; do
		read val_line
		key_line=${key_line#"/KEY/"}
		val_line=${val_line#"/VAL/"}
		[ "${val_line}" != "${key_line}" ] && return 1
		read tmp
	done < ${res_out_file}
	return $rc
}

dels1()
{
	local rc=0
	echo "Test:Del existing KEY"
	emsg="FAILED to del key:${key} from ${fid}"
	${MOTRTOOL} ${dropf} ${create} ${put_fid_key} >/dev/null
	rc=$?
	[ $rc != 0 ] && return $rc
	${MOTRTOOL} $del_fid_key >${out_file}
	rc=$?
	[ $rc != 0 ] && return $rc
	grep "operation rc:" ${out_file} >${rc_file}
	grep -v "operation rc: 0" ${rc_file} >${erc_file}
	[ -s "${erc_file}" ] && return 1
	rm_logs
	return $rc
}

bdels1()
{
	local rc=0
	echo "Test:Del existing KEY by one command"
	emsg="FAILED to del key:${key} from ${fid} by one command"
	${MOTRTOOL} ${dropf} ${create} ${put_fid_key} $del_fid_key>${out_file}
	rc=$?
	[ $rc != 0 ] && return $rc
	grep -B 1 "del done:" ${out_file} >${rc_file}
	grep -v ": 0" ${rc_file} >${erc_file}
	[ -s "${erc_file}" ] && return 1
	rm_logs
	return $rc
}

delsN()
{
	local rc=0
	echo "Test:Del existing KEYS"
	emsg="FAILED to del @keys:${keys_file} from ${fid}"
	${MOTRTOOL} ${dropf} ${create} ${put_fid_keys} >/dev/null
	rc=$?
	[ $rc != 0 ] && return $rc
	${MOTRTOOL} ${del_fid_keys} >${out_file}
	rc=$?
	[ $rc != 0 ] && return $rc
	grep "operation rc:" ${out_file} >${rc_file}
	grep -v "operation rc: 0" ${rc_file} >${erc_file}
	[ -s "${erc_file}" ] && return 1
	rm_logs
	return $rc
}

bdelsN()
{
	local rc=0
	echo "Test:Del existing KEYS by one command"
	emsg="FAILED to del keys:@${keys_file} from ${fid} by one command"
	${MOTRTOOL} ${dropf} ${create} ${put_fid_keys} ${del_fid_keys} >${out_file}
	rc=$?
	[ $rc != 0 ] && return $rc
	grep -B 1 "del done:" ${out_file} >${rc_file}
	grep -v ": 0" ${rc_file} >${erc_file}
	[ -s "${erc_file}" ] && return 1
	rm_logs
	return $rc
}

delmN()
{
	local rc=0
	echo "Test:Del existing KEYS from several indices"
	emsg="FAILED to del @keys:${keys_file} @vals:${vals_file} from @${fid_file}"
	${MOTRTOOL} ${dropf} ${createf} ${put_fids_keys} >/dev/null
	rc=$?
	[ $rc != 0 ] && return $rc
	${MOTRTOOL} ${del_fids_keys} >${out_file}
	[ $rc != 0 ] && return $rc
	grep "operation rc:" ${out_file} >${rc_file}
	grep -v "operation rc: 0" ${rc_file} >${erc_file}
	[ -s "${erc_file}" ] && return 1
	rm_logs
	return $rc
}

bdelmN()
{
	local rc=0
	echo "Test:Del existing KEYS in several indices by one command"
	emsg="FAILED to del keys:@${keys_file} from @${fid_file} by one command"
	${MOTRTOOL} ${dropf} ${createf} ${put_fids_keys} ${del_fids_keys} >${out_file}
	rc=$?
	[ $rc != 0 ] && return $rc
	grep -B 1 "del done:" ${out_file} >${rc_file}
	grep -v ": 0" ${rc_file} >${erc_file}
	[ -s "${erc_file}" ] && return 1
	rm_logs
	return $rc
}

st_init()
{
	# generate source files for KEYS, VALS, FIDS
	${MOTRTOOL} ${genf} ${genv} ${genv_bulk} >/dev/null
	[ $? != 0 ] && return 1
	cp ${vals_file} ${keys_file}
	cp ${vals_bulk_file} ${keys_bulk_file}
	fid=$(load_item $fids_file 1)
	key=$(load_item $keys_file 1)
	val=$(load_item $vals_file 1)
	t=${key#* }
	key=$t
	t=${val#* }
	val=$t
	dropf="drop @${fids_file}"
	drop="drop \"${fid}\""
	createf="create @${fids_file}"
	create="create \"${fid}\""

	put_fids_keys="put @${fids_file} @${keys_file} @${vals_file}"
	put_fids_key="put @${fids_file} ${key} ${val}"
	put_fid_keys="put \"${fid}\" @${keys_file} @${vals_file}"
	put_fid_key="put \"${fid}\" ${key} ${val}"
	put_fid_keys_bulk="put \"${fid}\" @${keys_bulk_file} @${vals_bulk_file}"

	del_fids_keys="del @${fids_file} @${keys_file}"
	del_fids_key="del @${fids_file} ${key}"
	del_fid_keys="del \"${fid}\" @${keys_file}"
	del_fid_key="del \"${fid}\" ${key}"

	get_fid_keys="get \"${fid}\" @${keys_file}"
	get_fid_key="get \"${fid}\" ${key}"

	next="next \"${fid}\" ${key} 10"

	lookup="lookup \"${fid}\""
	lookups="lookup @${fids_file}"

	list="list \"${fid}\" ${num}"
	list3="list \"${fid}\" 3"
}


start_motr()
{
	local rc=0
	local multiple_pools=0
	echo "Motr service starting..."
	if [ $verbose == 1 ]; then
		motr_service start $multiple_pools
	else
		motr_service start $multiple_pools >/dev/null 2>&1
	fi
	rc=$?
	if [ $rc -ne "0" ]
	then
		echo "Failed to start Motr Service."
	fi
	return $rc
}

stop_motr()
{
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

m0kv_st_start_tests()
{
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
		(${f})
		rc=$?
		if [ $rc != 0 ]; then
			echo "Failed: $f"
			echo "MSG   : ${emsg}"
			return $rc
		fi
	done
	return $rc
}

dixinit_exec()
{
	local m0dixinit="$M0_SRC_DIR/dix/utils/m0dixinit"
	local cmd
	local pverid=$(echo $DIX_PVERID | tr -d ^)

	if [ ! -f $dixinit] ; then
		echo "Can't find m0dixinit"
		return 1
	fi
#	cmd="$m0dixinit -l $MOTR_LOCAL_EP -H $MOTR_HA_EP \
#	     -p '$MOTR_PROF_OPT' -I '$pverid' -d '$pverid' -a create"
#	echo $cmd
#	eval "$cmd"
	echo "*** m0dixinit is omitted. Mkfs creates meta indices now."
}

main()
{
	m0kv_st_pre
	if [ ${do_motr_start} == 1 ]; then
		start_motr || error "Failed to start Motr service"
		do_dixinit=1
	fi

	if [ ${do_dixinit} == 1 ]; then
		dixinit_exec || error "m0dixinit failed ($?)!"
	fi
	m0kv_st_start_tests
	m0kv_st_post
}

arg_list=("$@")

[ `id -u` -eq 0 ] || die 'Must be run by superuser'

OPTS=`getopt -o vhni --long verbose,no-setup,no-dixinit,help -n 'parse-options' -- "$@"`
if [ $? != 0 ] ; then echo "Failed parsing options." >&2 ; exit 1 ; fi
eval set -- "$OPTS"
while true; do
	case "$1" in
	-v | --verbose ) verbose=1; shift ;;
	-h | --help )    usage ; exit 0; shift ;;
	-n | --no-setup ) do_motr_start=0; shift ;;
	-i | --no-dixinit ) do_dixinit=0; shift ;;
	-- ) shift; break ;;
	* ) break ;;
	esac
done

for arg in "${arg_list[@]}"; do
	if [ "${arg:0:2}" != "--" -a "${arg:0:1}" != "-" ]; then
		tests_list+=(${arg})
	fi
done

if [ ${#tests_list[@]} -eq 0 ]; then
	declare -a tests_list=(creates
				createm
				drops
				dropm
				list
				lookup
				puts1
				putsN_exist
				bputsN_exist
				putsN
				putmN
				putsNb
				gets1
				bgets1
				getsN
				bgetsN
				nextsN
				bnextsN
				dels1
				bdels1
				delsN
				bdelsN
				delmN
				bdelmN)
fi

trap interrupt SIGINT SIGTERM
main

# Local variables:
# sh-basic-offset: 8
# sh-indentation: 8
# tab-width: 8
# End:
