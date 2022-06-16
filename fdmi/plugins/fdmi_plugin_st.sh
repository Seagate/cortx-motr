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
# Authors:
#   hua.huang@seagate.com
#   yuriy.umanets@seagate.com
#


TOPDIR=$(dirname "$0")/../../

. "${TOPDIR}/m0t1fs/linux_kernel/st/common.sh"
. "${TOPDIR}/m0t1fs/linux_kernel/st/m0t1fs_common_inc.sh"
. "${TOPDIR}/m0t1fs/linux_kernel/st/m0t1fs_client_inc.sh"
. "${TOPDIR}/m0t1fs/linux_kernel/st/m0t1fs_server_inc.sh"
. "${TOPDIR}/m0t1fs/linux_kernel/st/m0t1fs_sns_common_inc.sh"


export MOTR_CLIENT_ONLY=1
export ENABLE_FDMI_FILTERS=YES
interactive=false

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

	for ((i=0; i<1; i++)) ; do
		DIX_FID="12345:12345$i"
		MOTR_PARAM="-l ${lnet_nid}:$SNS_MOTR_CLI_EP  \
			    -h ${lnet_nid}:$HA_EP -p $PROF_OPT \
			    -f $M0T1FS_PROC_ID -s "

		echo "Let's create an index and put {somekey:somevalue}"
		"$M0_SRC_DIR/utils/m0kv" "${MOTR_PARAM}"                                       \
					index create "$DIX_FID"                            \
					      put    "$DIX_FID" "somekey" "somevalue"      \
					      get    "$DIX_FID" "somekey"                  \
				 || {
			rc=$?
			echo "m0kv failed"
		}
		if $interactive ; then echo "Press Enter to go ..." && read; fi

		echo "Let's put {key1: ***}"
		"$M0_SRC_DIR/utils/m0kv" "${MOTR_PARAM}"                                       \
					index put    "$DIX_FID" "key1" "something1 anotherstring2 YETanotherstring3"      \
					      get    "$DIX_FID" "key1"                     \
				 || {
			rc=$?
			echo "m0kv failed"
		}
		if $interactive ; then echo "Press Enter to go ..." && read; fi

		echo "Let's put {key2: ***}"
		"$M0_SRC_DIR/utils/m0kv" "${MOTR_PARAM}"                                       \
					index put    "$DIX_FID" "key2" "something1_anotherstring2*YETanotherstring3"      \
					      get    "$DIX_FID" "key2"                     \
				 || {
			rc=$?
			echo "m0kv failed"
		}
		if $interactive ; then echo "Press Enter to go ..." && read; fi

		echo "Now, kill the two fdmi plug app and start them again..."
		echo "This is to simulate the plugin failure and start"

		rc=1
		sleep 5 && stop_fdmi_plugin      && sleep 5              &&
		start_fdmi_plugin "$FDMI_FILTER_FID"  "$FDMI_PLUGIN_EP"  &&
		start_fdmi_plugin "$FDMI_FILTER_FID2" "$FDMI_PLUGIN_EP2" && rc=0
		if [[ $rc -eq 1 ]] ; then
			echo "Can not stop and start plug again".
			return $rc
		fi
		sleep 5
		rc=0

		echo "Let's put {key3: ***}"
		"$M0_SRC_DIR/utils/m0kv" "${MOTR_PARAM}"                                       \
					index put    "$DIX_FID" "key3" "{Bucket-Name:SomeBucket, Object-Name:Someobject, x-amz-meta-replication:Pending}"      \
					      get    "$DIX_FID" "key3"                     \
				 || {
			rc=$?
			echo "m0kv failed"
		}
		if $interactive ; then echo "Press Enter to go ..." && read; fi

		echo "Now do more index put ..."
		echo "Because the plug was restarted, these operations "
		echo "will trigger failure handling"
		for ((j=0; j<10; j++)); do
			echo "j=$j"
			"$M0_SRC_DIR/utils/m0kv" "${MOTR_PARAM}"                                       \
					index put    "$DIX_FID" "iter-äää-$j" "something1_anotherstring2*YETanotherstring3-$j"
		done
		if $interactive ; then echo "Press Enter to go ..." && read; fi

		echo "Now, let's delete 'key2' from this index. The plugin must show the del op coming"
		"$M0_SRC_DIR/utils/m0kv" "${MOTR_PARAM} "                                      \
					index del    "$DIX_FID" "key2"                     \
				 || {
			rc=$?
			echo "m0kv index del failed"
		}
		if $interactive ; then echo "Press Enter to go ..." && read; fi

		echo "Now, let's get 'key2' from this index again. It should fail."
		"$M0_SRC_DIR/utils/m0kv" "${MOTR_PARAM}"                                       \
					index get    "$DIX_FID" "key2"                     \
				 && {
			rc=22 # EINVAL to indicate the test is failed
			echo "m0kv index get expected to fail, but did not."
		}
		if $interactive ; then echo "Press Enter to go ..." && read; fi

		sleep 1
		echo "Now, let's delete 'key2' from this index again."
		echo "It should fail, and the plugin must NOT show the del op coming."
		"$M0_SRC_DIR/utils/m0kv" "${MOTR_PARAM}"                                       \
                                       index del    "$DIX_FID" "key2"                     \
                    && {
                    rc=22 # EINVAL to indicate the test is failed
                    echo "m0kv index del should fail, but did not"
		}
		if $interactive ; then echo "Press Enter to go ..." && read; fi


		echo "A second time, kill the two fdmi plug app"
		echo "This is to simulate the plugin failure"
		stop_fdmi_plugin
		sleep 5
		echo "Because the plugins failed, these operations "
		echo "will trigger failure handling"
		for ((j=0; j<4; j++)); do
			echo "a second time j=$j"
			"$M0_SRC_DIR/utils/m0kv" "${MOTR_PARAM}"                                       \
					index put    "$DIX_FID" "iter-äää-2-$j" "something1_anotherstring2*YETanotherstring3-$j"
		done
		if $interactive ; then echo "Press Enter to go ..." && read; fi

		echo "Start plugins again ..."
		rc=1
		start_fdmi_plugin "$FDMI_FILTER_FID"  "$FDMI_PLUGIN_EP"  &&
		start_fdmi_plugin "$FDMI_FILTER_FID2" "$FDMI_PLUGIN_EP2" && rc=0
		if [[ $rc -eq 1 ]] ; then
			echo "Can not stop and start plug again".
			return $rc
		fi
		sleep 5
		rc=0

		echo "Because the plugin started, these operations "
		echo "will work as normal"
		for ((j=0; j<4; j++)); do
			echo "back j=$j"
			"$M0_SRC_DIR/utils/m0kv" "${MOTR_PARAM}"                                       \
					index put    "$DIX_FID" "iter-äää-3-$j" "something1_anotherstring2*YETanotherstring3-$j"
		done
		if $interactive ; then echo "Press Enter to go ..." && read; fi

	done
	return $rc
}

start_fdmi_plugin()
{
	local fdmi_filter_fid=$1
	local fdmi_plugin_ep=$2
	local fdmi_output_file="${fdmi_filter_fid}.txt"
	local rc=0

	# Using `fdmi_sample_plugin`, which has duplicated records.
	# PLUG_PARAM="-l ${lnet_nid}:$fdmi_plugin_ep        \
	#	    -h ${lnet_nid}:$HA_EP -p $PROF_OPT    \
	#	    -f $M0T1FS_PROC_ID -g $fdmi_filter_fid -s "
	# if $interactive ; then PLUG_PARAM="$PLUG_PARAM -r"; fi
	# PLUGIN_CMD="$M0_SRC_DIR/fdmi/plugins/fdmi_sample_plugin $PLUG_PARAM"

	# Using `fdmi_app`, which can de-dup the duplicated records
	 APP_PARAM="-le ${lnet_nid}:$fdmi_plugin_ep        \
		    -he ${lnet_nid}:$HA_EP -pf $PROF_OPT    \
		    -sf $M0T1FS_PROC_ID                     \
		    -fi $fdmi_filter_fid                    \
		    --plugin-path $M0_SRC_DIR/fdmi/plugins/fdmi_sample_plugin "
	# To test with kafka server, uncomment following with valid kafka server
	# APP_PARAM="$APP_PARAM -ks 127.0.0.1:9092"
	PLUGIN_CMD="$M0_SRC_DIR/fdmi/plugins/fdmi_app $APP_PARAM"

	if $interactive ; then
		echo "Please use another terminal and run this command:"
		echo sudo "${PLUGIN_CMD}"
		echo "Then switch back to this terminal and press ENTER"
		read
	else
		echo "Please check fdmi plugin output from this file: $(pwd)/${fdmi_output_file}"
		(${PLUGIN_CMD} 2>&1 |& tee -a "${fdmi_output_file}" ) &
		sleep 5
	fi

	# Checking for the pid of the started plugin process
	local pid=$(pgrep -f "lt-fdmi_sample_plugin")
	if test "x$pid" = x; then
		echo "Failed to start FDMI plugin"
		rc=22 # EINVAL to indidate plugin start is failed
	fi
	return $rc
}


stop_fdmi_plugin()
{
	if $interactive ; then
		echo "Please switch to the plugin terminals and press Ctrl+C "
		echo "When both plugin applications are terminated, come back"
		echo "Please press Enter to continue ..." && read
	else
		local pids=$(pgrep -f "lt-fdmi_sample_plugin")
		if test "x$pids" != x; then
			for pid in $pids; do
				echo "Terminating ${pid}"
				kill -KILL "${pid}"
			done
		fi
		echo "The output of $FDMI_FILTER_FID  filter are:"
		echo "==================>>>======================"
		cat "${FDMI_FILTER_FID}.txt"
		echo "==================<<<======================"
		echo "The output of $FDMI_FILTER_FID2 filter are:"
		echo "==================>>>======================"
		cat "${FDMI_FILTER_FID2}.txt"
		echo "==================<<<======================"
	fi

	return 0
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
	echo "HA addr        : ${lnet_nid}:$HA_EP           "
	echo "Client addr    : ${lnet_nid}:$SNS_MOTR_CLI_EP "
	echo "Profile fid    : $PROF_OPT                    "
	echo "Process fid    : $M0T1FS_PROC_ID              "
	echo "FDMI plugin ep : ${lnet_nid}:$FDMI_PLUGIN_EP  "
	echo "FDMI filter fid: $FDMI_FILTER_FID             "
	echo "FDMI plugin ep : ${lnet_nid}:$FDMI_PLUGIN_EP2 "
	echo "FDMI filter fid: $FDMI_FILTER_FID2            "
	echo
	echo

	start_fdmi_plugin "$FDMI_FILTER_FID"  "$FDMI_PLUGIN_EP"  &&
	start_fdmi_plugin "$FDMI_FILTER_FID2" "$FDMI_PLUGIN_EP2" && {
	    do_some_kv_operations || {
		# Make the rc available for the caller and fail the test
		# if kv operations fail.
		rc=$?
		echo "Test failed with error $rc"
	    }
	}

	# wait_and_exit

	sleep 3
	stop_fdmi_plugin
	return $rc
}

parse_cli_options()
{
    while true ; do
        case "$1" in
            -i|--interactive)  interactive=true; shift ;;

            *)             break ;;
        esac
    done
}

parse_cli_options "$@"

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
		echo "FDMI plugin test failed."
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
