#!/usr/bin/env bash
#
# Copyright (c) 2022 Seagate Technology LLC and/or its Affiliates
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

TOPDIR=$(dirname "$0")/../../

. "${TOPDIR}/m0t1fs/linux_kernel/st/common.sh"
. "${TOPDIR}/m0t1fs/linux_kernel/st/m0t1fs_common_inc.sh"
. "${TOPDIR}/m0t1fs/linux_kernel/st/m0t1fs_client_inc.sh"
. "${TOPDIR}/m0t1fs/linux_kernel/st/m0t1fs_server_inc.sh"
. "${TOPDIR}/m0t1fs/linux_kernel/st/m0t1fs_sns_common_inc.sh"

export MOTR_CLIENT_ONLY=1


# Suppose we have P number of catalogue services, usually the same number
# of io services in a Motr cluster.
# DIX layout: [1 + PARITY + 0]. PARITY is the extra replica number.
# If PARITY is (P-1), then every KV pair in a DIX has (1+PARITY) replicas.
# If (1+PARITY) equals to P, then every catalogue will have all KV pairs.
# So, in this case, Parity Group width equals Pool width.
# Please see dix_pver_build() in m0t1fs/linux_kernel/st/m0t1fs_common_inc.sh
export MOTR_DIX_PG_N_EQ_P=YES


DIX_FID="7800000000012345:12345"
proc_fid="0x7200000000000001:0"
num_of_kv=16
key_prefix="iter-äää"
val_prefix="something1_anotherstring2*YETanotherstring3"

kvs_create_n_insert()
{
	local rc=0
	local cli_ep=${lnet_nid}:$SNS_MOTR_CLI_EP
	local ha_ep=${lnet_nid}:$HA_EP

	MOTR_PARAM="-l $cli_ep  -h $ha_ep -p $PROF_OPT \
		    -f $M0T1FS_PROC_ID -s "

	echo "$M0_SRC_DIR/utils/m0kv" "${MOTR_PARAM}" index create "$DIX_FID"

	"$M0_SRC_DIR/utils/m0kv" "${MOTR_PARAM}"                                     \
				index create "$DIX_FID"                            \
			 || {
		rc=$?
		echo "m0kv failed"
	}

	for ((j=0; j<$num_of_kv; j++)); do
		"$M0_SRC_DIR/utils/m0kv" "${MOTR_PARAM}"                                       \
			index put    "$DIX_FID" "$key_prefix-$j" "$val_prefix-$j"
	done
	return $rc
}

ctgdump_comp()
{
	local dir=$1
	local cas_ep=$2
	local proc_fid=$3
	local dix_fid=$4
	local no_diff=YES

	ios_num=$(find "$dir"/ios* -maxdepth 0 -type d | wc -l)
	dump_cmd=$M0_SRC_DIR/cas/m0ctgdump

	rm -f "$SANDBOX_DIR/kv.src"
	for ((j=0; j<$num_of_kv; j++)); do
		echo "{key: $key_prefix-$j}, {val: $val_prefix-$j}" |tee -a "$SANDBOX_DIR/kv.src"
	done
	sort "$SANDBOX_DIR/kv.src" > "$SANDBOX_DIR/kv.src.sorted"
	rm -f "$SANDBOX_DIR/kv.src"

	# Dump ctg store
	for ((i=1; i<=$ios_num; i++)) ; do
		be_stob_dir=$dir/ios$i
		dump_cmd_args="-T ad -d ${be_stob_dir}/disks.conf -D ${be_stob_dir}/db -S ${be_stob_dir}/stobs \
			       -A linuxstob:addb-stobs -w 4 -m 65536 -q 16 -N 100663296 -C 262144 -K 100663296 \
			       -k 262144 -c ${dir}/confd/conf.xc -e ${cas_ep} -f ${proc_fid} str ${dix_fid} "
		echo "running: $dump_cmd $dump_cmd_args"
		$dump_cmd "$dump_cmd_args" | sort > "$SANDBOX_DIR/ios$i.dump"
	done

	# Compare each dump file
	for ((i=1; i<=$ios_num; i++)) ; do
		diff -u "$SANDBOX_DIR/ios$i.dump" "$SANDBOX_DIR/kv.src.sorted" || {
			echo "DIX $DIX_FID on ios$i and src are different!."
			no_diff=NO
		}
	done

	if [ "x${no_diff}" == "xYES" ]; then
		echo "All CTG dumps match!"
		return 0
	else
		echo "Error: some CTG dump does not match!"
		return 1
	fi
}

ctgdump_test()
{
	local rc=0

	sandbox_init

	local multiple_pools=0
	motr_service start "$multiple_pools" "$stride" "$N" "$K" "$S" "$P" || {
		echo "Failed to start Motr Service."
		return 1
	}

	kvs_create_n_insert || {
		# Make the rc available for the caller and fail the test
		# if kv operations fail.
		rc=$?
		echo "Test failed with error $rc"
	}

	motr_service stop || {
		echo "Failed to stop Motr Service."
		rc=1
	}

	if [ $rc == 0 ]; then
		ctgdump_comp "$SANDBOX_DIR" "${XPRT}":"${lnet_nid}":"${IOSEP[0]}" "${proc_fid}" $DIX_FID
		rc=$?
	fi

	if [ $rc -eq 0 ]; then
		sandbox_fini
	fi
	return $rc
}

cmd=$1
case "$cmd" in
	test)
		ctgdump_test
		rc=$?
		;;
	comp)
		ctgdump_comp "$2" "$3" "$4" "$5"
		rc=$?
		;;
	   *)
		echo "Usage: $0 {test|comp}"
esac

report_and_exit "ctgdump" $rc
