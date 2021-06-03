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


# Script for starting or stopping Motr system tests

random_mode=0
tests=
debugger=
gdbparams=${GDBPARAMS:-}

# Get the location of this script and look for m0st and kernel
# module in known locations (should changed to a more robust way)
st_util_dir=$(readlink -f $0)
motr_src=$(echo $(dirname $st_util_dir) \
         | sed -r -e 's#/?motr/st/utils/?$##' -e 's#^/usr/s?bin##')

. $motr_src/m0t1fs/linux_kernel/st/m0t1fs_common_inc.sh
. $motr_src/m0t1fs/linux_kernel/st/m0t1fs_sns_common_inc.sh

# kernel mode
function motr_st_start_k ()
{
	motr_index_init "KVS"
	local st_kmod=$motr_src/motr_st_kmod.ko
	local st_kmod_args="local_addr=$MOTR_LOCAL_EP \
			    ha_addr=$MOTR_HA_EP \
			    prof=$MOTR_PROF_OPT \
			    proc_fid=$MOTR_PROC_FID"

	if [ X$tests != X ]; then
		st_kmod_args="$st_kmod_args motr_tests=$tests"
	fi

	if [ ! -f $st_kmod ];then
		echo "Can't find Motr ST kernel module"
		return 1
	fi

	# insmod kernel
	echo -e "insmod $st_kmod $st_kmod_args "
	insmod $st_kmod $st_kmod_args
	if [ $? -ne 0  ] ; then
		echo "Failed to load Motr system test kernel module"
		return 1
	fi

	return 0
}

function motr_st_stop_k()
{
	# when remove the kernel module, it will wait till
	# all tests finish
	echo "Stop Motr system tests ..."
	rmmod motr_st_kmod
}

motr_st_run_debugger()
{
    # Support gdb only currently
    local gdbinit=${motr_src:+-x ${motr_src}/.gdbinit}
    local binary=$2
    shift 2

    echo "gdb $gdbinit --args $binary $@"  >/dev/tty
    gdb $gdbinit $gdbparams --args $binary $@
}

function motr_index_init()
{
	local idx_service=-1
	case $1 in
		"KVS")
			idx_service=1
			;;
		"CASS")
			idx_service=2
			;;
	esac
	if [ $idx_service -eq 1 ]; then
		#create DIX metadata
		local m0dixinit="$motr_src/dix/utils/m0dixinit"
		local cmd

		local pverid=$(echo $DIX_PVERID | tr -d ^)

		if [ ! -f $m0dixinit ] ; then
			echo "Can't find m0dixinit"
			return 1
		fi

#		cmd="$m0dixinit -l $MOTR_LOCAL_EP -H $MOTR_HA_EP \
#		     -p '$MOTR_PROF_OPT' -I '$pverid' -d '$pverid' -a create"
#		echo $cmd
#		eval "$cmd"
		echo "*** m0dixinit is omitted. Mkfs creates meta indices now."

		#create indices for composite extents
		local m0composite="$motr_src/motr/st/utils/m0composite"
		local cmd

		if [ ! -f $m0composite ] ; then
			echo "Can't find m0composite"
			return 1
		fi

		cmd="$m0composite $MOTR_LOCAL_EP $MOTR_HA_EP \
		     $MOTR_PROF_OPT $MOTR_PROC_FID"
		echo $cmd
		eval "$cmd"


	fi
}

# user space mode
function motr_st_start_u()
{
	motr_index_init "KVS"
	idx_service=1
	# Debugger
	if [ X$2 != X ]; then
		debugger=$2
	fi

	# Assembly command
	local st_exec=
	if [ X$debugger != X ]; then
		st_exec="$motr_src/motr/st/user_space/.libs/lt-m0st"
	else
		st_exec="$motr_src/motr/st/user_space/m0st"
	fi
	if [ ! -f $st_exec ];then
		echo "Can't find $st_exec"
		return 1
	fi

	local st_args="-m $MOTR_LOCAL_EP -h $MOTR_HA_EP \
		       -p $MOTR_PROF_OPT -f $MOTR_PROC_FID \
		       -I $idx_service"
	if [ $random_mode -eq 1 ]; then
		st_args+=" -r"
	fi
	if [ -n "$tests" ]; then
		st_args+=" -t $tests"
	fi
	local st_u="$st_exec $st_args"


	# Run it
	if [ X$debugger != X ];then
		echo Running system tests in gdb...
		motr_st_run_debugger $debugger $st_u
	else
		echo Running system tests ...
		echo "# $st_u" >/dev/tty
		eval $st_u || {
			echo "Failed to run Motr ST !!"
			return 1
		}
	fi
	return 0
}

function motr_st_stop_u ()
{
	echo "Stop Motr system tests ..."
	pkill -INT -f m0st

	while [ `ps ax | grep -v grep | grep m0st` ]; do
		echo -n '.'
		sleep 2;
	done
}

function motr_st_list_tests ()
{
	# Assembly command
	local st_exec="$motr_src/motr/st/user_space/m0st"
	if [ ! -f $st_exec ];then
		echo "Can't find m0st"
		return 1
	fi

	local st_args="-l"
	local st_u="$st_exec $st_args"

	# Run it
	echo Running system tests ...
	echo "# $st_u" >/dev/tty
	eval $st_u || {
		echo "Failed to run Motr ST !!"
		return 1
	}

	return 0
}

# Read/Write an object via Motr
io_conduct()
{
	operation=$1
	source=$2
	dest=$3
	verify=$4

	local cmd_exec
	if [ $operation == "READ" ]
	then
		cmd_args="-l $MOTR_LOCAL_EP -H $MOTR_HA_EP \
                          -p '$MOTR_PROF_OPT' -P '$MOTR_PROC_FID' \
                          -o $source"
		cmd_exec="${motr_st_util_dir}/m0cat"
		cmd_args="$cmd_args -s $BLOCKSIZE -c $BLOCKCOUNT"

		if [[ $verify == "true" ]]; then
			cmd_args+=" -r"
		fi

		local cmd="$cmd_exec $cmd_args $dest &"
	else
		cmd_args="-l $MOTR_LOCAL_EP -H $MOTR_HA_EP \
                          -p '$MOTR_PROF_OPT' -P '$MOTR_PROC_FID' \
                          -o $dest $source"
		cmd_exec="${motr_st_util_dir}/m0cp"
		cmd_args="$cmd_args -s $BLOCKSIZE -c $BLOCKCOUNT"

		if [ $verify == "true" ]
		then
			cmd_args="$cmd_args -r"
		fi

		local cmd="$cmd_exec $cmd_args &"
	fi
	cwd=`pwd`
	cd $MOTR_TRACE_DIR

	eval $cmd
	motr_pids[$cnt]=$!
	wait ${motr_pids[$cnt]}
	if [ $? -ne 0 ]
	then
		echo "  Failed to run command $cmd_exec"
		cd $cwd
		return 1
	fi
	cnt=`expr $cnt + 1`
	cd $cwd
	return 0
}

function motr_st_disk_state_set()
{
	local service_eps=$(service_eps_get)

	service_eps+=($MOTR_HA_EP)

	ha_events_post "${service_eps[*]}" $@
}

function dix_init()
{
	local m0dixinit="$M0_SRC_DIR/dix/utils/m0dixinit"
	local pverid=$(echo $DIX_PVERID | tr -d ^)
	if [ ! -f $m0dixinit ] ; then
		echo "Can't find m0dixinit"
		return 1
	fi

#	cmd="$m0dixinit -l $MOTR_LOCAL_EP -H $MOTR_HA_EP \
#	    -p '$MOTR_PROF_OPT' -I '$pverid' -d '$pverid' -a create"
#	echo $cmd
#	eval "$cmd"
	echo "*** m0dixinit is omitted. Mkfs creates meta indices now."
#	if [ $? -ne 0 ]
#	then
#		echo "Failed to initialise kvs..."
#		return 1
#	fi
}

function dix_destroy()
{
	local m0dixinit="$M0_SRC_DIR/dix/utils/m0dixinit"
	local pverid=$(echo $DIX_PVERID | tr -d ^)
	if [ ! -f $m0dixinit ] ; then
		echo "Can't find m0dixinit"
		return 1
	fi

	cmd="$m0dixinit -l $MOTR_LOCAL_EP -H $MOTR_HA_EP \
	    -p '$MOTR_PROF_OPT' -I '$pverid' -d '$pverid' -a destroy"
	echo $cmd
	eval "$cmd"
	if [ $? -ne 0 ]
	then
		echo "Failed to destroy kvs..."
		return 1
	fi
}

motr_service_start()
{
	local n=$1
	local k=$2
	local p=$3
	local stride=$4
	local multiple_pools=0

	echo "n k p:$N $K $P"
	motr_service start $multiple_pools $stride $n $k $p
	if [ $? -ne 0 ]
	then
		echo "Failed to start Motr Service..."
		return 1
	fi
	echo "motr service started"

	ios_eps=""
	for ((i=0; i < ${#IOSEP[*]}; i++)) ; do
		ios_eps="$ios_eps -S ${lnet_nid}:${IOSEP[$i]}"
	done
	return 0
}

motr_service_stop()
{
	motr_service stop
	if [ $? -ne 0 ]
	then
		echo "Failed to stop Motr Service..."
		return 1
	fi
}

clean()
{
        multiple_pools=$1
	local i=0
	for ((i=0; i < ${#IOSEP[*]}; i++)) ; do
		# Removes the stob files created in stob domain since
		# there is no support for m0_stob_delete() and after
		# unmounting the client file system, from next mount,
		# fids are generated from same baseline which results
		# in failure of cob_create fops.
		local ios_index=`expr $i + 1`
		rm -rf $MOTR_TEST_DIR/d$ios_index/stobs/o/*
	done

        if [ ! -z "$multiple_pools" ] && [ $multiple_pools == 1 ]; then
		local ios_index=`expr $i + 1`
		rm -rf $MOTR_TEST_DIR/d$ios_index/stobs/o/*
        fi
}

motr_error_handling()
{
	rc=$1
	msg=$2
	clean 0 &>>$MOTR_TEST_LOGFILE
	motr_service_stop
	echo $msg
	echo "Test log file available at $MOTR_TEST_LOGFILE"
	echo "Motr trace files are available at: $MOTR_TRACE_DIR"
	exit $1
}

error_handling()
{
	unmount_and_clean &>>$MOTR_TEST_LOGFILE
	motr_service_stop
	echo "Test log file available at $MOTR_TEST_LOGFILE"
	echo "Motr trace files are available at: $MOTR_TRACE_DIR"
	exit $1
}

# Local variables:
# sh-basic-offset: 8
# sh-indentation: 8
# tab-width: 8
# End:
