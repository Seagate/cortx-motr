#!/usr/bin/env python
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
import math

# This script calculates CORTX meta-data usage. 

# References: 

# https://seagatetechnology.sharepoint.com/:p:/r/sites/gteamdrv1/tdrive1224/_layouts/15/Doc.aspx?sourcedoc=%7B4109FB0E-4FB8-45D1-B602-45873B67500F%7D&file=Mero%20meta-data%20overview.pptx&action=edit&mobileredirect=true&DefaultItemOpen=1 


# amount of meta-data (in bytes) necessary to store an S3 object in the given
# bucket, with the given object key (key), extended attributes (ea) and content
# length (size), all measured in bytes.
#
# md() includes permanent meta-data cost of an object and does not include
# temporary meta-data, like "probably delete" records, addb and trace records,
# and space used in the transaction log.
#
# Assume R-way meta-data replication and N+K+S data striping over a pool with P
# devices.
def md(bucket, key, ea, size):
    return e(bucket, key, ea) + i(size);

# E(key) is the space used to store external meta-data (i.e., meta-data
# explicitly stored by S3 in dix) and I(size) is the space used to store
# internal meta-data.
def e(bucket, key, ea):
    return R * dix(key, obj_list_rec(bucket, key, ea));

def i(size):
    return P * (cob() + ad(size*(N+K)/N/P, U(size)));

# obj_list_rec(key, ea) is the size of S3 json-formatted value in
# object_list_idx_oid index.
def obj_list_rec(bucket, key, ea):
    return bucket + key + ea + 1700;

# Where dix(key, val) is space needed to store 1 replica of key-value pair in dix, 
def dix(key, val):
    return btree(8 + key, 8 + val);

def btree(key, val):
    if r1:
        return bseg(8 * math.ceil(key/8) + val) + 8;
    else:
        return key + val + 8;

def bseg(x):
    return 80 + 8 * math.ceil(x/8);

# Where cob is per-cob overhead, ad(nob, unit)---the overhead of writing nob bytes
# in a cob with the given unit size.
def cob():
    return btree(oi_key, oi_val) + btree(ns_key, ns_val);

def ad(nob, unit):
    return frag * (nob / unit) * btree(extmap_key, extmap_val);

def U(size):
    if size < 1024*1024:
        return size;
    else:
        return 1024*1024;

r1 = True;
R = 0;
N = 0;
K = 0;
S = 0;
P = 0;
frag = 1;
extmap_key = 54;
extmap_val = 48;
oi_key     = 20;
oi_val     = 20;
ns_key     = 20;
ns_val     = 15*8; 

def tabulate():
    global r1;
    global R;
    global N;
    global K;
    global S;
    global P;
    for size in [1, 512, 1024, 4096, 32*1024, 64*1024, 256*1024, 1024*1024,
                 2*1024*1024, 4*1024*1024, 8*1024*1024, 32*1024*1024,
                 64*1024*1024, 256*1024*1024]:
        r1 = True;
        R = 1;
        N = 1;
        K = 0;
        S = 0;
        P = 7*2;
        r1md = md(16, 20, 1024, size);
        r2 = True;
        R = 3;
        N = 4;
        K = 2;
        S = 0;
        P = 7*6;
        r2md = md(16, 20, 1024, size);
        print(size, r1md, r1md/size, r2md, r2md/size);

if __name__ == "__main__":
    tabulate()
