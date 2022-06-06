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


import argparse

def parse_args():
    description="""
    s3_overrides.py: Apply overrides to the specified s3config.yaml
    """

    parser = argparse.ArgumentParser(description=description)
    parser.add_argument("overrides", help="Overrides string in format key=value")
    parser.add_argument("s3config", help="s3config.yaml file")

    return parser.parse_args()

def main():
    args = parse_args()

    data = []
    if args.overrides:
        with open(args.s3config, 'r') as f:
            for line in f.readlines():
                data.append(line)

        for kv in args.overrides.split(" "):
            key, value = kv.split('=')
            for idx, line in enumerate(data):
                if key in line.split(': ')[0]:
                    k = line.split(': ')[0]
                    print(f"Overriding {k} with new value: {value}, old value: {line.split(': ')[1].split('#')[0]}")
                    nl='\n'
                    data[idx] = f"{k}: {value} # Override by s3_overrides.py{nl}"

        with open(args.s3config, 'w') as f:
            for line in data:
                f.write(line)

if __name__ == '__main__':
    main()
