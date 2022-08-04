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


from huey import exceptions
from config import huey
from tasks import io_workload_task
import yaml
import json
import sys
import argparse
from argparse import RawTextHelpFormatter
import signal
import os
from datetime import datetime
import validator as vr
from pprint import pprint

def print_info(tid, state, info=None):
    output = [{'task_id': tid}, {'state': state}]
    if info:
        output.append({'info': info})
    print(json.dumps(output))


def validation_failed(errors):
    print('Validation failed with the following errors:')
    pprint(errors)

def task_add():
    config = yaml.safe_load(sys.stdin.read())
    errors = vr.validate_config(config)

    if all([v for e in errors for v in e.values()]):
        validation_failed(errors)
        return

    # Additional check for cli/srv map
    if not all([n['srv'] for n in config['common']['nodes']]):
        validation_failed("Specify all server nodes : ['common']['nodes']['srv']")
        return

    if not any([n['cli'] for n in config['common']['nodes']]):
        validation_failed("Specify at least one client node : ['common']['nodes']['cli']")
        return

    opt  = { 'enqueue_time': str(datetime.now()) }
    task_priority = config['common']['priority']
    task = io_workload_task((config, opt), task_priority)
    print_info(task.id, 'ENQUEUED')

def task_del(tid):
    huey.revoke_by_id(tid)
    current_task = huey.get('current_task', peek=True)
    if current_task and current_task['task_id'] == tid:
        # TODO: use pgrep or ps aux and search $script_name's PID and kill
        if 'corrupt' in current_task['args'][0]['common']['type']:
            with open('/tmp/taskq_active_pid', 'r') as f:
                pid = yaml.safe_load(f)
            os.kill(pid['active_pid'], signal.SIGTERM)
            # TODO: Handle the case when deletion is called twice
            print_info(tid, 'TERMINATING')
        else:
            os.kill(current_task['pid'], signal.SIGKILL)
            huey.get(current_task['task_id']) # get w/o peek == delete
            print_info(tid, 'ABORTED')
    elif huey.get(tid, peek=True):
        print_info(tid, 'FINISHED')
    else:
        print_info(tid, 'REVOKED')

def get_args(args, is_yaml):
    if is_yaml:
        return "\n" + yaml.dump(args)
    else:
        return args

def list_queue(is_yaml):
    task = huey.get('current_task', peek=True)
    if task:
        print_info(task['task_id'], 'RUNNING', get_args({**{'conf': task['args'][0]}, **task['args'][1]}, is_yaml))
    for task in huey.pending():
        if not huey.is_revoked(task):
            print_info(task.id, 'QUEUED', get_args({**{'conf': task.args[0][0]}, **task.args[0][1]}, is_yaml))

def list_results(is_yaml):
    passed = []
    failed = []

    for r in huey.all_results():
        if not 'r:' in r and r != 'current_task':
            try:
                dummy = huey.result(r, preserve=True)
                passed.append(r)
            except exceptions.TaskException:
                failed.append(r)

    for r in failed:
        print_info(r, 'FAILED (exception)')

    for r in sorted(passed, key = lambda t: huey.result(t, preserve=True)['finish_time']):
        print_info(r, 'FINISHED', get_args(huey.result(r, preserve=True), is_yaml))


def task_set_prio(tid, prio):
    pending = next((t for t in huey.pending()
                    if t.id == tid and not huey.is_revoked(t)), None)
    if pending:
        params = pending.args
        huey.revoke_by_id(tid)
        print_info(tid, 'REVOKED')
        params[0][0]['priority'] = prio
        task_priority = prio
        task = io_workload_task(params[0], task_priority)
        print_info(task.id, 'ENQUEUED')
    else:
        print_info(tid, 'NOT_FOUND')


def args_parse():
    description="""
    task_queue.py: front-end module for cluster usage

    task_queue.py implements persistent queue and enqueues
    performance tests requests on the hardware cluster.
    It takes YAML configuration file as input and supplies
    io_workload.sh script with configuration overrides,
    which includes Motr and Halon building options and
    configuration, m0crate workload configuration, Halon
    facts, and other configuration data.

    Task is added by calling task_queue with '-a'/'--add-task'
    parameter. Tasks are enqueued and executed in recevied
    order. List of pending and running tasks can be retrieved
    by invoking '-l'/'--list-queue' parameter. When execution
    completes, the task results will be put into persistent
    storage and could be fetched any time by calling the script
    with '-r'/'--list-results'.

    Any pending or running task can be cancelled and removed
    from pending queue by invoking '-d'/'--del-task' parameter.

    task_queue.py supports task priorities. If priority is given
    in YAML configuration file, task_queue will enqueue task
    with that priority, otherwise priority "1" will be used.
    User can change task priority by calling '-p'/'--set-prio'
    parameter. Priority can't be changed on the fly, instead
    the old task is revoked and new one with updated priority
    is created and enqueued. task_queue returns TID of the
    newly created task.

    Task lifecycle diagram:
    Non-existent -> QUEUED -> RUNNING -> FINISHED

    Task cancelling diagram:
    QUEUED/RUNNING -> REVOKED

    Priority change diagram:
    QUEUED(TID1) -> REVOKED ... non-existent -> QUEUED(TID2)"""

    parser = argparse.ArgumentParser(description=description,
                                     formatter_class=RawTextHelpFormatter)
    group = parser.add_mutually_exclusive_group(required=True)
    group.add_argument('-a', '--add-task',
                       help="Add task to the task queue",
                       action="store_true")
    group.add_argument('-d', '--del-task',
                       metavar='task_id',
                       help="Delete task from the task queue")
    group.add_argument('-p', '--set-priority',
                       nargs=2,
                       metavar=("task_id", "priority"),
                       help="Set task priority")
    group.add_argument('-l', '--list-queue',
                       help="List pending queue",
                       action="store_true")
    group.add_argument('-r', '--list-results',
                       help="List results",
                       action="store_true")
    parser.add_argument('-v', '--verbose', action='store_true',
                        help='Print task arguments in YAML representation')
    if len(sys.argv[1:]) == 0:
        parser.print_help()
        parser.exit()
    return parser.parse_args()

def main():
    args = args_parse()
    if args.add_task:
        task_add()
    elif args.del_task:
        task_del(args.del_task)
    elif args.set_priority:
        task_set_prio(args.set_priority[0],
                      int(args.set_priority[1]))
    elif args.list_queue:
        list_queue(args.verbose)
    elif args.list_results:
        list_results(args.verbose)

if __name__ == '__main__':
    main()
