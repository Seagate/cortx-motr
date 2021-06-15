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

# METADATA PATH DB SCHEMA DIAGRAM
# ===============================
# |-----------------------------------CLIENT-SIDE------------------------------------|-----------------------SERVER-SIDE----------------------|
#
#                                                                              (rpc_to_sxid)
#                                                                                  |   ^
#                                                                                  V   |
#                                                                              (sxid_to_rpc)
#          client_to_dix    dix_to_mdix     dix_to_cas       cas_to_rpc             |                                        fom_to_tx
#                |              |  (meta_dix)   |               |                   |                                           |
# client_req:id --> dix_req:id --> dix_req:id -----> cas_req:id --> rpc_req:id  ------------> fom_desc:{rpc_sm_id, fom_sm_id}  -----> be_tx:id
#                      \               \...
#                      \               +-----------> cas_req:id --> rpc_req:id  ------------> fom_desc:{rpc_sm_id, fom_sm_id}  -----> be_tx:id
#                      \
#                      +---------------------------> cas_req:id --> rpc_req:id  ------------> fom_desc:{rpc_sm_id, fom_sm_id}  -----> be_tx:id
#                      \ ...
#                      \
#                      +---------------------------> cas_req:id --> rpc_req:id  ------------> fom_desc:{rpc_sm_id, fom_sm_id}  -----> be_tx:id
#
# I/O PATH DB SCHEMA DIAGRAM
# ==========================
#                                                                              (rpc_to_sxid)
#                                                                                  |   ^
#                                                                                  V   |
#                                                                              (sxid_to_rpc)
#                client_to_ioo                     ioo_to_rpc                        |                                       fom_to_tx
#                       |                               |                            |                                           |
# client_req:id ------------------>  ioo_req:id -----------------> rpc_req:id  --------------> fom_desc:{rpc_sm_id, fom_sm_id} ------> be_tx:id
#            \                                                                      ...
#             \  client_to_cob                     cob_to_rpc                        |                                       fom_to_tx
#              \        |                               |                            |                                           |
#               +----------------->  cob_req:id ------------------> rpc_req:id --------------> fom_desc:{rpc_sm_id, fom_sm_id} ------> be_tx:id
#                                                                            \
#                                                                             +--> bulk_req:id

import argparse
import logging
import yaml
import numpy
import time
from peewee import *
from typing import List
import multiprocessing
from itertools import zip_longest
from collections import defaultdict
from tqdm import tqdm
from plumbum.cmd import wc
from math import ceil
import sys
from functools import partial
from os.path import basename, splitext
import re
from dateutil.parser import parse as dtparse
from datetime import datetime


DB      = SqliteDatabase(None)
BLOCK   = 16<<10
PROC_NR = 48
DBBATCH = 95
PID     = 0

def die(what: str):
    print(what, file=sys.stderr)
    sys.exit(1)

# ======================================================================

class BaseModel(Model):
    class Meta:
        database = DB

class fom_to_tx(BaseModel):
    pid    = IntegerField()
    fom_id = IntegerField()
    tx_id  = IntegerField()

class cas_fom_to_crow_fom(BaseModel):
    pid          = IntegerField()
    fom_id       = IntegerField()
    crow_fom_id  = IntegerField()

class fom_desc(BaseModel):
    time       = IntegerField()
    pid        = IntegerField()
    service    = TextField()
    sender     = TextField()
    req_opcode = TextField()
    rep_opcode = TextField()
    local      = TextField()
    rpc_sm_id  = IntegerField()
    fom_sm_id  = IntegerField()
    fom_state_sm_id = IntegerField()

class sxid_to_rpc(BaseModel):
    time       = IntegerField()
    pid        = IntegerField()
    opcode     = IntegerField()
    xid        = IntegerField()
    session_id = IntegerField()
    id         = IntegerField()

class rpc_to_sxid(BaseModel):
    time       = IntegerField()
    pid        = IntegerField()
    opcode     = IntegerField()
    xid        = IntegerField()
    session_id = IntegerField()
    id         = IntegerField()

class fom_req(BaseModel):
    time   = IntegerField()
    pid    = IntegerField()
    id     = IntegerField()
    state  = TextField()

class fom_req_state(BaseModel):
    time   = IntegerField()
    pid    = IntegerField()
    id     = IntegerField()
    state  = TextField()

class tx_to_gr(BaseModel):
    pid   = IntegerField()
    tx_id = IntegerField()
    gr_id = IntegerField()

class be_tx(BaseModel):
    time   = IntegerField()
    pid    = IntegerField()
    id     = IntegerField()
    state  = TextField()

class rpc_req(BaseModel):
    time   = IntegerField()
    pid    = IntegerField()
    id     = IntegerField()
    state  = TextField()

class client_req(BaseModel):
    time   = IntegerField()
    pid    = IntegerField()
    id     = IntegerField()
    state  = TextField()

class dix_req(BaseModel):
    time   = IntegerField()
    pid    = IntegerField()
    id     = IntegerField()
    state  = TextField()

class cas_req(BaseModel):
    time   = IntegerField()
    pid    = IntegerField()
    id     = IntegerField()
    state  = TextField()

class cas_to_rpc(BaseModel):
    pid    = IntegerField()
    cas_id = IntegerField()
    rpc_id = IntegerField()

class dix_to_cas(BaseModel):
    pid    = IntegerField()
    dix_id = IntegerField()
    cas_id = IntegerField()

class dix_to_mdix(BaseModel):
    pid     = IntegerField()
    dix_id  = IntegerField()
    mdix_id = IntegerField()

class client_to_dix(BaseModel):
    pid       = IntegerField()
    client_id = IntegerField()
    dix_id    = IntegerField()

class client_to_cob(BaseModel):
    pid       = IntegerField()
    client_id = IntegerField()
    cob_id    = IntegerField()

class cob_to_rpc(BaseModel):
    pid    = IntegerField()
    cob_id = IntegerField()
    rpc_id = IntegerField()

class client_to_ioo(BaseModel):
    pid       = IntegerField()
    client_id = IntegerField()
    ioo_id    = IntegerField()

class ioo_to_rpc(BaseModel):
    pid    = IntegerField()
    ioo_id = IntegerField()
    rpc_id = IntegerField()

class cob_req(BaseModel):
    time   = IntegerField()
    pid    = IntegerField()
    id     = IntegerField()
    state  = TextField()

class ioo_req(BaseModel):
    time   = IntegerField()
    pid    = IntegerField()
    id     = IntegerField()
    state  = TextField()

class fom_to_stio(BaseModel):
    pid     = IntegerField()
    fom_id  = IntegerField()
    stio_id = IntegerField()

class stio_req(BaseModel):
    time   = IntegerField()
    pid    = IntegerField()
    id     = IntegerField()
    state  = TextField()

class attr(BaseModel):
    entity_id = IntegerField()
    pid       = IntegerField()
    name      = TextField()
    val       = TextField()

class bulk_to_rpc(BaseModel):
    bulk_id = IntegerField()
    rpc_id  = IntegerField()
    pid     = IntegerField()

class queues(BaseModel):
    pid  = IntegerField()
    type = TextField()
    locality = IntegerField()
    time = IntegerField()
    nr   = IntegerField()
    min  = IntegerField()
    max  = IntegerField()
    avg  = FloatField()
    dev  = FloatField()

class s3_request_to_client(BaseModel):
    pid           = IntegerField()
    s3_request_id = IntegerField()
    client_id     = IntegerField()

class s3_request_uid(BaseModel):
    pid = IntegerField()
    id  = IntegerField()
    uuid = TextField()

class s3_request_state(BaseModel):
    time  = IntegerField()
    pid   = IntegerField()
    id    = IntegerField()
    state = TextField()

class s3_measurement(BaseModel):
    time  = IntegerField()
    pid   = IntegerField()
    name  = TextField()
    val1  = IntegerField(null=False)
    val2  = IntegerField(null=True)
    val3  = IntegerField(null=True)
    val4  = IntegerField(null=True)
    val5  = IntegerField(null=True)
    val6  = IntegerField(null=True)
    val7  = IntegerField(null=True)
    val8  = IntegerField(null=True)
    val9  = IntegerField(null=True)
    val10  = IntegerField(null=True)
    val11  = IntegerField(null=True)
    val12  = IntegerField(null=True)
    val13  = IntegerField(null=True)

db_create_delete_tables = [client_to_dix, dix_to_mdix, dix_to_cas, cas_to_rpc,
                           cas_req, dix_req, client_req, rpc_req, rpc_to_sxid,
                           sxid_to_rpc, fom_desc, fom_to_tx, fom_req, be_tx,
                           client_to_cob, cob_to_rpc, client_to_ioo, ioo_to_rpc,
                           cob_req, ioo_req, fom_req_state, queues, tx_to_gr,
                           fom_to_stio, stio_req, attr, bulk_to_rpc,
                           cas_fom_to_crow_fom, s3_request_to_client,
                           s3_request_uid, s3_request_state, s3_measurement]

def db_create_tables():
    with DB:
        DB.create_tables(db_create_delete_tables)

def db_drop_tables():
    with DB:
        DB.drop_tables(db_create_delete_tables)

def db_init(path):
    DB.init(path, pragmas={
        'journal_mode': 'off',
        'cache_size': -1024*1024*256,
        'synchronous': 'off',
    })

def db_connect():
    DB.connect()

def db_close():
    DB.close()

# ======================================================================

class profiler:
    def __init__(self, what):
        self.what = what

    def __exit__(self, exp_type, exp_val, traceback):
        delta = time.time() - self.start
        logging.info(f"{self.what}: {delta}s")
    def __enter__(self):
        self.start = time.time()

# ======================================================================

class ADDB2PP:
    @staticmethod
    def clean_yaml(yml):
        return yml.translate(str.maketrans("><-","''_"))

    @staticmethod
    def to_unix(motr_time):
        mt = list(motr_time)
        mt[10] = 'T'
        np_time = numpy.datetime64("".join(mt))
        return np_time.item()

    # ['*', '2019-09-18-19:08:50.975943665', 'fom-phase',
    #  'sm_id:', '38', '-->', 'HA_LINK_OUTGOING_STATE_WAIT_REPLY']
    def p_sm_req(measurement, labels, table):
        name   = measurement[2]
        time   = measurement[1]
        state  = measurement[-1]
        sm_id  = measurement[4]
        return((table, { 'time': ADDB2PP.to_unix(time), 'state': state, 'id': int(sm_id) }))

    # ['*', '2019-08-29-12:16:54.279414683',
    #  'client-to-dix', 'client_id:', '1170,', 'dix_id:', '1171']
    def p_1_to_2(measurement, labels, table):
        ret = yaml.safe_load("{"+" ".join(measurement[3:])+"}")
        return((table, ret))

    # ['*', '2019-08-29-12:08:23.766071289', 'fom-descr', 'service:', '<0:0>,',  'sender:', '0,', 'req-opcode:', 'none,',  'rep-opcode:', 'none,', 'local:', 'false,', 'rpc_sm_id:', '0,',    'fom_sm_id:', '0']
    # ['*', '2019-08-29-12:16:48.097420953', 'rpc-item-id-assign', 'id:', '19,', 'opcode:', '117,', 'xid:', '1,', 'session_id:', '98789222400000038']
    # [* 2020-03-03-21:55:21.632535498 stio-req-state   stio_id: 1345, stio_state: M0_AVI_LIO_ENDIO]
    # [* 2020-03-03-21:55:19.141584520 s3-request-state s3_request_id: 3, state: START]
    # [* 2019-09-07-09:57:43.936545770 cob-req-state    cob_id: 1310, cob_state: 2]
    # def p_cob_req(measurement, labels, table):
    # def p_stio_req(measurement, mnl, param):
    # def p_rpc_item_id(measurement, labels, table):
    # def p_yaml_req(measurement, labels, table):
    def p_yaml_translate(translate_dict, measurement, labels, table):
        # cob_req: {id: cob_id, state: cob_state}
        # stio_req: {id: stio_id, state: stio_state}
        # s3_req: {id: s3_request_id}
        # rpc_item_id: {}
        # yaml_req: {}
        name  = measurement[2]
        time  = measurement[1]
        # XXX: This is a hot fix. Sergey, Dmitry please find out better solution.
        # XXX: test case: {'id': 19, 'opcode': 33, 'xid': '_1', 'session_id': 0, 'time': 1586878694440410275, 'pid': 30},
        # XXX: xid = "_1" should be "-1"
        clean = (lambda x: x) if name in ["rpc-item-id-fetch",
                                          "rpc-item-id-assign"] else ADDB2PP.clean_yaml
        for i,m in enumerate(measurement[3:]) :
            measurement[i+3] = m.replace("::", "_")
        ret   = yaml.safe_load(clean("{"+" ".join(measurement[3:])+"}"))
        ret['time']  = ADDB2PP.to_unix(time)
        for k,v in translate_dict.items():
            ret[k] = ret.pop(v)
        return((table, ret))

    # [* 2019-11-01-20:27:37.467306782 wail  nr: 992 min: 1 max: 4 avg: 2.719758 dev: 0.461787]
    # [.. | ..         locality         0]
    def p_queue(measurement, labels, table):
        name  = measurement[2]
        time  = measurement[1]
        stat  = measurement[3:13]
        ret = dict(zip([s[:-1] for s in stat[::2]], stat[1::2]))
        ret['time'] = ADDB2PP.to_unix(time)
        ret['type'] = name
        ret.update({"locality":
                    labels.get("locality") or
                    labels.get("stob-ioq-thread") or
                    die(f" {measurement} / {labels} : Label not found!")})
        return((table, ret))

    # ['*'
    #  '2019-11-21-11:32:38.717028449',
    #  'attr',
    #  'entity_id:', '1150,', 'M0_AVI_ATTR__RPC_OPCODE:', 'M0_IOSERVICE_READV_OPCODE']
    def p_attr(measurement, labels, table):
        name      = measurement[2]
        entity_id = measurement[4][:-1]
        attr_name = measurement[5][:-1]
        attr_val  = str(measurement[6])
        ret   = { 'entity_id': entity_id, 'name': attr_name, 'val': attr_val }
        return((table, ret))

    # ['*',
    #  '2020-01-26-17:14:57.134583699'
    #  's3-request-uid'
    #  's3_request_id:'
    #  '3,',
    #  'uid_first_64_bits:'
    #  '0x9d4251f41ddb76f0,',
    #  'uid_last_64_bits:',
    #  '0xbe11ec28e6e52a80']
    # uid form: f076db1d-f451-429d-802a-e5e628ec11be
    def s3req_uid(measurement, labels, table):
        def s3req_bytes_swap(hexstr):
            t = int(hexstr.strip(" ,"), 16)
            t = f"{t:016x}"
            s = []
            for i in range(int(len(t) / 2)):
                s.append(int(t[2*i:2*i+2], 16))
            s.reverse()
            return "".join(map(lambda a: f"{a:02x}", s))

        ret = {}
        ret['id'] = int(measurement[4][:-1])
        first = s3req_bytes_swap(measurement[6])
        last = s3req_bytes_swap(measurement[8])
        ret['uuid'] = f"{first[:8]}-{first[8:12]}-{first[12:16]}-{last[:4]}-{last[4:]}"
        return((table, ret))

    # ['*', '2020-05-20-01:41:07.988231709', 's3-measurement', 'TRACE_POINT,', '1']
    # ['*', '2020-05-20-01:41:08.060563989', 's3-measurement', 'TRACE_POINT_2,', '2,', '3']
    def p_s3_msrm(measurement, labels, table):
        time = measurement[1]
        msrm = measurement[3].strip(',')
        ret = dict(zip_longest(
            [f"val{i}" for i in range(1,14)],
            map(lambda x: int(x.strip(',')), measurement[4:])))
        ret["time"] = ADDB2PP.to_unix(time)
        ret["name"] = msrm
        return((table, ret))

    def __init__(self):
        self.parsers = {
            "runq"              : (ADDB2PP.p_queue,       "queues"),
            "wail"              : (ADDB2PP.p_queue,       "queues"),
            "fom-active"        : (ADDB2PP.p_queue,       "queues"),
            "loc-forq-hist"     : (ADDB2PP.p_queue,       "queues"),
            "loc-wait-hist"     : (ADDB2PP.p_queue,       "queues"),
            "loc-cb-hist"       : (ADDB2PP.p_queue,       "queues"),
            "loc-queue-hist"    : (ADDB2PP.p_queue,       "queues"),
            "stob-ioq-inflight" : (ADDB2PP.p_queue,       "queues"),
            "stob-ioq-queued"   : (ADDB2PP.p_queue,       "queues"),
            "stob-ioq-got"      : (ADDB2PP.p_queue,       "queues"),
            "rpc-item-id-fetch" : (partial(ADDB2PP.p_yaml_translate, {}), "sxid_to_rpc"),
            "fom-descr"         : (partial(ADDB2PP.p_yaml_translate, {}), "fom_desc"),
            "tx-state"          : (ADDB2PP.p_sm_req,      "be_tx"),
            "fom-phase"         : (ADDB2PP.p_sm_req,      "fom_req"),
            "fom-state"         : (ADDB2PP.p_sm_req,      "fom_req_state"),
            "fom-to-tx"         : (ADDB2PP.p_1_to_2,      "fom_to_tx"),
            "tx-to-gr"          : (ADDB2PP.p_1_to_2,      "tx_to_gr"),
            "cas-to-rpc"        : (ADDB2PP.p_1_to_2,      "cas_to_rpc"),
            "dix-to-cas"        : (ADDB2PP.p_1_to_2,      "dix_to_cas"),
            "dix-to-mdix"       : (ADDB2PP.p_1_to_2,      "dix_to_mdix"),
            "client-to-dix"     : (ADDB2PP.p_1_to_2,      "client_to_dix"),
            "rpc-item-id-assign": (partial(ADDB2PP.p_yaml_translate, {}), "rpc_to_sxid"),
            "rpc-out-phase"     : (ADDB2PP.p_sm_req,      "rpc_req"),
            "rpc-in-phase"      : (ADDB2PP.p_sm_req,      "rpc_req"),
            "cas-req-state"     : (ADDB2PP.p_sm_req,      "cas_req"),
            "dix-req-state"     : (ADDB2PP.p_sm_req,      "dix_req"),
            "op-state"          : (ADDB2PP.p_sm_req,      "client_req"),
            "client-to-cob"     : (ADDB2PP.p_1_to_2,      "client_to_cob"),
            "cob-to-rpc"        : (ADDB2PP.p_1_to_2,      "cob_to_rpc"),
            "client-to-ioo"     : (ADDB2PP.p_1_to_2,      "client_to_ioo"),
            "ioo-to-rpc"        : (ADDB2PP.p_1_to_2,      "ioo_to_rpc"),
            "ioo-req-state"     : (ADDB2PP.p_sm_req,      "ioo_req"),
            "cob-req-state"     : (partial(ADDB2PP.p_yaml_translate, {"id": "cob_id", "state": "cob_state"}), "cob_req"),
            "stio-req-state"    : (partial(ADDB2PP.p_yaml_translate, {"id": "stio_id", "state": "stio_state"}), "stio_req"),
            "fom-to-stio"       : (ADDB2PP.p_1_to_2,      "fom_to_stio"),
            "attr"              : (ADDB2PP.p_attr,        "attr"),
            "bulk-to-rpc"       : (ADDB2PP.p_1_to_2,      "bulk_to_rpc"),
            "cas-fom-to-crow-fom" : (ADDB2PP.p_1_to_2,    "cas_fom_to_crow_fom"),

            "s3-request-to-client" : (ADDB2PP.p_1_to_2,  "s3_request_to_client"),
            "s3-request-uid"       : (ADDB2PP.s3req_uid, "s3_request_uid"),
            "s3-request-state"     : (partial(ADDB2PP.p_yaml_translate, {"id": "s3_request_id"}), "s3_request_state"),
            "s3-measurement"       : (ADDB2PP.p_s3_msrm, "s3_measurement"),
        }

    def consume_record(self, rec):
        def _add_pid(_,ret):
            ret.update({"pid": PID})
            return ((_,ret))

        # measurement[0] and labels[1..] (mnl)
        mnl = rec.split("|")
        measurement = mnl[0].split()
        if measurement == []:
            return
        measurement_name = measurement[2]

        labels=dict([kvf for kvf in [kv.strip().split() for kv in mnl[1:]]
                     if kvf and len(kvf)==2])

        for pname, (parser, table) in self.parsers.items():
            if pname == measurement_name:
                return _add_pid(*parser(measurement, labels, table))
        return None

APP = ADDB2PP()
def fd_consume_record(rec):
    return APP.consume_record(rec) if rec else None

def fd_consume_data(file, pool):
    def grouper(n, iterable, padvalue=None):
        return zip_longest(*[iter(iterable)]*n,
                           fillvalue=padvalue)
    results=[]
    _wc = int(wc["-l", file]().split()[0])
    _wc = ceil(_wc/BLOCK)*BLOCK

    with tqdm(total=_wc, desc=f"Read file: {file}") as t:
        with open(file) as fd:
            for chunk in grouper(BLOCK, fd):
                results.extend(pool.map(fd_consume_record, chunk))
                t.update(BLOCK)

    return results

def fd_id_get(f):
    f_name = splitext((basename(f)))[0]
    fid = f_name.split("_")[-1]
    return int(fid) if fid.isnumeric() else int(fid, base=16)

def db_consume_data(files: List[str]):
    rows   = []
    tables = defaultdict(list)

    if len(files) == 0:
        return

    db_drop_tables()
    db_create_tables()

    with profiler(f"Read files: {files}"):
        for f in files:
            def pool_init(pid):
                global PID; PID=pid
            # Ugly reinitialisation of the pool due to PID value propagation
            pool = multiprocessing.Pool(PROC_NR, pool_init, (fd_id_get(f),))
            rows.extend(filter(None, fd_consume_data(f, pool)))
        for k,v in rows:
            tables[k].append(v)

    with tqdm(total=len(rows), desc="Insert records") as t:
        with profiler("Write to db"):
            for k in tables.keys():
                batching = DBBATCH
                while batching >= 0:
                    if batching == 0:
                        raise Exception("Cannot insert records with zero dbbatch size")
                    with DB.atomic() as dbatomic:
                        with tqdm(total=len(tables[k]), desc=f"into {k}") as tbl:
                            with profiler(f"    {k}/{len(tables[k])}"):
                                try:
                                    for batch in chunked(tables[k], batching):
                                        globals()[k].insert_many(batch).execute()
                                        tbl.update(len(batch))
                                    break
                                except OperationalError as ex:
                                    if "too many" in str(ex):
                                        logging.warning(f"insert recs int {k} err {ex}")
                                        dbatomic.rollback()
                                        batching = batching // 2
                                    else:
                                        raise ex
                t.update(len(tables[k]))

def db_setup_loggers():
    format='%(asctime)s %(name)s %(levelname)s %(message)s'
    level=logging.INFO
    level_sh=logging.WARN
    logging.basicConfig(filename='logfile.txt',
                        filemode='w',
                        level=level,
                        format=format)

    sh = logging.StreamHandler()
    sh.setFormatter(logging.Formatter(format))
    sh.setLevel(level_sh)
    log = logging.getLogger()
    log.addHandler(sh)


auth_srv_format = re.compile(r"\A([0-9\- :,]{23,29})\s+\w+\s+\[ReqId:([0-9a-fA-F\-]{36})\]\s+(\S+).*")

def parse_app_record(res):
    mm = auth_srv_format.match(res)
    ret = {}
    if mm:
        dt = dtparse(mm.group(1))
        tdelt = int((dt - datetime.utcfromtimestamp(0)).total_seconds())
        ret["time"] = tdelt * 1000000000 + dt.microsecond * 1000
        ret["id"] = mm.group(2)
        ret["state"] = mm.group(3)
    return ret

def parse_app_data(file, pool):
    results={}
    cont = []

    try:
        with open(file) as f:
            cont = f.readlines()
    except:
        print(f"Error app reading file {file}")
        return results

    blk = min(BLOCK, len(cont) // PROC_NR)

    with tqdm(total=len(cont), desc=f"Process app file: {file}") as t:
        for r in pool.imap_unordered(parse_app_record, cont, blk):
            if "id" in r:
                results.setdefault(r["id"], []).append(r)
            t.update(1)

    return results

def parse_app_logs(applogs: List[str], pool):
    states = {}

    with profiler(f"Read application files: {applogs}"):
        for app in applogs:
            states.update(parse_app_data(app, pool))

    req_states = []
    for req in s3_request_uid.select():
        if req.uuid in states:
            for st in states[req.uuid]:
                req_states.append(
                    {"time":st["time"],
                     "pid":req.pid,
                     "id":req.id,
                     "state": st["state"]})

    with profiler("Write app to db"):
        batching = DBBATCH
        while batching >= 0:
            if batching == 0:
                raise Exception("Cannot insert app records with zero dbbatch size")
            with DB.atomic() as dbatomic:
                try:
                    with tqdm(total=len(req_states), desc="Insert app records") as t:
                        for batch in chunked(req_states, batching):
                            s3_request_state.insert_many(batch).execute()
                            t.update(len(batch))
                    break
                except OperationalError as ex:
                    if "too many" in str(ex):
                        logging.warning(f"app insert recs err {ex}")
                        dbatomic.rollback()
                        batching = batching // 2
                    else:
                        raise ex

def db_parse_args():
    parser = argparse.ArgumentParser(description="""
addb2db.py: creates sql database containing performance samples
    """)
    parser.add_argument('--dumps', nargs='+', type=str, required=False,
                        default=[],
                        help="""
A bunch of addb2dump.txts can be passed here for processing:
python3 addb2db.py --dumps dump1.txt dump2.txt ...
""")
    parser.add_argument('--db', type=str, required=False,
                        default="m0play.db",
                        help="Output database file")
    parser.add_argument('--procs', type=int, required=False,
                        default=PROC_NR,
                        help="Number of processes to parse dump files")
    parser.add_argument('--block', type=int, required=False,
                        default=BLOCK,
                        help="Block of data from dump files processed at once")
    parser.add_argument('--batch', type=int, required=False,
                        default=DBBATCH,
                        help="Number of samples commited at once")
    parser.add_argument('--app', nargs='+', type=str, required=False,
                        default=[],
                        help="Application logs path, e.g. AuthServer")

    return parser.parse_args()

if __name__ == '__main__':
    args=db_parse_args()
    BLOCK=args.block
    PROC_NR=args.procs
    DBBATCH=args.batch

    db_init(args.db)
    db_setup_loggers()
    db_connect()

    db_consume_data(args.dumps)

    with multiprocessing.Pool(PROC_NR) as gpool:
        parse_app_logs(args.app, gpool)

    db_close()
