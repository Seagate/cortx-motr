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

import io_req as ior
import md_req as mdr
from addb2db import *
from graphviz import Digraph
from req_utils import *


def graph_prepare(graph: Digraph, relations):
    #           relation  |     from    |    to       | table or direct | flags
    #                     |table/mapping|table/mapping|     mapping     |
    schema = [("s3_request_id", "s3_request_id", "client_id", "s3_request_to_client",  "C")]
    # flags: '1' - one to one mapping, 's' - stash samples, 'l' - leaf element

    graph_add_relations(graph, relations, schema)

def get_timelines_ioo(client_id: str, grange: int, client_pid: int, create_attr_graph: bool,
                      export_only: bool, ext_graph: Digraph):
    return ior.get_timelines(client_id, grange, client_pid, create_attr_graph,
                             export_only, ext_graph, False)

def get_timelines_cob(client_id: str, grange: int, client_pid: int, create_attr_graph: bool,
                      export_only: bool, ext_graph: Digraph):
    return ior.get_timelines(client_id, grange, client_pid, create_attr_graph,
                             export_only, ext_graph, True)

def get_timelines(s3reqs: List[str], pid: str, create_attr_graph: bool = False, verbose: bool = False):
    get_timelines_fns = [mdr.get_timelines, get_timelines_ioo, get_timelines_cob]
    time_table = []
    attr_graph = None

    for s3id in s3reqs:
        if "-" in s3id:
            s3_req_rel_d = s3_request_uid.select().where(s3_request_uid.uuid == s3id)
        else:
            s3_req_rel_d = s3_request_uid.select().where(s3_request_uid.id == s3id)

        if pid is not None:
            s3_req_rel_d = filter(lambda x: x.pid == pid, s3_req_rel_d)

        for s3_req_rel in s3_req_rel_d:
            s3_req_d = query2dlist(s3_request_state.select().where(
                (s3_request_state.id == s3_req_rel.id) &
                (s3_request_state.pid == s3_req_rel.pid)))
            for r in s3_req_d:
                r['op'] = s3_req_rel.uuid

            time_table.append(s3_req_d)

            s3_to_client_d = query2dlist(s3_request_to_client.select().where(
                (s3_request_to_client.s3_request_id == s3_req_rel.id) &
                (s3_request_to_client.pid == s3_req_rel.pid)))

            s3_to_client_d.sort(key=lambda s3rc: s3rc['client_id'])

            print("Processing S3 request {} (id {}, pid {}), client requests: "
                  .format(s3_req_rel.uuid, s3_req_rel.id, s3_req_rel.pid), end='')
            cids_str = ""
            for s3c in s3_to_client_d:
                cids_str += "{}, ".format(s3c['client_id'])
            print(cids_str[0:-2])

            if not verbose:
                sql_tmpl = """
select client_req.*, 'client[' || ifnull(cob.op, '') || ifnull(dix.op, '') || ifnull(ioo.op, '') || '] ' || client_req.id as 'op' from
client_req LEFT JOIN (select 'cob' as 'op', client_id, pid from client_to_cob) as cob ON cob.client_id = client_req.id AND cob.pid = client_req.pid
LEFT JOIN (select 'dix' as 'op', client_id, pid from client_to_dix) as dix ON dix.client_id = client_req.id AND dix.pid = client_req.pid
LEFT JOIN (select 'ioo' as 'op', client_id, pid from client_to_ioo) as ioo ON ioo.client_id = client_req.id AND ioo.pid = client_req.pid
WHERE client_req.id = {clvid} and client_req.pid = {clvpid};
"""
                for s3c in s3_to_client_d:
                    clvreq = sql_tmpl.format(clvid=s3c['client_id'], clvpid=s3c['pid'])
                    lbls = ["time", "pid", "id", "state", "op"]
                    clov_req_d = list(map(lambda tpl: dict(zip(lbls, tpl)),
                                          DB.execute_sql(clvreq).fetchall()))
                    time_table.append(clov_req_d)
                continue

            if create_attr_graph:
                attr_graph = Digraph(strict=True, format='png', node_attr = {'shape': 'plaintext'})
                relations = [dict(s3_request_id = s3_req_rel.id, cli_pid = s3_req_rel.pid, srv_pid = None)]
                graph_prepare(attr_graph, relations)

            for s3c in s3_to_client_d:
                found = False
                i = 0
                ext_tml = []

                print("Processing client request {} (pid {})...".format(s3c['client_id'], s3c['pid']))

                while not found and i < len(get_timelines_fns):
                    try:
                        ext_tml, _, _, _, _ = get_timelines_fns[i](s3c['client_id'], [0, 0], s3c['pid'],
                                                                   create_attr_graph, True, attr_graph)
                        found = True
                    except Exception: # pylint: disable=broad-except
                        print(sys.exc_info()[0])
                        pass
                    i = i + 1

                if found:
                    time_table += ext_tml
                    print("Done")
                else:
                    print("Failed")
                    print("Could not build timelines for client request {} (pid: {})".format(s3c['client_id'], s3c['pid']))

            if create_attr_graph:
                attr_graph.render(filename='s3_graph_{}'.format(s3_req_rel.uuid))

    return time_table

def parse_args():
    parser = argparse.ArgumentParser(description="draws s3 request timeline")
    parser.add_argument('--s3reqs', nargs='+', type=str, required=True,
                        help="requests ids to draw")
    parser.add_argument("-p", "--pid", type=int, required=False, default=None,
                        help="Client pid to get requests for")
    parser.add_argument('--db', type=str, required=False, default="m0play.db",
                        help="input database file")
    parser.add_argument("-a", "--attr", action='store_true', help="Create attributes graph")
    parser.add_argument("-m", "--maximize", action='store_true', help="Display in maximised window")
    parser.add_argument("-u", "--time-unit", choices=['ms','us'], default='us',
                        help="Default time unit")
    parser.add_argument("-v", "--verbose", action='store_true',
                        help="Display detailed request structure")
    parser.add_argument("-i", "--index", action='store_true',
                        help="Create indexes before processing")

    return parser.parse_args()

def create_table_index(tbl_model):
    index_query = "CREATE INDEX {} ON {} ({});"
    tbl_name = tbl_model.__name__
    tbl_fields = filter(lambda nm: (("id" in nm) or ("time" in nm)) and "__" not in nm,
                        tbl_model.__dict__.keys())
    for f in tbl_fields:
        iq = index_query.format(f"idx_{tbl_name}_{f}", tbl_name, f)
        try:
            DB.execute_sql(iq)
        except: # pylint: disable=bare-except
            print(sys.exc_info()[0])
            pass

def create_indexes():
    for tbl in db_create_delete_tables:
        create_table_index(tbl)

if __name__ == '__main__':
    args=parse_args()

    db_init(args.db)
    db_connect()

    if args.index:
        create_indexes()

    time_table = get_timelines(args.s3reqs, args.pid, args.attr, args.verbose)
    prepare_time_table(time_table)

    db_close()

    print("Plotting timelines...")

    draw_timelines(time_table, None, 0, None, args.time_unit, False, args.maximize)
