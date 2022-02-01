#!/bin/sh
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
#
# Clone, build and prepare to start a single-node cluster.
#
# Usage: ./build-prep-1node.sh [-dev] [-pkg]
#
#   -dev  development mode, don't build and install rpms
#   -pkg  install packages without using ansible
#
set -e -o pipefail
#set -x
dev_mode=
pkg=

[[ $1 == "-dev" ]] && dev_mode='yes'
[[ $1 == "-pkg" || $2 == "-pkg" ]] && pkg='yes'

which git || sudo yum install -y git

[[ -f scripts/$(basename "$0") ]] || [[ -d cortx-motr ]] || {
    git clone --recurse https://github.com/Seagate/cortx-motr.git &&
        ln -s cortx-motr motr
}

echo 'Install Motr deps...'
[[ -f scripts/$(basename "$0") ]] || cd motr
if [[ $pkg == "yes" ]]; then
    sudo scripts/install-build-deps --no-ansible
else
    sudo scripts/install-build-deps
fi

echo 'Configure Motr...'
[[ -f configure ]] && sudo git clean -dfx
./autogen.sh && ./configure --disable-expensive-checks --with-user-mode-only
if [[ $dev_mode ]]; then
    echo 'Build Motr...'
    make -j4
    echo 'Install Motr from sources...'
    sudo ./scripts/install-motr-service
else
    echo 'Build Motr rpms...'
    make rpms
    echo 'Install Motr from rpms...'
    ls -t ~/rpmbuild/RPMS/$(arch)/cortx-motr{,-devel,-debuginfo}-2* | head -3 |
        xargs sudo rpm -U --force --nodeps
fi
cd ..

echo 'Install Hare deps...'

rpm -q consul || {
    echo 'Install Consul...'
    sudo yum-config-manager --add-repo \
        https://rpm.releases.hashicorp.com/RHEL/hashicorp.repo
    sudo yum install -y consul-1.9.10
}

[[ -d cortx-utils ]] || {
    git clone --recurse https://github.com/Seagate/cortx-utils.git
}
cd cortx-utils
echo 'Build CORTX py-utils...'
sudo pip3 install -r py-utils/python_requirements.txt
sudo pip3 install -r py-utils/python_requirements.ext.txt
./jenkins/build.sh -v 2.0.0 -b 3
echo 'Install CORTX py-utils...'
sudo rpm -U --force --nodeps py-utils/dist/cortx-py-utils-2.0.0-3*.rpm
cd -

[[ -d cortx-hare ]] || {
    git clone --recurse https://github.com/Seagate/cortx-hare.git &&
        ln -s cortx-hare hare
}

echo 'Install facter version >= 3.14...'
source /etc/os-release
case "$ID" in
    rocky|centos|rhel|ol)
        MAJOR_VERSION="$(echo $VERSION_ID | cut -d. -f1)"
        if [[ "$MAJOR_VERSION" == 7 ]]; then
            sudo yum localinstall -y https://yum.puppetlabs.com/puppet/el/7/$(arch)/puppet-agent-7.0.0-1.el7.$(arch).rpm
            sudo ln -sf /opt/puppetlabs/bin/facter /usr/bin/facter
        elif [[ "$MAJOR_VERSION" == 8 ]]; then
            sudo dnf install -y facter
        else
            echo "Unknown rhel major version: $MAJOR_VERSION"
            exit 1
        fi
        ;;
    *)
        echo "$ID is unknown"
        exit 1
        ;;
esac

cd hare
echo 'Build Hare...'
[[ -d .mypy_cache ]] && sudo git clean -dfx
make
if [[ $dev_mode ]]; then
    echo 'Install Hare from sources...'
    sudo make devinstall
else
    echo 'Build Hare rpms...'
    make rpm
    echo 'Install Hare from rpms...'
    sudo rm -rf /opt/seagate/cortx/hare
    ls -t ~/rpmbuild/RPMS/$(arch)/cortx-hare{,-debuginfo}-2* | head -2 |
        xargs sudo rpm -U --force
fi
cd -

echo 'Creating block devices...'
sudo mkdir -p /var/motr
for i in {0..9}; do
    sudo dd if=/dev/zero of=/var/motr/disk$i.img bs=1M seek=9999 count=1
    sudo losetup /dev/loop$i /var/motr/disk$i.img
done

IFNAME=$(ip a | grep ^2 | awk -F': ' '{print $2}')
IP=$(ip a show "$IFNAME" | grep 'inet ' | awk '{print $2}' | sed 's;/.*;;')

echo 'Updating /etc/hosts...'
sudo sed "/$(hostname)/d" -i /etc/hosts
sudo sed "\$a $IP $(hostname)" -i /etc/hosts

echo 'Preparing CDF (Cluster Description File)...'
[[ -f singlenode.yaml ]] || cp hare/cfgen/examples/singlenode.yaml ./
sed "s/localhost/$(hostname)/" -i singlenode.yaml
sed "s/data_iface: eth./data_iface: $IFNAME/" -i singlenode.yaml

echo
echo 'Now you are ready to start the singlenode Motr cluster!'
echo 'To start run: hctl bootstrap --mkfs singlenode.yaml'
echo 'To check:     hctl status'
echo 'To shutdown:  hctl shutdown'
echo
echo 'See also: hctl help'
