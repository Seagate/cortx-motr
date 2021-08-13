#!/usr/bin/env bash
#
# Copyright (c) 2021 Seagate Technology LLC and/or its Affiliates
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

wait_and_exit()
{
	while [ true ] ; do
		echo "Please type EXIT or QUIT to quit"
		read keystroke
		if [ "$keystroke" == "EXIT" -o "$keystroke" == "QUIT" ] ; then
			return 0
		fi
	done
}

do_some_kv_operations()
{
	local rc=0

	for ((i=0; i < 1; i++)) ; do
		DIX_FID="12345:12345$i"
		MOTR_PARAM="-l ${lnet_nid}:$SNS_MOTR_CLI_EP  \
			    -h ${lnet_nid}:$HA_EP -p $PROF_OPT \
			    -f $M0T1FS_PROC_ID -s "

		$M0_SRC_DIR/utils/m0kv ${MOTR_PARAM}                                       \
					index create "$DIX_FID"                            \
					      put    "$DIX_FID" "somekey" "somevalue"      \
					      get    "$DIX_FID" "somekey"                  \
					      put    "$DIX_FID" "key1" "something1 anotherstring2 YETanotherstring3"      \
					      get    "$DIX_FID" "key1"                     \
					      put    "$DIX_FID" "key2" "something1_anotherstring2*YETanotherstring3"      \
					      get    "$DIX_FID" "key2"                     \
				 || {
			rc=$?
			echo "m0kv failed"
		}

		echo "Now, let's delete 'key2' from this index"
		$M0_SRC_DIR/utils/m0kv ${MOTR_PARAM}                                       \
					index del    "$DIX_FID" "key2"                     \
				 || {
			rc=$?
			echo "'m0kv index del' failed"
		}

		echo "Now, let's get 'key2' from this index again. It should fail."
		$M0_SRC_DIR/utils/m0kv ${MOTR_PARAM}                                       \
					index get    "$DIX_FID" "key2"                     \
				 && {
			rc=$?
			echo "'m0kv index get' is expected to fail, but not"
		}

	done
	return $rc
}

start_fdmi_plugin()
{
	local rc=0

	# We are using FDMI_PLUGIN_EP
	MOTR_PARAM="-l ${lnet_nid}:$FDMI_PLUGIN_EP        \
		    -h ${lnet_nid}:$HA_EP -p $PROF_OPT    \
		    -f $M0T1FS_PROC_ID                    "

	$M0_SRC_DIR/fdmi/plugins/fdmi_sample_plugin $MOTR_PARAM -g "$FDMI_FILTER_FID" &
	sleep 5

	return $rc
}



motr_fdmi_plugin_test()
{
	local rc=0

	echo "Starting Motr FDMI Plugin testing ..."

	echo
	echo
	echo "MOTR is UP."
	echo "Motr client config:"
	echo
	echo "HA_addr        : ${lnet_nid}:$HA_EP           "
	echo "Client_addr    : ${lnet_nid}:$SNS_MOTR_CLI_EP "
	echo "Profile_fid    : $PROF_OPT                    "
	echo "Process_fid    : $M0T1FS_PROC_ID              "
	echo "FDMI_plugin_ep : ${lnet_nid}:$FDMI_PLUGIN_EP  "
	echo "FDMI_FILTER_FID: $FDMI_FILTER_FID             "
	echo
	echo

	start_fdmi_plugin

	do_some_kv_operations

#	wait_and_exit

	sleep 3

	local pid=$(pgrep -f "lt-fdmi_sample_plugin")
	echo "Terminating ${pid}"
	kill -TERM "${pid}"
	wait "${pid}"

	return 0
}

main()
{
	local rc=0

	sandbox_init

	local multiple_pools=0
	motr_service start "$multiple_pools" "$stride" "$N" "$K" "$S" "$P" || {
		echo "Failed to start Motr Service."
		return 1
	}


	if [[ $rc -eq 0 ]] && ! motr_fdmi_plugin_test ; then
		echo "Failed: SNS repair failed.."
		rc=1
	fi

	motr_service stop || {
		echo "Failed to stop Motr Service."
		rc=1
	}

	if [ $rc -eq 0 ]; then
		sandbox_fini
	fi
	return $rc
}

trap unprepare EXIT
main
report_and_exit "motr_fdmi_plugin_test" $?
