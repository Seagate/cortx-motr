#!/usr/bin/env python3

import subprocess
import os
import sys
import getopt
import json
import time
from socket import gethostname
from typing import Dict, List, NamedTuple, Set
import re
from datetime import datetime
from collections import OrderedDict
lnet_info = {}

sys.path.append(os.path.join(os.path.dirname(__file__), "..", ".."))
from cortx.utils.schema.payload import Json
from cortx.utils.conf_store import ConfStore

LNET_CONF_FILE = "/etc/modprobe.d/lnet.conf"
TIMEOUT = 180

class motr_prov:
    def __init__(self, index, url):
        self.conf_store = ConfStore()
        self.url = url
        self.index = index
        self.load_config(self.index, self.url)
        self.server_id = int(self.conf_store.get(self.index, 'cluster>current>server_id', default_val=None))

    def configure_lnet_from_conf_store(self):
        '''
           Get iface and /etc/modprobe.d/lnet.conf params from
           conf store. Configure lnet. Start lnet service
        '''
        lnet_conf = self.conf_store.get(self.index, f'cluster>server[{self.server_id}]>network>motr_net', default_val=None)
        with open(LNET_CONF_FILE, "w") as fp:
            fp.write(f"options lnet networks={lnet_conf['interface_type']}({lnet_conf['interface']})  config_on_load=1  lnet_peer_discovery_disabled=1\n")
        time.sleep(10)
        self.start_services(["lnet"])

    def execute_command(self, cmd, timeout_secs):
        
        ps = subprocess.Popen(cmd, stdin=subprocess.PIPE,
                              stdout=subprocess.PIPE, stderr=subprocess.STDOUT,
                              shell=True)
        stdout, stderr = ps.communicate(timeout=timeout_secs);
        stdout = str(stdout, 'utf-8')
        sys.stdout.write(f"[CMD] {cmd}\n")
        sys.stdout.write(f"[OUT]\n{stdout}\n")
        sys.stdout.write(f"[RET] {ps.returncode}\n")
        if ps.returncode != 0:
            sys.exit(ps.returncode)
        else:
            return stdout
                 
    def start_services(self, services):
        for service in services:
            cmd = "service {} start".format(service)
            sys.stdout.write("Executing cmd = {}\n".format(cmd))
            self.execute_command(cmd, TIMEOUT)
            time.sleep(10)
            cmd = "service {} status".format(service)
            sys.stdout.write("Executing cmd = {}\n".format(cmd))
            self.execute_command(cmd, TIMEOUT)
            time.sleep(10)

    def load_config(self, index, backend_url):
        """Instantiate and Load Config into constore"""
        self.conf_store.load(index, backend_url)
        return self.conf_store

    def create_lvm(self, node_name, metadata_dev, is_physical):

        self.validate_file(metadata_dev)

        cmd = f"fdisk -l {metadata_dev}"
        op = self.execute_command(cmd, TIMEOUT)

        cmd = f"swapoff -a"
        self.execute_command(cmd, TIMEOUT)

        cmd = f"pvcreate {metadata_dev}"
        self.execute_command(cmd, TIMEOUT)

        cmd = f"vgcreate  vg_metadata_{node_name} {metadata_dev}"
        self.execute_command(cmd, TIMEOUT)

        cmd = f"vgchange --addtag {node_name} vg_metadata_{node_name}"
        self.execute_command(cmd, TIMEOUT)

        cmd = f"vgscan --cache"
        self.execute_command(cmd, TIMEOUT)

        cmd = f"lvcreate -n lv_main_swap vg_metadata_{node_name} -l 51%VG"
        self.execute_command(cmd, TIMEOUT)

        cmd = f"lvcreate -n lv_raw_metadata vg_metadata_{node_name} -l 100%FREE"
        self.execute_command(cmd, TIMEOUT)

        cmd = f"mkswap -f /dev/vg_metadata_{node_name}/lv_main_swap"
        self.execute_command(cmd, TIMEOUT)

        cmd = f"test -e /dev/vg_metadata_{node_name}/lv_main_swap"
        self.execute_command(cmd, TIMEOUT)

        cmd = f"swapon /dev/vg_metadata_{node_name}/lv_main_swap"
        self.execute_command(cmd, TIMEOUT)

        cmd = (
            f"echo \"/dev/vg_metadata_{node_name}/lv_main_swap    swap    "
            f"swap    defaults        0 0\" >> /etc/fstab"
        )
        self.execute_command(cmd, TIMEOUT)


    def config_lvm(self):
        node_name = self.conf_store.get(self.index, f'cluster>server[{self.server_id}]>hostname', default_val=None)
        node_name = node_name.split('.')[0]
        metadata_device = self.conf_store.get(self.index,
        f'cluster>server[{self.server_id}]>storage>metadata_devices', default_val=None)
        is_physical = True if self.conf_store.get(self.index, f'cluster>server[{self.server_id}]>node_type', default_val=None) == "HW" else False

        self.create_lvm(node_name, metadata_device[0], is_physical)

    def config_motr(self):
        is_physical = True if self.conf_store.get(self.index, f'cluster>server[{self.server_id}]>node_type', default_val=None) == "HW" else False
        if is_physical:
            cmd = f"/opt/seagate/cortx/motr/libexec/motr_cfg.sh"
            self.execute_command(cmd, TIMEOUT)

    def validate_motr_rpm(self):
        cmd = f"uname -r"
        op = self.execute_command(cmd, TIMEOUT)
        kernel_ver = op.replace('\n', '')

        self.validate_file(f"/lib/modules/{kernel_ver}/kernel/fs/motr/m0tr.ko")

        self.validate_file("/etc/sysconfig/motr")

    def validate_file(self, file):
        if not os.path.exists(file):
            sys.stderr.write(f"[ERR] {file} does not exist.\n")
            sys.exit(1)
        else:
            sys.stdout.write(f"[MSG] {file} exists.\n")

    def test_lnet(self):
        missing_pkgs = []
        LNET_CONF_FILE="/etc/modprobe.d/lnet.conf"
        search_lnet_pkgs = ["kmod-lustre-client", "lustre-client"]

        # Check missing luster packages
        cmd = "rpm -qa | grep lustre"
        temp =  self.execute_command(cmd, TIMEOUT)
        lustre_pkgs = list(filter(None, temp.split("\n")))
        for pkg in search_lnet_pkgs:
            found = False
            for lustre_pkg in lustre_pkgs:
                if pkg in lustre_pkg:
                    found = True
                    break;
            if found == False:
                missing_pkgs.append(pkg)
        if missing_pkgs:
            sys.stderr.write("Missing pkgs ={}\n".format(missing_pkgs))

        # Check for lnet config file
        if os.path.exists(LNET_CONF_FILE):
            with open(LNET_CONF_FILE) as fp:
                line = fp.readline()
                while line:
                    tokens = line.split(' ')
                    # Get lnet iface
                    cmd = 'echo \'{}\' | cut -d "(" -f2 | cut -d ")" -f1'.format(tokens[2])
                    device = self.execute_command(cmd, TIMEOUT)
                    device = device.strip('\n')
                    sys.stdout.write("iface:{}\n".format(device))

                    # Get ip of iface
                    cmd = "ifconfig {} | awk \'/inet /\'".format(device)
                    ipconfig_op = self.execute_command(cmd, TIMEOUT)
                    ip = list(ipconfig_op.split())[1]
                    sys.stdout.write("ip = {}\n".format(ip))

                    # Ping ip
                    cmd = "ping -c 3 {}".format(ip)
                    op = self.execute_command(cmd, TIMEOUT)
                    sys.stdout.write("{}\n".format(op))
                    line = fp.readline()
