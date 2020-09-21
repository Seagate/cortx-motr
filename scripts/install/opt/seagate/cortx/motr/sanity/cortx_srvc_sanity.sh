#!/usr/bin/env bash
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

# set -eu -o pipefail ## commands tend to fail unpredictably

## Declare variables

source "/opt/seagate/cortx/motr/common/cortx_error_codes.sh"
source "/opt/seagate/cortx/motr/common/cortx_util_funcs.sh"

RESULT=0
UUID_FILE_CREATED=$NO
MOTR_KERNEL="motr-kernel"
MOTR_KERNEL_SERVICE=$MOTR_KERNEL".service"
ETC_SYSCONFIG_MOTR="/etc/sysconfig/motr"

workflow()
{
cat << EOF

This script [$0] will take the following actions:
    1. Verify that the file [$ETC_SYSCONFIG_MOTR] exists.
    2. Find the status of [$MOTR_KERNEL_SERVICE].
    3. If [$MOTR_KERNEL_SERVICE] is up and running, the script will exit &
       return 0.
    4. If failed, the script will verify if the file
       [/etc/sysconfig/$MOTR_KERNEL] exists. If not, it will be
       created.
    5. Next it  will attempt to start the [$MOTR_KERNEL_SERVICE]. If
       failed, it will cleanup the $MOTR_KERNEL service states and
       delete the [/etc/sysconfig/$MOTR_KERNEL] file, it it was created,
       and exit with appropriate non-zero error code.
    6. If the [$MOTR_KERNEL_SERVICE] service was started successfully,
       then the service will be shut down, service states cleaned-up, and then
       delete the [/etc/sysconfig/$MOTR_KERNEL] file, it it was created,
       and exit & return 0.

EOF
}

systemctl_action()
{
    local RESULT=0
    CMD=$1
    SYSCTL_MOTR_STATUS=$($CMD)
    RESULT=$?
    if [ $RESULT == 0 ]; then
        msg "Motr executing [$CMD] succeeded."
    else
        err "Motr executing [$CMD] failed. RESULT [$RESULT]"
        msg "$SYSCTL_MOTR_STATUS"
    fi
    return $RESULT
}

create_temp_uuid_file()
{
    echo "MOTR_NODE_UUID='$(uuidgen --time)'" > /etc/sysconfig/$MOTR_KERNEL
    die_if_failed $? "Failed to create [/etc/sysconfig/$MOTR_KERNEL."

    msg "File [/etc/sysconfig/$MOTR_KERNEL] created."
    msg "[$(cat /etc/sysconfig/$MOTR_KERNEL)]"
    UUID_FILE_CREATED=$YES
}

delete_temp_uuid_file()
{
    if [ $UUID_FILE_CREATED == $YES ]; then
        rm /etc/sysconfig/$MOTR_KERNEL
        die_if_failed $? "Failed to rm file [/etc/sysconfig/$MOTR_KERNEL]."

        msg "File [/etc/sysconfig/$MOTR_KERNEL] deleted."
        UUID_FILE_CREATED=$NO
    else
        msg "This file [/etc/sysconfig/$MOTR_KERNEL] was never created by \
        this script."
    fi
}
## main
is_sudoed

workflow

if [ ! -f $ETC_SYSCONFIG_MOTR ]; then
    err "File Not found: [$ETC_SYSCONFIG_MOTR]"
    die $ERR_ETC_SYSCONFIG_MOTR_NOT_FOUND
fi

systemctl_action "systemctl status $MOTR_KERNEL"
RESULT=$?
if [ $RESULT == 0 ]; then
    msg "The $MOTR_KERNEL was found up and running."
    die $ERR_SUCCESS
fi
msg "Status of $MOTR_KERNEL could not be determined. Probing further."

if [ ! -f "/etc/sysconfig/$MOTR_KERNEL" ]; then
    msg "File Not found: [/etc/sysconfig/$MOTR_KERNEL]"
    ## we will create the UUID file here and later delete it
    create_temp_uuid_file
    ## we never return if we fail, so validation of result is not required
else
    msg "File [/etc/sysconfig/$MOTR_KERNEL] exists."
    if [ -z "/etc/sysconfig/$MOTR_KERNEL" ]; then
        msg "File [/etc/sysconfig/$MOTR_KERNEL] exists is empty."
        create_temp_uuid_file
        ## we never return if we fail, so validation of result is not required
    fi
fi

# find out if lnet is running even when
systemctl_action "systemctl status lnet"
LNET_STATUS=$?
systemctl_action "systemctl status motr-trace@kernel.service"
MOTR_TRACE_STATUS=$?

systemctl_action "systemctl start $MOTR_KERNEL"
RESULT=$?
if [ $RESULT == 0 ]; then
    systemctl_action "systemctl stop $MOTR_KERNEL"
    RESULT=$?
    if [ $RESULT == 0 ]; then
        msg "The $MOTR_KERNEL was started & stopped successfully."
        if [ $LNET_STATUS != 0 ]; then
            # lnet was not running before we started $MOTR_KERNEL
            # we will stop it down
            msg "Stopping lnet as well."
            systemctl_action "systemctl stop lnet"
        fi

        if [ $MOTR_TRACE_STATUS != 0 ]; then
            # motr-trace@kernel.service was not running before we started
            # $MOTR_KERNEL. we will stop it.
            msg "Stopping motr-trace@kernel.service as well."
            systemctl_action "systemctl stop lnet"
        fi

        delete_temp_uuid_file
        die $ERR_SUCCESS
    fi
fi

delete_temp_uuid_file
die $ERR_SRVC_MOTR_STATUS_FAILED
