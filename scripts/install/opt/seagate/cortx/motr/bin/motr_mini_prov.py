#!/usr/bin/env python3
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
import sys
import errno
import argparse
import inspect
import traceback
import os
import re
import subprocess
import time
from cortx.utils.process import SimpleProcess
from cortx.utils.conf_store import Conf

MOTR_KERNEL_FILE = "/lib/modules/{kernel_ver}/kernel/fs/motr/m0tr.ko"
MOTR_SYS_FILE = "/etc/sysconfig/motr"
MOTR_CONFIG_SCRIPT = "/opt/seagate/cortx/motr/libexec/motr_cfg.sh"
LNET_CONF_FILE = "/etc/modprobe.d/lnet.conf"
SYS_CLASS_NET_DIR = "/sys/class/net/"
SLEEP_SECS = 2
TIMEOUT_SECS = 120

class MotrError(Exception):
    """ Generic Exception with error code and output """

    def __init__(self, rc, message, *args):
        self._rc = rc
        self._desc = message % (args)
        sys.stderr.write("error(%d): %s\n" %(self._rc, self._desc))

    def __str__(self):
        if self._rc == 0: return self._desc
        return "error(%d): %s" %(self._rc, self._desc)


def execute_command(cmd, timeout_secs):
        
    ps = subprocess.Popen(cmd, stdin=subprocess.PIPE,
                          stdout=subprocess.PIPE, stderr=subprocess.STDOUT,
                          shell=True)
    stdout, stderr = ps.communicate(timeout=timeout_secs);
    stdout = str(stdout, 'utf-8')
    sys.stdout.write(f"[CMD] {cmd}\n")
    sys.stdout.write(f"[OUT]\n{stdout}\n")
    sys.stdout.write(f"[RET] {ps.returncode}\n")
    return stdout, ps.returncode

def start_services(services):
    for service in services:
        cmd = "service {} start".format(service)
        execute_command(cmd, TIMEOUT_SECS)
        cmd = "service {} status".format(service)
        execute_command(cmd, TIMEOUT_SECS)

def validate_file(file):
    if not os.path.exists(file):
        raise MotrError(errno.ENOENT, "{} not exist".format(file))

def validate_motr_rpm(self):
    try:
        cmd = f"uname -r"
        cmd_res = execute_command(cmd, TIMEOUT_SECS)
        op = cmd_res[0]
        kernel_ver = op.replace('\n', '')
        kernel_module = f"/lib/modules/{kernel_ver}/kernel/fs/motr/m0tr.ko"
        sys.stdout.write(f"[INFO] Checking for {kernel_module}\n")
        validate_file(kernel_module)
        sys.stdout.write(f"[INFO] Checking for {MOTR_SYS_FILE}\n")
        validate_file(MOTR_SYS_FILE)
    except MotrError as e:
        pass

def motr_config(self):
    server_id = int(Conf.get(self._index, 'cluster>current>server_id'))
    node_type = Conf.get(self._index, f'cluster>server')[server_id]['node_type']
    is_physical = True if node_type == "HW" else False
    sys.stdout.write(f"[INFO] server_id={server_id} node_type={node_type} is_physical={is_physical}\n")
    if is_physical:
        execute_command(MOTR_CONFIG_SCRIPT, TIMEOUT_SECS)

def configure_net(self):
     server_id = int(Conf.get(self._index, 'cluster>current>server_id'))
     xport = Conf.get(self._index,
                          f'cluster>server')[server_id]['network']['motr_net']['transport']
     sys.stdout.write(f"[INFO] transport type = {xport}\n")
     if (xport == "lnet"):
        configure_lnet_from_conf_store(self)

def configure_lnet_from_conf_store(self):
    '''
       Get iface and /etc/modprobe.d/lnet.conf params from
       conf store. Configure lnet. Start lnet service
    '''
    server_id = int(Conf.get(self._index, 'cluster>current>server_id'))
    lnet_conf = Conf.get(self._index,
                         f'cluster>server')[server_id]['network']['motr_net']
    iface_type = lnet_conf['interface_type']
    iface = lnet_conf['interface']
    sys.stdout.write(f"[INFO] {iface_type}=({iface})") 
    sys.stdout.write(f"[INFO] Updating {LNET_CONF_FILE}")
    with open(LNET_CONF_FILE, "w") as fp:
        fp.write(f"options lnet networks={iface_type}({iface}) config_on_load=1  lnet_peer_discovery_disabled=1\n")
        time.sleep(SLEEP_SECS)
        start_services(["lnet"])


def create_lvm(node_name, metadata_dev, is_physical):
    try:
        validate_file(metadata_dev)

        cmd = f"fdisk -l {metadata_dev}"
        execute_command(cmd, TIMEOUT_SECS)

        cmd = f"swapoff -a"
        execute_command(cmd, TIMEOUT_SECS)

        cmd = f"pvcreate {metadata_dev}"
        execute_command(cmd, TIMEOUT_SECS)

        cmd = f"vgcreate  vg_metadata_{node_name} {metadata_dev}"
        execute_command(cmd, TIMEOUT_SECS)

        cmd = f"vgchange --addtag {node_name} vg_metadata_{node_name}"
        execute_command(cmd, TIMEOUT_SECS)

        cmd = f"vgscan --cache"
        execute_command(cmd, TIMEOUT_SECS)

        cmd = f"lvcreate -n lv_main_swap vg_metadata_{node_name} -l 51%VG"
        execute_command(cmd, TIMEOUT_SECS)

        cmd = f"lvcreate -n lv_raw_metadata vg_metadata_{node_name} -l 100%FREE"
        execute_command(cmd, TIMEOUT_SECS)

        cmd = f"mkswap -f /dev/vg_metadata_{node_name}/lv_main_swap"
        execute_command(cmd, TIMEOUT_SECS)

        cmd = f"test -e /dev/vg_metadata_{node_name}/lv_main_swap"
        execute_command(cmd, TIMEOUT_SECS)

        cmd = f"swapon /dev/vg_metadata_{node_name}/lv_main_swap"
        execute_command(cmd, TIMEOUT_SECS)

        cmd = (
           f"echo \"/dev/vg_metadata_{node_name}/lv_main_swap    swap    "
           f"swap    defaults        0 0\" >> /etc/fstab"
        )
        execute_command(cmd, TIMEOUT_SECS)
    except:
        pass

def config_lvm(self):
    server_id = int(Conf.get(self._index, 'cluster>current>server_id'))
    node_name = Conf.get(self._index, f'cluster>server')[server_id]['hostname']
    node_name = node_name.split('.')[0]
    metadata_device = Conf.get(self._index, f'cluster>server')[server_id]['storage']['metadata_devices']
    node_type = Conf.get(self._index, f'cluster>server')[server_id]['node_type']
    sys.stdout.write(f"[INFO] server_id={server_id} node_name={node_name} node_type={node_type} metadata_device={metadata_device}\n")
    is_physical = True if node_type == "HW" else False
    create_lvm(node_name, metadata_device[0], is_physical)

def get_lnet_xface() -> str:
    lnet_xface = None
    try:
        with open(LNET_CONF_FILE, 'r') as f:
            # Obtain interface name
            for line in f.readlines():
                if len(line.strip()) <= 0: continue
                tokens = re.split(r'\W+', line)
                if len(tokens) > 4:
                    lnet_xface = tokens[4]
                    break
    except:
        pass

    if lnet_xface == None:
        raise MotrError(errno.EINVAL, "Cant obtain iface details from %s", LNET_CONF_FILE)
    if lnet_xface not in os.listdir(SYS_CLASS_NET_DIR):
        raise MotrError(errno.EINVAL, "Invalid iface %s in lnet.conf", lnet_xface)

    return lnet_xface

def check_pkgs(src_pkgs, dest_pkgs):
    missing_pkgs = []
    for src_pkg in src_pkgs:
        found = False
        for dest_pkg in dest_pkgs:
            if src_pkg in dest_pkg:
                found = True
                break
        if found == False:
            missing_pkgs.append(src_pkg)
    if missing_pkgs:
        raise MotrError(errno.ENOENT, f'Missing pkgs: {missing_pkgs}')

def test_lnet(self):
    search_lnet_pkgs = ["kmod-lustre-client", "lustre-client"]

    try:
        # Check missing luster packages
        cmd = f'rpm -qa | grep lustre'
        cmd_res = execute_command(cmd, TIMEOUT_SECS)
        temp = cmd_res[0]
        lustre_pkgs = list(filter(None, temp.split("\n")))
        check_pkgs(search_lnet_pkgs, lustre_pkgs)

        lnet_xface = get_lnet_xface()
        ip_addr = os.popen(f'ip addr show {lnet_xface}').read().split("inet ")[1].split("/")[0]
        cmd = "ping -c 3 {}".format(ip_addr)
        cmd_res = execute_command(cmd, TIMEOUT_SECS)
        sys.stdout.write("{}\n".format(cmd_res[0]))
    except MotrError as e:
        pass
