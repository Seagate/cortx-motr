#!/usr/bin/env python3

import json
import sys
import re
import os
from collections import namedtuple
import json

ROUNDING_PRECISION = 4 # number of digit after decimal point
SUM_COLUMN = 'sum' # name of column
S3BENCH_LOG_NAME_PATTERN = '^s3bench-(?P<hostname>.*).log$'
M0CARTE_LOG_NAME_PATTERN = '^m0crate.(?P<hostname>.*).log$'

# 'name' is optional. It is used for convert original field name to a shorter
# 'sum' option is used to mark fields which should be summed from all nodes
# 'aggr' option is used to mark fields which should be printed in summary table
FIELDS = {'numClients' : {'name' : "nCli", 'opts' : ['aggr']},
          'numSamples' : {'name' : "nSamp", 'opts' : ['aggr']},
          'objectSize (MB)' : {'name' : 'obj_s MB', 'opts' : ['aggr']},
          'Total Throughput (MB/s)' : {'name' : 'Thr', 'opts' : ['aggr', 'sum']},
          'Total Transferred (MB)' : {'name' : 'Size', 'opts' : ['sum']},
          'Ttfb 99th-ile' : {'name' : 'ttfb-99'},
          'Ttfb 90th-ile' : {'name' : 'ttfb-90'},
          'Ttfb Max' : {'name' : 'ttfb-max'},
          'Ttfb Min' : {'name' : 'ttfb-min'},
          'Ttfb Avg' : {'name' : 'ttfb-avg'},
          'Total Duration (s)' : {'name' : 'duration'},
          'Total Requests Count' : {'name' : 'request_nr', 'opts' : ['sum'] },
          'Errors Count' : {'name' : 'errors', 'opts' : ['sum']}
          }

def is_s3bench_log(file_path):
    path_parts = file_path.split('/')
    p = re.compile(S3BENCH_LOG_NAME_PATTERN)
    return p.match(path_parts[-1]) != None

def is_m0crate_log(file_path):
    path_parts = file_path.split('/')
    p = re.compile(M0CARTE_LOG_NAME_PATTERN)
    return p.match(path_parts[-1]) != None

def parse_hostname(file_path):
    path_parts = file_path.split('/')
    p = re.compile(S3BENCH_LOG_NAME_PATTERN)
    match_res = p.match(path_parts[-1])
    return match_res.group('hostname')

def parse_s3bench_log(file_path):
    # print(file_path)
    with open(file_path) as f:
        data = json.loads(f.read())
    return data

def get_visible_field_name(raw_field_name):
    field_desc = FIELDS[raw_field_name]
    if 'name' in field_desc:
        return field_desc['name']
    else:
        return raw_field_name

Measurement = namedtuple('Measurement' , 'measurement_name operation')

class run_results:
    def __init__(self, task_id, time_marker):
        self.task_id = task_id
        self.time_marker = time_marker
        self.params = dict()
        self.measurements = dict()
        self._hostnames = list()

    def get_s3bench_params(self):
        result = dict()
        for param_name, param_values in self.params.items():
            shared_param_value = None
            for cli_hostname, val in param_values.items():

                # skip aggregated values (such as 'sum')
                if cli_hostname not in self._hostnames:
                    continue

                if shared_param_value is None:
                    shared_param_value = val
                elif shared_param_value != val:
                    raise Exception("Found different s3bench parameters during the test run")

            result[param_name] = shared_param_value
        return result

    def put_data(self, cli_hostname, raw_data):
        self._hostnames.append(cli_hostname)
        self._put_params(cli_hostname, raw_data)
        self._put_measurements(cli_hostname, raw_data)

    def _put_params(self, cli_hostname, raw_data):
        for k, v in raw_data['Parameters'].items():
            if k in FIELDS:
                if k not in self.params:
                    self.params[k] = dict()

                row = self.params[k]
                row[cli_hostname] = v

    def _put_measurements(self, cli_hostname, raw_data):
        for test_results in raw_data['Tests']:
            operation_name = test_results['Operation']

            # print(operation_name)
            if operation_name == 'Read':
                operation_name = 'R'
            elif operation_name == 'Write':
                operation_name = 'W'
            elif operation_name == 'PutObjTag':
                operation_name = 'P'
            elif operation_name == 'GetObjTag':
                operation_name = 'G'
            elif operation_name == 'HeadObj':
                operation_name = 'H'
            else:
                raise Exception("unexpected operation name")

            for measure_name, measure_val in test_results.items():
                if measure_name in FIELDS:
                    measurement = Measurement(measure_name, operation_name)
                    if measurement not in self.measurements:
                        self.measurements[measurement] = dict()

                    row = self.measurements[measurement]
                    row[cli_hostname] = measure_val

                    if field_required_calc_sum(measure_name):
                        if SUM_COLUMN not in row:
                            row[SUM_COLUMN] = 0
                        row[SUM_COLUMN] += measure_val

def cut_task_id(task_id):
    return "{0}***{1}".format(task_id[0:4], task_id[-4:])

def field_has_option(field_name, opt_name):
    field_desc = FIELDS[field_name]

    if 'opts' in field_desc:
        return opt_name in field_desc['opts']
    else:
        return False

def is_field_for_aggr_table(field_name):
    return field_has_option(field_name, 'aggr')

def field_required_calc_sum(field_name):
    return field_has_option(field_name, 'sum')


class json_aggr_result:
    def __init__(self):
        # self._run_results = run_results_data
        self._aggr_result = dict()

    def add_data(self, run_result_data):
        task_id = run_result_data.task_id
        time_marker = run_result_data.time_marker

        if task_id not in self._aggr_result:
            self._aggr_result[task_id] = dict()

        task_data = self._aggr_result[task_id]
        task_data[time_marker] = dict()
        run_data = task_data[time_marker]

        for measur, measur_vals in run_result_data.measurements.items():
            if SUM_COLUMN in measur_vals:
                measur_name = "{0}({1})".format(
                    get_visible_field_name(measur.measurement_name),
                    measur.operation)
                run_data[measur_name] = measur_vals[SUM_COLUMN]

    def print(self):
        print(json.dumps(self._aggr_result))

    def get_result(self):
        return self._aggr_result


def find_s3bench_logs(path_to_artifacts_dir):
    s3bench_dirs = dict()
    for item in os.walk(path_to_artifacts_dir):
        all_files = item[2]
        s3bench_files = list(filter(is_s3bench_log, all_files))
        if len(s3bench_files) > 0:
            dir_path = item[0]

            # transform each file_name to file_path
            s3bench_files = list(map(lambda f: "{0}/{1}".format(dir_path, f),
                                            s3bench_files))
            s3bench_dirs[dir_path] = s3bench_files
    return s3bench_dirs

def find_m0crate_logs(path_to_artifacts_dir):
    m0crate_dirs = dict()
    for item in os.walk(path_to_artifacts_dir):
        all_files = item[2]
        m0crate_files = list(filter(is_m0crate_log, all_files))
        if len(m0crate_files) > 0:
            dir_path = item[0]

            # transform each file_name to file_path
            m0crate_files = list(map(lambda f: "{0}/{1}".format(dir_path, f),
                                            m0crate_files))
            m0crate_dirs[dir_path] = m0crate_files
    return m0crate_dirs



def process_run_results(s3bench_log_list, task_id, time_marker):
    dt = run_results(task_id, time_marker)

    # processing data from several s3bench log files
    # related to the same test run
    for s3bench_log in s3bench_log_list:
        cli_hostname = parse_hostname(s3bench_log)
        data_from_file = parse_s3bench_log(s3bench_log)
        dt.put_data(cli_hostname, data_from_file)

    return dt

def parse_perf_results(task_id, path_to_artifacts):
    # find s3bench logs and group them by test runs
    s3bench_dirs = find_s3bench_logs(path_to_artifacts)
    run_result_list = list()

    for directory_path, s3bench_logs in s3bench_dirs.items():
        directory_name = directory_path.split('/')[-1]

        #search for time marker
        time_marker_end = directory_name.find('-')
        assert time_marker_end != -1
        time_marker = directory_name[0:time_marker_end]

        run_result_list.append(process_run_results(s3bench_logs, task_id, time_marker))

    j_aggr = json_aggr_result()
    for dt in run_result_list:
        j_aggr.add_data(dt)
    return j_aggr.get_result()

def parse_m0crate_perf_results(task_id, path_to_artifacts):
    m0crate_dirs = find_m0crate_logs(path_to_artifacts)
    result = dict()
    result[task_id] = dict()

    for directory_path, m0crate_logs in m0crate_dirs.items():
        directory_name = directory_path.split('/')[-1]

        #search for time marker
        time_marker_end = directory_name.find('-')
        assert time_marker_end != -1
        time_marker = directory_name[0:time_marker_end]

        result[task_id][time_marker] = {'read_perf': 0, 'write_perf': 0}
        run_result = result[task_id][time_marker]

        for fff in m0crate_logs:
            node_run_result = parse_m0crate_log(fff)

            if 'read_perf' in node_run_result:
                run_result['read_perf'] += node_run_result['read_perf']
            
            if 'write_perf' in node_run_result:
                run_result['write_perf'] += node_run_result['write_perf']

    return result


def parse_m0crate_log(m0crate_log_path):
    read_re = re.compile('.*info:\sR:\s\[.*]\s\(.*\),\s(?P<read_s>\d+)\sKiB,\s(?P<read_p>\d+)\sKiB\/s$')
    write_re = re.compile('.*info:\sW:\s\[.*]\s\(.*\),\s(?P<write_s>\d+)\sKiB,\s(?P<write_p>\d+)\sKiB\/s$')

    result_data = dict()

    with open(m0crate_log_path) as f:
        for line in f:
            m_res = read_re.match(line)
            if m_res is not None:
                result_data['read_size'] = int(m_res.group('read_s'))
                result_data['read_perf'] = int(m_res.group('read_p'))

            m_res = write_re.match(line)
            if m_res is not None:
                result_data['write_size'] = int(m_res.group('write_s'))
                result_data['write_perf'] = int(m_res.group('write_p'))

    return result_data

