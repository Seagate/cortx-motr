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

set -e

[[ $UID -eq 0 ]] || {
    echo 'Please, run this script with "root" privileges.' >&2
    exit 1
}

SANDBOX_DIR=${SANDBOX_DIR:-/var/motr/sandbox.signal-st}
M0_SRC_DIR=`realpath $0`
M0_SRC_DIR=${M0_SRC_DIR%/*/*}

. "$M0_SRC_DIR"/utils/functions # report_and_exit

cd "$M0_SRC_DIR"

scripts/install-motr-service -u
scripts/install-motr-service -l
utils/m0setup --no-cas -v -c
utils/m0setup --no-cas -v

# update Motr configuration: set specific dir for test artifacts
sed -i "s@.*MOTR_LOG_DIR.*@MOTR_LOG_DIR=${SANDBOX_DIR}/log@" \
     /etc/sysconfig/motr
sed -i "s@.*MOTR_M0D_DATA_DIR.*@MOTR_M0D_DATA_DIR=${SANDBOX_DIR}/motr@" \
     /etc/sysconfig/motr

test_for_signal()
{
    local sig=$1
    echo "------------------ Configuring Motr for $sig test ------------------"
    systemctl start motr-mkfs
    local cursor=$(journalctl --show-cursor -n0 | grep -e '-- cursor:' | sed -e 's/^-- cursor: //')
    systemctl start motr

    echo 'Waiting for ios1 to become active'
    while ! systemctl -q is-active motr-server@ios1 ; do
        sleep 1
    done

    while ! journalctl -c "$cursor" -l -u motr-server@ios1 | grep -q 'Press CTRL+C to quit'; do
        sleep 1
    done

    echo "Sending $sig to ios1"
    systemctl -s "$sig" --kill-who=main kill motr-server@ios1

    if [[ $sig == SIGUSR1 ]] ; then
        sleep 5
    else
        echo "Waiting for ios1 to stop"
        while systemctl -q is-active motr-server@ios1 ; do
            sleep 1
        done
    fi

    if journalctl -c "$cursor" -l -u motr-server@ios1 | grep -Eq 'got signal -?[0-9]+'
    then
        echo "Successfully handled $sig during Motr setup"

        if [[ $sig == SIGUSR1 ]] ; then
            if journalctl -c "$cursor" -l -u motr-server@ios1 | grep -q 'Restarting'
            then
                echo "Wait for Motr restart"
                while ! systemctl -q is-active motr-server@ios1 &&
                      ! systemctl -q is-failed motr-server@ios1
                do
                    sleep 1
                done
            else
                echo "Restarting Motr failed"
                return 1
            fi
        fi
    else
        echo "Failed to handle $sig during Motr setup"
        return 1
    fi

    echo "Stopping Motr"
    systemctl stop motr
    systemctl stop motr-kernel
    return 0
}

# Test for stop
test_for_signal SIGTERM
rc1=$?

# Test for restart
test_for_signal SIGUSR1
rc2=$?

rc=$((rc1 + rc2))
if [ $rc -eq 0 ]; then
    rm -r "$SANDBOX_DIR"
fi

report_and_exit m0d-signal $rc
