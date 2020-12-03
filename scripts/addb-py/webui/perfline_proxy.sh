#!/usr/bin/env bash

PL_SCRIPT_PATH=$(echo 'import config; print(config.perfline_script_path)' | python3)
PL_DIR="${PL_SCRIPT_PATH%/*}"

pushd $PL_DIR > /dev/null
$PL_SCRIPT_PATH $@
popd > /dev/null