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


[[ $UID -eq 0 ]] || {
    echo 'Please, run this script with "root" privileges.' >&2
    exit 1
}

SANDBOX_DIR=${SANDBOX_DIR:-/var/motr/sandbox.device-detach-st}
M0_SRC_DIR=`realpath $0`
M0_SRC_DIR=${M0_SRC_DIR%/*/*}

# Number of test iterations
ITER_NR=10

. "$M0_SRC_DIR"/utils/functions # report_and_exit

# install "motr" Python module required by m0spiel tool
cd "$M0_SRC_DIR"/utils/spiel
python3 setup.py install > /dev/null ||
    die 'Cannot install Python "motr" module'
cd "$M0_SRC_DIR"

echo "Installing Motr services"
scripts/install-motr-service -u
rm -rf /etc/motr
rm -f  /etc/sysconfig/motr
scripts/install-motr-service -l
utils/m0setup -v -P 3 -N 1 -K 1 -S 1 -i 1 -d /var/motr/img -s 8 -c
utils/m0setup -v -P 3 -N 1 -K 1 -S 1 -i 1 -d /var/motr/img -s 8

# update Motr configuration: set specific dir for test artifacts
sed -i "s@.*MOTR_LOG_DIR.*@MOTR_LOG_DIR=${SANDBOX_DIR}/log@" \
     /etc/sysconfig/motr
sed -i "s@.*MOTR_M0D_DATA_DIR.*@MOTR_M0D_DATA_DIR=${SANDBOX_DIR}/motr@" \
     /etc/sysconfig/motr

echo "Start Motr services"
systemctl start motr-mkfs
systemctl start motr-singlenode
sleep 10 # allow motr to finish its startup

echo "Perform device-detach test"
cd "$SANDBOX_DIR"

LNET_NID=`lctl list_nids | head -1`
SPIEL_ENDPOINT="$LNET_NID:12345:34:1021"
HA_ENDPOINT="$LNET_NID:12345:45:1"
M0_SPIEL_OPTS="-l $M0_SRC_DIR/motr/.libs/libmotr.so --client $SPIEL_ENDPOINT \
               --ha $HA_ENDPOINT"

function spiel_cmd {
    "$M0_SRC_DIR"/utils/spiel/m0spiel $M0_SPIEL_OPTS <<EOF
fids = {'profile'       : Fid(0x7000000000000001, 0),
        'disk0'         : Fid(0x6b00000000000001, 2)
}

if spiel.cmd_profile_set(str(fids['profile'])):
    sys.exit('cannot set profile {0}'.format(fids['profile']))

if spiel.rconfc_start():
    sys.exit('cannot start rconfc')

device_commands = [('$1', fids['disk0'])]
for command in device_commands:
    try:
        if getattr(spiel, command[0])(*command[1:]) != 0:
            sys.exit("an error occurred while {0} executing, device fid {1}"
                     .format(command[0], command[1]))
    except:
        spiel.rconfc_stop()
        sys.exit("an error occurred while {0} executing, device fid {1}"
                 .format(command[0], command[1]))

spiel.rconfc_stop()
EOF
    return $?
}

rc=0
for I in $(seq 1 $ITER_NR); do
    filename="/mnt/m0t1fs/1:$I"
    echo "Iteration $I of $ITER_NR (file: $filename)"
    touch "$filename" && setfattr -n lid -v 8 "$filename"
    rc=$?
    if [ $rc -ne 0 ]; then echo "Cannot create file"; break; fi
    echo "Start I/O"
    dd if=/dev/zero of="$filename" bs=1M count=10 >/dev/null 2>&1 &
    dd_pid=$!

    spiel_cmd device_detach
    rc=$?
    echo "device_detach finished with rc=$rc"

    wait $dd_pid
    echo "I/O finished with rc=$? (may fail)"

    if [ $rc -ne 0 ]; then break; fi

    spiel_cmd device_attach
    rc=$?
    echo "device_attach finished with rc=$rc"
    if [ $rc -ne 0 ]; then break; fi
done

echo "Tear down Motr services"
systemctl stop motr-singlenode
systemctl stop motr-kernel
motr_rc=$?
if [ $rc -eq 0 ]; then
    rc=$motr_rc
fi

cd "$M0_SRC_DIR"
scripts/install-motr-service -u
utils/m0setup -v -P 3 -N 1 -K 1 -S 1 -i 1 -d /var/motr/img -s 8 -c

if [ $rc -eq 0 ]; then
    rm -r "$SANDBOX_DIR"
fi

report_and_exit m0d-device-detach $rc
