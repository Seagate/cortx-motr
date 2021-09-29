#!/bin/bash

set -e
#set -x

. ../helpers

DEVEL_DIR="/root/cortx"
MOTR_DIR="${DEVEL_DIR}/cortx-motr"
HARE_DIR="${DEVEL_DIR}/cortx-hare"
M0_SRC_DIR=$MOTR_DIR

CLUSTER_YAML="/var/lib/hare/cluster.yaml"
CLUSTER_YAML_BAK="${CLUSTER_YAML}_bak"

MOTR_SYSCONFIG="/etc/sysconfig/motr"
MOTR_SYSCONFIG_BAK="${MOTR_SYSCONFIG}_bak"

LNET_CONF="/etc/modprobe.d/lnet.conf"
LNET_CONF_BAK="${LNET_CONF}_bak"

PIP_CONF="/etc/pip.conf"
PIP_CONF_BAK="${PIP_CONF}_bak"

MOTR_MAKE_OPTS="-j32"
MOTR_CONFIGURE_OPTS="--enable-dtm0 --disable-altogether-mode --enable-debug --with-trace-ubuf-size=64"

CLUSTER_YAML_PATCH="${PWD}/cluster_yaml.patch"
HARE_PATCH="${PWD}/hare.patch"

MOTR_VAR_DIR="/var/motr"
M0D_DIR_COMMON="${MOTR_VAR_DIR}/m0d-0x720000000000000"
S3_VAR_DIR="/var/log/seagate/motr"
S3_DIR_COMMON="${S3_VAR_DIR}/s3server-0x720000000000000"
ADDB_DUMP_DIR="/tmp/s3it-addb-out"

S3_ADDBPLUGIN_LIB="/opt/seagate/cortx/s3/addb-plugin/libs3addbplugin.so"

S3BENCH_URL="https://github.com/Seagate/s3bench/releases/download/v2021-06-28/s3bench.2021-06-28"
S3BENCH_BIN="${PWD}/s3bench"

NO_BOOTSTRAP=0
RUN_S3BENCH=0
RUN_S3WORKLOAD=0

function repos_clone()
{
    ssh-keygen -F github.com || ssh-keyscan github.com >> ~/.ssh/known_hosts

    pushd $DEVEL_DIR

    git clone --recurse-submodules https://github.com/Seagate/cortx-motr.git
    pushd cortx-motr
    git checkout dtm0-main
    popd

    git clone https://github.com/Seagate/cortx-hare.git
    pushd cortx-hare
    git checkout dtm-0
    popd

    popd
}

function cfgs_backup()
{
    cp $CLUSTER_YAML $CLUSTER_YAML_BAK
    cp $MOTR_SYSCONFIG $MOTR_SYSCONFIG_BAK
    cp $LNET_CONF $LNET_CONF_BAK
    mv $PIP_CONF $PIP_CONF_BAK
}

function cfgs_restore()
{
    cp $CLUSTER_YAML_BAK $CLUSTER_YAML
    cp $MOTR_SYSCONFIG_BAK $MOTR_SYSCONFIG
    cp $LNET_CONF_BAK $LNET_CONF
    # Uncomment and change MOTR_DEVEL_WORKDIR_PATH in motr sysconfig
    # to use development files.
    sed -i "s|^#MOTR_DEVEL_WORKDIR_PATH=.*|MOTR_DEVEL_WORKDIR_PATH=${MOTR_DIR}|" $MOTR_SYSCONFIG
    patch $CLUSTER_YAML < $CLUSTER_YAML_PATCH
}

function motr_build()
{
    pushd $MOTR_DIR
    ./scripts/install-build-deps
    MAKE_OPTS=$MOTR_MAKE_OPTS CONFIGURE_OPTS=$MOTR_CONFIGURE_OPTS ./scripts/m0 rebuild
    popd
}

function hare_build()
{
    pushd $HARE_DIR
    patch -p1 < $HARE_PATCH
    make
    popd
}

function motr_install()
{
    pushd $MOTR_DIR
    ./scripts/install-motr-service -l
    rm -f /usr/lib64/libmotr-helpers.so.0 /usr/lib64/libmotr.so.2 /usr/lib64/libgalois-0.1.so
    ln -s "${MOTR_DIR}/helpers/.libs/libmotr-helpers.so.0" /usr/lib64/libmotr-helpers.so.0
    ln -s "${MOTR_DIR}/motr/.libs/libmotr.so.2" /usr/lib64/libmotr.so.2
    ln -s "${MOTR_DIR}/extra-libs/galois/src/.libs/libgalois-0.1.so" /usr/lib64/libgalois-0.1.so
    popd
}

function hare_install()
{
    pushd $HARE_DIR
    make devinstall    
    popd
}

function cluster_bootstrap()
{
    hctl bootstrap --mkfs $CLUSTER_YAML
}

function cluster_stop()
{
    hctl shutdown
}

function s3bench_run()
{
    local ak
    local sk
    local sgiam_pass

    # Generate password.
    sgiam_pass=$(s3cipher decrypt --data="$(s3confstore properties:///opt/seagate/cortx/auth/resources/authserver.properties getkey --key ldapLoginPW)" --key="$(s3cipher generate_key --const_key cortx)")

    # Fetch the user from ldap, copy access key (ak) and secret key (sk) from output.
    read ak sk <<< $(ldapsearch -b "ou=accesskeys,dc=s3,dc=seagate,dc=com" -x -w $sgiam_pass -D "cn=sgiamadmin,dc=seagate,dc=com" "(&(objectclass=accesskey))" | egrep "^ak:|^sk:" | awk '{ print $2; }')

    # Run s3bench.
    $S3BENCH_BIN -accessKey $ak -accessSecret "${sk}" -bucket test1 -endpoint http://127.0.0.1  -numClients 4 -numSamples 4 -objectSize 1Mb -skipCleanup
}

function addb2_dump()
{
    rm -fR "${ADDB_DUMP_DIR}"
    mkdir "${ADDB_DUMP_DIR}"

    echo "Dumping ADDB2..."

    dtm0_it_m0ds_addb2_dump "${ADDB_DUMP_DIR}" \
        "${MOTR_DIR}/utils/m0addb2dump" \
        "${M0D_DIR_COMMON}"
    dtm0_it_s3_addb2_dump  "${ADDB_DUMP_DIR}" \
        "${MOTR_DIR}/utils/m0addb2dump" \
        "${S3_DIR_COMMON}" \
        "${S3_ADDBPLUGIN_LIB}"
}

function s3workload()
{
    cluster_bootstrap

    dtm0_it_fids_get
    dtm0_it_pids_get

    s3bench_run

    cluster_stop

    addb2_dump
}

function s3setup()
{
    if [[ -e $DEVEL_DIR ]]; then
        echo "Development directory '$DEVEL_DIR' exists, exiting"
        exit 1
    fi

    mkdir $DEVEL_DIR

    repos_clone

    pcs cluster stop --force

    cfgs_backup

    rpm -e --nodeps cortx-motr cortx-hare

    motr_build
    hare_build

    motr_install
    hare_install

    cfgs_restore

    # Get s3bench.
    wget $S3BENCH_URL -O $S3BENCH_BIN
    chmod +x $S3BENCH_BIN

    systemctl start haproxy
    /bin/sh -x /opt/seagate/cortx/auth/startauth.sh &

    if [[ $NO_BOOTSTRAP == 1 ]]; then
        echo "Done."
        exit 0
    fi

    cluster_bootstrap

    if [[ $RUN_S3BENCH == 0 ]]; then
        echo "Done."
        exit 0
    fi

    dtm0_it_fids_get
    dtm0_it_pids_get

    s3bench_run

    cluster_stop

    addb2_dump
}

function main()
{
    if [[ $RUN_S3WORKLOAD -eq 0 ]]; then
        s3setup
    else
        s3workload
    fi
}

function usage()
{
    echo "Usage: $0 <option>"
    echo ""
    echo "Without option the script sets up S3 stack (means replacing of motr/hare with"
    echo "custom builds, change configs to have 3-nodes setup etc.) and bootstraps the"
    echo "cluster."
    echo "Run s3bench means: get s3bench if not done yet, run s3bench, stop the cluster"
    echo "and gather addb2 dumps."
    echo ""
    echo "Options:"
    echo "    --no-bootstrap - setup S3 stack but no bootstrap the cluster"
    echo "    --s3bench      - setup S3 stack, bootstrap the cluster and run s3bench"
    echo "    --s3workload   - bootstrap the cluster and run s3bench"
}

if [[ $# > 1 ]]; then
    usage
    exit 1
fi

HELP=0

if [[ $# == 1 ]]; then
case $1 in
    -h|--help)
            HELP=1 ;;
    --no-bootstrap)
            NO_BOOTSTRAP=1 ;;
    --s3bench)
            RUN_S3BENCH=1 ;;
    --s3workload)
            RUN_S3WORKLOAD=1 ;;
    *)
            usage
            exit 1 ;;
esac
fi

if [[ $HELP -eq 1 ]]; then
    usage
    exit 0
fi

main
