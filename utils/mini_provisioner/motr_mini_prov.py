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

conf_store = ConfStore()

class motr_prov:
    def __init__(self, index, url):
        self.conf_store = ConfStore()
        self.url = url
        self.index = index
        self.load_config(self.index, self.url)
        self.server_id = int(self.conf_store.get(self.index, 'cluster>current>server_id', default_val=None))
        self.server_id = self.server_id - 1

    def configure_lnet_from_conf_store(self):
        '''
           Get iface and /etc/modprobe.d/lnet.conf params from
           conf store. Configure lnet. Start lnet service
        '''
        lnet_conf = self.conf_store.get(self.index, f'cluster>server[{self.server_id}]>network>motr_net', default_val=None)
        print("Atul in motr_mini_prov_class.py 37 lnet_conf={}".format(lnet_conf))
        fp = open("/etc/modprobe.d/lnet.conf", "w")
        fp.write(f"options lnet networks={lnet_conf['interface_type']}\({lnet_conf['interface']}\)  config_on_load=1  lnet_peer_discovery_disabled=1\n")
        fp.close()
        time.sleep(10)
        self.start_services(["lnet"])

    def execute_command(self, cmd, timeout_secs):
        ps = subprocess.Popen(cmd, stdin=subprocess.PIPE,
                              stdout=subprocess.PIPE, stderr=subprocess.STDOUT,
                              shell=True)
        stdout, stderr = ps.communicate(timeout=timeout_secs);
        stdout = str(stdout, 'utf-8')
        print(f"[CMD] {cmd}")
        print(f"[OUT]\n{stdout}")
        print(f"[RET] {ps.returncode}")
        if ps.returncode != 0:
            sys.exit(1)
        return stdout


    def start_services(self, services):
        for service in services:
            cmd = "service {} start".format(service)
            print("Executing cmd = {}".format(cmd))
            self.execute_command(cmd, 180)
            time.sleep(10)
            cmd = "service {} status".format(service)
            print("Executing cmd = {}".format(cmd))
            self.execute_command(cmd, 180)
            time.sleep(10)

    def load_config(self, index, backend_url):
        """Instantiate and Load Config into constore"""
        self.conf_store.load(index, backend_url)
        return self.conf_store

    def create_lvm(self, node_name, metadata_dev, is_physical):

        self.validate_file(metadata_dev)

        cmd = f"fdisk -l {metadata_dev}"
        op = self.execute_command(cmd, 180)

        cmd = f"swapoff -a"
        self.execute_command(cmd, 180)

        #cmd = f"parted {metadata_dev} mklabel gpt"
        #self.execute_command(cmd, 180)

        #end = 10
        #if is_physical:
        #     end = 1000
        #cmd = f"parted {metadata_dev} mkpart primary ext4 0% {end}GB"
        #self.execute_command(cmd, 180)

        #start = 11
        #if is_physical:
        #    start = 1001
        #cmd = f"parted {metadata_dev} mkpart primary ext2 {start}GB 100%"
        #self.execute_command(cmd, 180)

        #cmd = f"parted {metadata_dev} toggle 2 lvm"
        #self.execute_command(cmd, 180)

        cmd = f"pvcreate {metadata_dev}2"
        self.execute_command(cmd, 180)

        cmd = f"vgcreate  vg_metadata_{node_name} {metadata_dev}2"
        self.execute_command(cmd, 180)

        cmd = f"vgchange --addtag {node_name} vg_metadata_{node_name}"
        self.execute_command(cmd, 180)

        cmd = f"vgscan --cache"
        self.execute_command(cmd, 180)

        cmd = f"lvcreate -n lv_main_swap vg_metadata_{node_name} -l 51%VG"
        self.execute_command(cmd, 180)

        cmd = f"lvcreate -n lv_raw_metadata vg_metadata_{node_name} -l 100%FREE"
        self.execute_command(cmd, 180)

        cmd = f"mkswap -f /dev/vg_metadata_{node_name}/lv_main_swap"
        self.execute_command(cmd, 180)

        cmd = f"test -e /dev/vg_metadata_{node_name}/lv_main_swap"
        self.execute_command(cmd, 180)

        cmd = f"swapon /dev/vg_metadata_{node_name}/lv_main_swap"
        self.execute_command(cmd, 180)

        cmd = (
            f"echo \"/dev/vg_metadata_{node_name}/lv_main_swap    swap    "
            f"swap    defaults        0 0\" >> /etc/fstab"
        )
        self.execute_command(cmd, 180)

        cmd = f"mkfs.ext4 {metadata_dev}1 -L cortx_metadata"
        self.execute_command(cmd, 180)

        #cmd = f"blockdev --flushbufs /dev/disk/by-id/dm-name-mpath* || true"
        #execute_command(cmd)

        time.sleep(10)

        cmd = f"timeout -k 10 30 partprobe || true"
        self.execute_command(cmd, 180)

    def config_lvm(self):
        node_name = self.conf_store.get(self.index, f'cluster>server[{self.server_id}]>hostname', default_val=None)
        node_name = node_name.split('.')[0]
        metadata_device = self.conf_store.get(self.index,
        f'cluster>server[{self.server_id}]>storage>metadata_devices', default_val=None)
        is_physical = True if self.conf_store.get(self.index, f'cluster>server[{self.server_id}]>node_type', default_val=None) == "HW" else False

        self.create_lvm(node_name, metadata_device[0], is_physical)

    def config_motr(self):
        cmd = f"/opt/seagate/cortx/motr/libexec/motr_cfg.sh"
        self.execute_command(cmd, 180)

    def test_motr(self):
        cmd = f"uname -r"
        op = self.execute_command(cmd, 180)
        kernel_ver = op.replace('\n', '')

        self.validate_file(f"/lib/modules/{kernel_ver}/kernel/fs/motr/m0tr.ko")

        self.validate_file("/etc/sysconfig/motr")

    def validate_file(self, file):
        if not os.path.exists(file):
            print(f"[ERR] {file} does not exist.")
            sys.exit(1)
        else:
            print(f"[MSG] {file} exists.")

    def test_lnet(self):
        missing_pkgs = []
        LNET_CONF_FILE="/etc/modprobe.d/lnet.conf"
        search_lnet_pkgs = ["kmod-lustre-client", "lustre-client"]

        # Check missing luster packages
        cmd = "rpm -qa | grep lustre"
        temp =  self.execute_command(cmd, 180)
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
            print("Missing pkgs ={}".format(missing_pkgs))

        # Check for lnet config file
        if os.path.exists(LNET_CONF_FILE):
            with open(LNET_CONF_FILE) as fp:
                line = fp.readline()
                while line:
                    tokens = line.split(' ')
                    # Get lnet iface
                    cmd = 'echo \'{}\' | cut -d "(" -f2 | cut -d ")" -f1'.format(tokens[2])
                    device = self.execute_command(cmd, 180)
                    device = device.strip('\n')
                    print("iface:{}".format(device))

                    # Get ip of iface
                    cmd = "ifconfig {} | awk \'/inet /\'".format(device)
                    ipconfig_op = self.execute_command(cmd, 180)
                    ip = list(ipconfig_op.split())[1]
                    print("ip = {}".format(ip))

                    # Ping ip
                    cmd = "ping -c 3 {}".format(ip)
                    op = self.execute_command(cmd, 180)
                    print(op)
                    line = fp.readline()
            fp.close()
