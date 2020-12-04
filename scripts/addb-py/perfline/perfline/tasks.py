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

import config
from config import huey
from datetime import datetime
import os
import plumbum


def get_overrides(overrides):
    return " ".join([f"{x}={y}" for (x, y) in overrides.items()])


def parse_options(conf, result_dir):
    options = []

    options.append('-p')
    options.append(result_dir)

    options.append('--nodes')
    options.append(config.nodes)
    options.append('--ha_type')
    options.append(config.ha_type)

    for w in conf['workload']:
        options.append('-w')
        options.append(w['cmd'])

    # Execution options:
    if 'mkfs' in conf['execution_options']:
        if conf['execution_options']['mkfs']:
            options.append("--mkfs")
    if conf['execution_options']['no_m0trace_files']:
        options.append("--no-m0trace-files")
    if conf['execution_options']['no_m0trace_dumps']:
        options.append("--no-m0trace-dumps")
    if conf['execution_options']['no_addb_stobs']:
        options.append("--no-addb-stobs")
    if conf['execution_options']['no_addb_dumps']:
        options.append("--no-addb-dumps")
    if conf['execution_options']['no_m0play_db']:
        options.append("--no-m0play-db")

    print(options)
    return options


def run_cmds(cmds, path):
    # TODO: Implement me!
    return


def send_mail(to, status, tid):
    nl = "\n"
    msg = f"Subject: Cluster task queue{nl}Task {tid} {status}"
    sendmail = plumbum.local["sendmail"]
    echo = plumbum.local["echo"]
    chain = echo[msg] | sendmail[to]
    try:
        chain()
    except:
        print(f"Couldn't send email to {to}")


def pack_artifacts(path):
    tar = plumbum.local["tar"]
    parent_dir = '/'.join(path.split('/')[:-1])
    archive_dir = path.split('/')[-1]
    tar[f"-cJvf {parent_dir}/{archive_dir}.tar.xz -C {parent_dir} {archive_dir}".split(
        " ")] & plumbum.FG
    print(f"Rm path: {path}")
    rm = plumbum.local["rm"]
    # rm[f"-rf {path}".split(" ")]()


@huey.task(context=True)
def worker_task(conf_opt, task):
    conf, opt = conf_opt
    current_task = {
        'task_id': task.id,
        'pid': os.getpid(),
        'args': conf_opt,
    }
    huey.put('current_task', current_task)

    result = {
        'conf': conf,
        'start_time': str(datetime.now()),
        'path': f"{config.artifacts_dir}",
        'artifacts_dir': f"{config.artifacts_dir}/result_{task.id}",
    }
    result.update(opt)

    if config.pack_artifacts:
        result['archive_name'] = f"result_{task.id}.tar.xz"

    if conf['common']['send_email']:
        send_mail(conf['common']['user'], "Task started", task.id)

    if 'pre_exec_cmds' in conf:
        run_cmds(conf['pre_exec_cmds'], result['artifacts_dir'])

    with plumbum.local.env():
        run_workload = plumbum.local["scripts/worker.sh"]
        try:
            tee = plumbum.local['tee']
            options = parse_options(conf, result["artifacts_dir"])
            (run_workload[options] | tee['/tmp/workload.log']) & plumbum.FG
            result['status'] = 'SUCCESS'
        except plumbum.commands.processes.ProcessExecutionError:
            result['status'] = 'FAILED'

        mv = plumbum.local['mv']
        mv['/tmp/workload.log', result["artifacts_dir"]] & plumbum.FG

    result['finish_time'] = str(datetime.now())

    if 'post_exec_cmds' in conf:
        run_cmds(conf['post_exec_cmds'], result['artifacts_dir'])

    if conf['common']['send_email']:
        send_mail(conf['common']['user'], f"finished, status {result['status']}",
                  task.id)

    if config.pack_artifacts:
        pack_artifacts(result["artifacts_dir"])

    return result


@huey.post_execute()
def post_execute_hook(task, task_value, exc):
    if exc is not None:
        print(f'Task "{task.id}" failed with error: {exc}')
    # Current task finished - do cleanup
    huey.get('current_task')
