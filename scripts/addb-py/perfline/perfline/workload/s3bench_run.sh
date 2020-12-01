#!/usr/bin/env bash
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

set -x

NUM_CLIENTS=128
OBJ_SIZE="128m"
NUM_SAMPLES=1024
BUCKET_NAME="bucket0"
FILE=

function parse_args()
{
    while [[ $# -gt 0 ]]; do
        case $1 in
            -a|--access-key)
                ACCESS_KEY="$2"
                shift
                ;;
            -s|--secret-key)
                SECRET_KEY="$2"
                shift
                ;;
            -b|--bucket)
                BUCKET_NAME="$2"
                shift
                ;;
            -n|--num-samples)
                NUM_SAMPLES="$2"
                shift
                ;;
            -c|--num-clients)
                NUM_CLIENTS="$2"
                shift
                ;;
            -o|--object-size)
                OBJ_SIZE="$2"
                shift
                ;;
            -f|--dump-file)
                FILE="$2"
                shift
                ;;
            -h|--help)
                echo "help information"
                exit 0
                ;;
            *)
                echo "unknown parameter: $1"
                exit 1
                ;;
        esac
        shift
    done
}

function parse_obj_size()
{
    local size_val_fmt='[0-9]+'
    local size_unit_fmt='k|m|g'
    local fmt="^$size_val_fmt($size_unit_fmt)\$"

    if ! echo "$OBJ_SIZE" | grep -P "$fmt"; then
        echo "invalid object size format"
        exit 1
    fi

    local size_val=$(echo "$OBJ_SIZE" | grep -P -o "$size_val_fmt")
    local size_unit=$(echo "$OBJ_SIZE" | grep -P -o "$size_unit_fmt")

    case $size_unit in
        k)
            OBJ_SIZE_BYTES=$((1024*$size_val))
            ;;
        m)
            OBJ_SIZE_BYTES=$((1024*1024*$size_val))
            ;;
        g)
            OBJ_SIZE_BYTES=$((1024*1024*1024*$size_val))
            ;;
        *)
            echo "invalid object size unit"
            exit 1
            ;;
    esac
    echo "OBJ_SIZE_BYTES: $OBJ_SIZE_BYTES"
}

function parse_creds()
{
    if [[ -n "$ACCESS_KEY" && -n "$SECRET_KEY" ]]; then
        return 0
    fi

    ACCESS_KEY=$(cat ~/.aws/credentials | grep aws_access_key_id | cut -d= -f2)
    SECRET_KEY=$(cat ~/.aws/credentials | grep aws_secret_access_key | cut -d= -f2)
}

function create_bucket()
{
    mkdir -p s3bench_test
    aws s3 mb s3://$BUCKET_NAME || true
}

function run_s3bench()
{
    if [[ -z $FILE ]]; then
	FILE="s3bench_workload_${NUM_SAMPLES}_${num_size}${num_size_units}.log"
    fi
    
    s3bench -accessKey $ACCESS_KEY -accessSecret $SECRET_KEY \
        -bucket $BUCKET_NAME -endpoint http://s3.seagate.com -numClients $NUM_CLIENTS \
        -numSamples $NUM_SAMPLES -objectNamePrefix=s3workload -objectSize $OBJ_SIZE_BYTES \
        -verbose | tee s3bench_test/${FILE}
}

parse_args $@
parse_obj_size
parse_creds
create_bucket
run_s3bench
