#!/usr/bin/env python3
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
import sys
import errno
import os
import re
import subprocess
from cortx.utils.conf_store import Conf

MOTR_SYS_FILE = "/etc/sysconfig/motr"
MOTR_CONFIG_SCRIPT = "/opt/seagate/cortx/motr/libexec/motr_cfg.sh"
LNET_CONF_FILE = "/etc/modprobe.d/lnet.conf"
SYS_CLASS_NET_DIR = "/sys/class/net/"
MOTR_SYS_CFG = "/etc/sysconfig/motr"
TIMEOUT_SECS = 120
MACHINE_ID_LEN = 32

class MotrError(Exception):
    """ Generic Exception with error code and output """

    def __init__(self, rc, message, *args):
        self._rc = rc
        self._desc = message % (args)

    def __str__(self):
        return f"error[{self._rc}]: {self._desc}"


def execute_command(self, cmd, timeout_secs = TIMEOUT_SECS):

    ps = subprocess.Popen(cmd, stdin=subprocess.PIPE,
                          stdout=subprocess.PIPE, stderr=subprocess.STDOUT,
                          shell=True)
    stdout, stderr = ps.communicate(timeout=timeout_secs);
    stdout = str(stdout, 'utf-8')
    if self._debug:
        sys.stdout.write(f"[CMD] {cmd}\n")
        sys.stdout.write(f"[OUT]\n{stdout}\n")
        sys.stdout.write(f"[RET] {ps.returncode}\n")
    if ps.returncode != 0:
        raise MotrError(ps.returncode, f"\"{cmd}\" command execution failed")
    return stdout, ps.returncode

def check_type(var, type, msg):
    if not isinstance(var, type):
        raise MotrError(errno.EINVAL, f"Invalid {msg} type. Expected: {type}")


def get_current_node(self):
    cmd = "cat /etc/machine-id"
    machine_id = execute_command(self, cmd)
    machine_id = machine_id[0].split('\n')[0]

    check_type(machine_id, str, "machine-id")
    if len(machine_id) != MACHINE_ID_LEN:
        raise MotrError(errno.EINVAL, "Invalid machine-id length."
                        f" Expected: {MACHINE_ID_LEN}"
                        f" Actual: {len(machine_id)}")

    try:
        current_node = Conf.get(self._index, 'cluster>server_nodes')[machine_id]
    except:
        raise MotrError(errno.EINVAL, "Current node not found")

    check_type(current_node, str, "current node")
    return current_node


def restart_services(self, services):
    for service in services:
        sys.stdout.write(f"Restarting {service} service\n")
        cmd = f"systemctl stop {service}"
        execute_command(self, cmd)
        cmd = f"systemctl start {service}"
        execute_command(self, cmd)
        cmd = f"systemctl status {service}"
        execute_command(self, cmd)

def validate_file(file):
    if not os.path.exists(file):
        raise MotrError(errno.ENOENT, "{} not exist".format(file))

def is_hw_node(self):
    try:
        node_type = Conf.get(self._index,
                    f'cluster>{self._server_id}')['node_type']
    except:
        raise MotrError(errno.EINVAL, "node_type not found")
    check_type(node_type, str, "node type")
    if node_type == "HW":
        return True
    else:
        return False

def validate_motr_rpm(self):
    cmd = "uname -r"
    cmd_res = execute_command(self, cmd)
    op = cmd_res[0]
    kernel_ver = op.replace('\n', '')
    check_type(kernel_ver, str, "kernel version")

    kernel_module = f"/lib/modules/{kernel_ver}/kernel/fs/motr/m0tr.ko"
    sys.stdout.write(f"Checking for {kernel_module}\n")
    validate_file(kernel_module)

    sys.stdout.write(f"Checking for {MOTR_SYS_FILE}\n")
    validate_file(MOTR_SYS_FILE)


def motr_config(self):
    is_hw = is_hw_node(self)
    if is_hw:
        execute_command(self, MOTR_CONFIG_SCRIPT)

def configure_net(self):
    """Wrapper function to detect lnet/libfabric transport"""
    try:
        transport_type = Conf.get(self._index,
            f'cluster>{self._server_id}')['network']['data']['transport_type']
    except:
        raise MotrError(errno.EINVAL, "transport_type not found")
    check_type(transport_type, str, "transport_type")

    if transport_type == "lnet":
        configure_lnet(self)
    elif transport_type == "libfabric":
        configure_libfabric(self)
    else:
        raise MotrError(errno.EINVAL, "Unknown data transport type\n")

def configure_lnet(self):
    '''
       Get iface and /etc/modprobe.d/lnet.conf params from
       conf store. Configure lnet. Start lnet service
    '''
    try:
        iface = Conf.get(self._index,
        f'cluster>{self._server_id}')['network']['data']['private_interfaces']
        iface = iface[0]
    except:
        raise MotrError(errno.EINVAL, "private_interfaces[0] not found\n")

    try:
        iface_type = Conf.get(self._index,
            f'cluster>{self._server_id}')['network']['data']['interface_type']
    except:
        raise MotrError(errno.EINVAL, "interface_type not found\n")

    lnet_config = (f"options lnet networks={iface_type}({iface}) "
                  f"config_on_load=1  lnet_peer_discovery_disabled=1\n")
    sys.stdout.write(f"lnet config: {lnet_config}")

    with open(LNET_CONF_FILE, "w") as fp:
        fp.write(lnet_config)

    restart_services(self, ["lnet"])

def configure_libfabric(self):
    raise MotrError(errno.EINVAL, "libfabric not implemented\n")

def create_lvm(node_name, index, metadata_dev):
    index = index + 1
    vg_name = f"vg_{node_name}_md{index}"
    lv_swap_name = f"lv_main_swap{index}"
    lv_md_name = f"lv_raw_md{index}"
    try:
        validate_file(metadata_dev)

        cmd = f"fdisk -l {metadata_dev}"
        execute_command(self, cmd)

        cmd = f"wipefs --all --force {metadata_dev}"
        execute_command(self, cmd)

        cmd = f"pvcreate {metadata_dev}"
        execute_command(self, cmd)

        cmd = f"vgcreate {vg_name} {metadata_dev}"
        execute_command(self, cmd)

        cmd = f"vgchange --addtag {node_name} {vg_name}"
        execute_command(self, cmd)

        cmd = "vgscan --cache"
        execute_command(self, cmd)

        cmd = f"lvcreate -n {lv_swap_name} {vg_name} -l 51%VG"
        execute_command(self, cmd)

        cmd = f"lvcreate -n {lv_md_name} {vg_name} -l 100%FREE"
        execute_command(self, cmd)

        cmd = f"mkswap -f /dev/{vg_name}/{lv_swap_name}"
        execute_command(self, cmd)

        cmd = f"test -e /dev/{vg_name}/{lv_swap_name}"
        execute_command(self, cmd)

        cmd = (
           f"echo \"/dev/{vg_name}/{lv_swap_name}    swap    "
           f"swap    defaults        0 0\" >> /etc/fstab"
        )
        execute_command(self, cmd)
    except:
        pass

def config_lvm(self):
    try:
        metadata_devices = Conf.get(self._index,
                f'cluster>{self._server_id}')['storage']['metadata_devices']
    except:
        raise MotrError(errno.EINVAL, "metadata_devices not found\n")

    sys.stdout.write(f"lvm: metadata_devices={metadata_devices}\n")

    cmd = "swapoff -a"
    execute_command(self, cmd)

    for device in metadata_devices:
        create_lvm(self._server_id, metadata_devices.index(device), device)

    cmd = "swapon -a"
    execute_command(self, cmd)

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
        raise MotrError(errno.EINVAL, f"Cant parse {LNET_CONF_FILE}")

    if lnet_xface == None:
        raise MotrError(errno.EINVAL,
                        f"Cant obtain iface details from {LNET_CONF_FILE}")
    if lnet_xface not in os.listdir(SYS_CLASS_NET_DIR):
        raise MotrError(errno.EINVAL,
                        f"Invalid iface {lnet_xface} in lnet.conf")
    return lnet_xface

def check_pkgs(self, pkgs):
    for pkg in pkgs:
        cmd = f"rpm -q {pkg}"
        cmd_res = execute_command(self, cmd)
        ret = cmd_res[1]
        if ret == 0:
            sys.stdout.write(f"rpm found: {pkg}\n")
        else:
            raise MotrError(errno.ENOENT, f"Missing rpm: {pkg}")

def test_lnet(self):
    search_lnet_pkgs = ["kmod-lustre-client", "lustre-client"]
    check_pkgs(self, search_lnet_pkgs)

    lnet_xface = get_lnet_xface()
    cmd = f"ip addr show {lnet_xface}"
    cmd_res = execute_command(self, cmd)
    ip_addr = cmd_res[0]

    try:
        ip_addr = ip_addr.split("inet ")[1].split("/")[0]
    except:
        raise MotrError(errno.EINVAL, f"Cant parse {lnet_xface} ip addr")

    cmd = f"ping -c 3 {ip_addr}"
    execute_command(self, cmd)



