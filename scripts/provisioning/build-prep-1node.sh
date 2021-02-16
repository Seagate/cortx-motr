#!/bin/sh

# Clone, build and prepare to start a single-node cluster.
#
# Usage: ./build-prep-1node.sh [-dev]
#
#   -dev  development mode, don't build and install rpms
#
set -e -o pipefail

dev_mode=

[[ $1 == "-dev" ]] && dev_mode='yes'

[[ -d cortx-motr ]] || {
    git clone --recurse https://github.com/Seagate/cortx-motr.git &&
        ln -s cortx-motr motr
}
cd motr
echo 'Configure Motr...'
[[ -f configure ]] && git clean -dfx
./autogen.sh && ./configure --disable-expensive-checks
if [[ $dev_mode ]]; then
    echo 'Build Motr...'
    make -j4
    echo 'Install Motr from sources...'
    sudo ./scripts/install-motr-service
else
    echo 'Build Motr rpms...'
    make rpms-notests
    echo 'Install Motr from rpms...'
    ls -t ~/rpmbuild/RPMS/x86_64/cortx-motr{,-devel,-debuginfo}-1* | head -3 |
        xargs sudo rpm -i --force
fi
cd -

rpm -q consul || {
    echo 'Install Consul...'
    sudo yum-config-manager --add-repo \
        https://rpm.releases.hashicorp.com/RHEL/hashicorp.repo
    sudo yum install -y consul-1.7.8
}

[[ -d cortx-hare ]] || {
    git clone --recurse https://github.com/Seagate/cortx-hare.git &&
        ln -s cortx-hare hare
}
cd hare
echo 'Build Hare...'
[[ -d .mypy_cache ]] && git clean -dfx
make
if [[ $dev_mode ]]; then
    echo 'Install Hare from sources...'
    sudo make devinstall
else
    echo 'Build Hare rpms...'
    make rpm
    echo 'Install Hare from rpms...'
    sudo rm -rf /opt/seagate/cortx/hare
    ls -t ~/rpmbuild/RPMS/x86_64/cortx-hare{,-debuginfo}-1* | head -2 |
        xargs sudo rpm -i --force
fi
cd -

echo 'Creating block devices...'
sudo mkdir -p /var/motr
for i in {0..9}; do
    sudo dd if=/dev/zero of=/var/motr/disk$i.img bs=1M seek=9999 count=1
    sudo losetup /dev/loop$i /var/motr/disk$i.img
done

echo 'Preparing CDF (Cluster Description File)...'
[[ -f singlenode.yaml ]] || cp hare/cfgen/examples/singlenode.yaml ./
sed "s/localhost/$(hostname)/" -i singlenode.yaml
sed 's/data_iface: eth./data_iface: eth0/' -i singlenode.yaml

echo
echo 'Now you are ready to start the singlenode Motr cluster!'
echo 'To start run: hctl bootstrap --mkfs singlenode.yaml'
echo 'To check:     hctl status'
echo 'To shutdown:  hctl shutdown'
