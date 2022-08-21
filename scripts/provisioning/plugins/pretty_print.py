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

# Ansible plugin for pretty-printing playbook tasks output

from __future__ import (absolute_import, division, print_function)
from ansible.plugins.callback import CallbackBase
import json
__metaclass__ = type

RECORDS = (
    #'failed',
    'msg',
    'reason',
    'results',
    'stderr',
    'stdout',
)

class CallbackModule(CallbackBase):


    CALLBACK_NAME = 'pretty_print'
    CALLBACK_TYPE = 'notification'
    CALLBACK_VERSION = 2.0
    CALLBACK_NEEDS_WHITELIST = False

    #
    # helpers ------------------------------
    #

    def pretty_print(self, data):
        if isinstance(data, dict):
            for rec in RECORDS:
                no_log = data.get('_ansible_no_log', False)
                if rec in data and data[rec] and not no_log:
                    output = self._format(data[rec]).replace("\\n", "\n")
                    self._display.display("{0}:  {1}".format(rec, output),
                                          log_only=False)

    def _format(self, output):
        if isinstance(output, dict):
            return json.dumps(output, indent=2, sort_keys=True)

        # output may contain nested results when a task uses 'with_items'
        if isinstance(output, list) and isinstance(output[0], dict):
            formatted_output = []
            for _, elem in enumerate(output):
                if isinstance(elem, dict):
                    for rec in set(RECORDS) & set(elem):
                        formatted_output.append( self._format(elem[rec]) )
            if len(formatted_output) == 1:
                return formatted_output[0]
            else:
                return '\n  ' + '\n  '.join(formatted_output)

        if isinstance(output, list) and not isinstance(output[0], dict):
            if len(output) == 1:
                return output[0]
            else:
                return '\n  ' + '\n  '.join(output)

        return str(output)

    #
    # V1 methods ---------------------------
    #

    def runner_on_async_failed(self, host, res, jid):
        self.pretty_print(res)

    def runner_on_async_ok(self, host, res, jid):
        self.pretty_print(res)

    def runner_on_async_poll(self, host, res, jid, clock):
        self.pretty_print(res)

    def runner_on_failed(self, host, res, ignore_errors=False):
        self.pretty_print(res)

    def runner_on_ok(self, host, res):
        self.pretty_print(res)

    def runner_on_unreachable(self, host, res):
        self.pretty_print(res)

    #
    # V2 methods ---------------------------
    #

    def v2_runner_on_async_failed(self, result):
        self.pretty_print(result._result)

    def v2_runner_on_async_ok(self, host, result):
        self.pretty_print(result._result)

    def v2_runner_on_async_poll(self, result):
        self.pretty_print(result._result)

    def v2_runner_on_failed(self, result, ignore_errors=False):
        self.pretty_print(result._result)

    def v2_runner_on_ok(self, result):
        self.pretty_print(result._result)

    def v2_runner_on_unreachable(self, result):
        self.pretty_print(result._result)
