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

from addb2db import *
import matplotlib.pyplot as plt
import sys

def get_queue(query, sfrom: List[str], sto: List[str], filter_key_fields):
    qlength = []
    qtime   = []
    ql      = 0
    active  = set()

    item_d  = [q for q in query.dicts()]
    for item in item_d:
        update = False
        key = list()
        for f in filter_key_fields:
            key.append(item[f])
        key = tuple(key)
        if item['state'] in sfrom and key not in active:
            active.add(key)
            ql+=1
            update = True
        elif item['state'] in sto and key in active:
            active.remove(key)
            ql-=1
            update = True
        if update:
            qlength.append(ql)
            qtime.append(item['time'])

    return qlength, qtime

def plot(query, axs, subplot:int, title: str, sfrom: List[str], sto: List[str], extra: str, extra2, filter_key_fields):
    qextra  = []
    SEC     = 10**9

    qlength, qtime = get_queue(query, sfrom, sto, filter_key_fields)
    axs[subplot][extra].plot(qtime, qlength)
    axs[subplot][extra].set_ylabel(f"{title}")

    if extra2:
        item_d2  = [q for q in extra2.dicts()]
        q1time = [ i['time'] for i in item_d2]
        q1nr = [ i['NR'] for i in item_d2]
        axs[subplot][extra].plot(q1time, q1nr)

def qs_cli(cli_pids):
    qs_cli = []
    for cli_pid in cli_pids:
        qs_cli.append([
            { "query": client_req.select().where(client_req.pid==cli_pid).order_by(client_req.time),
              "title": "Client", "from": ["launched"], "to": ["stable"]},
            { "query": rpc_req.select(rpc_req, rpc_to_sxid.xid, rpc_to_sxid.session_id).\
              join(rpc_to_sxid, on=((rpc_req.id == rpc_to_sxid.id)&\
                                    (rpc_req.pid == rpc_to_sxid.pid))).\
              order_by(rpc_req.time).where(rpc_req.pid==cli_pid),
              "title": "rpc-cli", "from": ["INITIALISED"], "to": ["REPLIED"]}
            ]
        )
    return qs_cli

def qs_srv(srv_pids):
    qs_srv = []
    for srv_pid in srv_pids:
        qs_srv.append([
            { "query": rpc_req.select(rpc_req, sxid_to_rpc.xid, sxid_to_rpc.session_id).\
              join(sxid_to_rpc, on=((sxid_to_rpc.id==rpc_req.id)&\
                                    (sxid_to_rpc.pid==rpc_req.pid))).\
              order_by(rpc_req.time).where(rpc_req.pid==srv_pid),
              "title": "rpc-srv", "from": "ACCEPTED", "to": "REPLIED"},
            { "query": stio_req.select().order_by(stio_req.time).where(stio_req.pid==srv_pid),
              "title": f"stob-io", "from": ["M0_AVI_AD_PREPARE"], "to": ["M0_AVI_AD_ENDIO"]},
            { "query": be_tx.select().order_by(be_tx.time).where(be_tx.pid==srv_pid),
              "title": "TXs", "from": ["prepare"], "to": ["done"]},
            { "query": be_tx.select().order_by(be_tx.time).where(be_tx.pid==srv_pid),
              "title": "TXs:active", "from": ["active"], "to": ["closed"]},
            { "query": fom_req_state.select().order_by(fom_req_state.time).where(fom_req_state.pid==srv_pid),
              "title": "FOMs", "from": ["Init"], "to": ["Finished"]},
            { "query": fom_req_state.select().order_by(fom_req_state.time).where(fom_req_state.pid==1010101),
              "title": "FOMs-act", "from": ["Init"], "to": ["Finished"],
              "extra2": queues.select(queues.time, queues.avg.alias('NR')).order_by(queues.time).where((queues.pid==srv_pid)&(queues.type=="fom-active")),
          },
            { "query": fom_req_state.select().order_by(fom_req_state.time).where(fom_req_state.pid==1010101),
              "title": "FOMs-runq", "from": ["Init"], "to": ["Finished"],
              "extra2": queues.select(queues.time, queues.avg.alias('NR')).order_by(queues.time).where((queues.pid==srv_pid)&(queues.type=="runq")),
          },
            { "query": fom_req_state.select().order_by(fom_req_state.time).where(fom_req_state.pid==1010101),
              "title": "FOMs-wail", "from": ["Init"], "to": ["Finished"],
              "extra2": queues.select(queues.time, queues.avg.alias('NR')).order_by(queues.time).where((queues.pid==srv_pid)&(queues.type=="wail")),
          },
        ]
        )

    return qs_srv

def main(cli_pids, srv_pids):
    # time convertor
    CONV={"us": 1000, "ms": 1000*1000}
    time_unit="ms"

    cursor={"x0":0, "y0":0, "x1": 0, "y1": 0, "on": False}
    undo=[]
    def onpress(event):
        if event.key == 'a':
            cursor.update({ "on": True })
        elif event.key == 'd':
            if undo:
                for an in undo.pop():
                    an.remove()
        event.canvas.draw()

    def onrelease(event):
        if not cursor["on"]:
            return
        cursor.update({ "x1": event.xdata, "y1": event.ydata })
        cursor.update({ "on": False })

        an1=event.inaxes.axvspan(cursor["x0"], cursor["x1"], facecolor='0.9', alpha=.5)
        an2=event.inaxes.annotate('', xy=(cursor["x0"], cursor["y0"]),
                                  xytext=(cursor["x1"], cursor["y0"]),
                                  xycoords='data', textcoords='data',
                                  arrowprops={'arrowstyle': '|-|'})
        an3=event.inaxes.annotate(str(round((cursor["x1"]-cursor["x0"])/CONV[time_unit],2))+f" {time_unit}",
                                  xy=(min(cursor["x1"], cursor["x0"])+
                                      abs(cursor["x1"]-cursor["x0"])/2, 0.5+cursor["y0"]),
                                  ha='center', va='center')
        undo.append([an1, an2, an3])
        event.canvas.draw()

    def onclick(event):
        if not cursor["on"]:
            return
        cursor.update({ "x0": event.xdata, "y0": event.ydata })

    q_cli = qs_cli(cli_pids)
    q_srv = qs_srv(srv_pids)
    cli_nrows = len(q_cli) * len(q_cli[0]) if len(q_cli) > 0 else 0
    srv_nrows = len(q_srv[0]) if len(q_srv) > 0 else 0

    ncols = 1 if cli_nrows > 0 else 0
    ncols += len(q_srv)

    fig, axs = plt.subplots(ncols=ncols, nrows=max(cli_nrows, srv_nrows), sharex=True)

    fig.canvas.mpl_connect('key_press_event', onpress)
    fig.canvas.mpl_connect('button_press_event', onclick)
    fig.canvas.mpl_connect('button_release_event', onrelease)

    sub = 0
    if len(q_cli) > 0:
        for qs in q_cli:
            for q in qs:
                filter_fields = ('id', 'pid')
                if q['title'] == "rpc-cli":
                    filter_fields = ('xid', 'session_id', 'pid')
                plot(q['query'], axs, sub, q['title'], q['from'], q['to'], 0, q.get("extra2"), filter_fields)
                sub += 1

    row = 1 if len(q_cli) > 0 else 0
    for qs in q_srv:
        sub = 0
        for q in qs:
            filter_fields = ('id', 'pid')
            if q['title'] == "rpc-srv":
                filter_fields = ('xid', 'session_id', 'pid')
            plot(q['query'], axs, sub, q['title'], q['from'], q['to'], row, q.get("extra2"), filter_fields)
            sub += 1

        row += 1

    plt.show()

def parse_args():
    parser = argparse.ArgumentParser(prog=sys.argv[0], description="""
    queues.py: Display queues.
    """)
    parser.add_argument("--cpids", required=False,
                        help="Comma separated list of client PIDs, example: --cpids 42642,43132")
    parser.add_argument("--spids", required=False,
                        help="Comma separated list of server PIDs, example: --spids 29,78")
    parser.add_argument("-d", "--db", type=str, default="m0play.db",
                        help="Performance database (m0play.db)")

    return parser, parser.parse_args()

if __name__ == '__main__':
    parser, args = parse_args()

    cli_pids = []
    srv_pids = []

    if args.cpids:
        cli_pids = args.cpids.split(",")
    if args.spids:
        srv_pids = args.spids.split(",")

    if len(cli_pids) == 0 and len(srv_pids) == 0:
        print("At least one PID must be specified")
        parser.print_help(sys.stderr)
        exit(1)

    db_init(args.db)
    db_connect()

    main(cli_pids, srv_pids)

    db_close()
