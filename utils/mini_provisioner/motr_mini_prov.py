#!/usr/bin/env python3

import subprocess
import os
import sys
import getopt
import json
import time
import netifaces
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
motr_url = 'json:///home/743120/mini_provisioner/motr_prov_conf.json'

class motr_prov:
    def __init__(self, index, url):
        self.conf_store = ConfStore()
        self.url = url
        self.index = index
        self.load_config(self.index, self.url)
        print("Atul in motr_mini_prov_class.py on 30 self.url={} self.index={}".format(self.url, self.index))
    def configure_lnet_from_conf_store(self):
        '''
           Get iface and /etc/modprobe.d/lnet.conf params from
           conf store. Configure lnet. Start lnet service
        '''
        lnet_conf = self.conf_store.get(self.index, 'motr>lnet', default_val=None)
        print("Atul in motr_mini_prov_class.py 37 lnet_conf={}".format(lnet_conf))
        fp = open("/etc/modprobe.d/lnet.conf", "w")
        fp.write("options lnet networks=tcp({})  config_on_load=1  lnet_peer_discovery_disabled=1\n".format(lnet_conf["iface"], lnet_conf["config_on_load"], lnet_conf["lnet_peer_discovery_disabled"]))
        fp.close()
        time.sleep(10)
        self.start_services(["lnet"])

    def execute_command(self, cmd, timeout_secs):
        ps = subprocess.Popen(cmd, stdin=subprocess.PIPE,
                              stdout=subprocess.PIPE, stderr=subprocess.STDOUT,
                              shell=True)
        stdout, stderr = ps.communicate(timeout=timeout_secs);
        stdout = str(stdout, 'utf-8')
        if (ps.returncode != 0):
            print("Failed cmd = {}\nret = {}\nout = %s\n".format(cmd, ps.returncode, stdout))


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
        cmd = f"swapoff -a"
        self.execute_command(cmd, 180)

        cmd = f"parted {metadata_dev} mklabel gpt"
        self.execute_command(cmd, 180)

        end = 10
        if is_physical:
             end = 1000
        cmd = f"parted {metadata_dev} mkpart primary ext4 0% {end}GB"
        self.execute_command(cmd, 180)

        start = 11
        if is_physical:
            start = 1001
        cmd = f"parted {metadata_dev} mkpart primary ext2 {start}GB 100%"
        self.execute_command(cmd, 180)

        cmd = f"parted {metadata_dev} toggle 2 lvm"
        self.execute_command(cmd, 180)

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
        node_name = self.conf_store.get(self.index, 'node_name', default_val=None)
        metadata_device = self.conf_store.get(self.index, 'metadata_device', default_val=None)
        is_physical = True if self.conf_store.get(self.index, 'node_type', default_val=None) == "physical" else False
        
        print("In config_lvm node_name={} metadata_device={} is_physical={}".format(node_name, metadata_device, is_physical))
        self.create_lvm(node_name, metadata_device, is_physical)

    def config_motr(self):
        cmd = f"/opt/seagate/cortx/motr/libexec/motr_cfg.sh"
        print(f"Running {cmd}")
        self.execute_command(cmd, 180)

