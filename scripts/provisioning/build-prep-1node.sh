#!/bin/sh

# Clone, build and prepare to start a single-node cluster.

set -eu -o pipefail

[[ -d cortx-motr ]] ||
    git clone --recurse https://github.com/Seagate/cortx-motr.git &&
        ln -s cortx-motr motr
cd motr
echo 'Building and installing Motr...'
./autogen.sh && ./configure --disable-expensive-checks && make -j4 &&
    ./scripts/install-motr-service
cd -

[[ -d cortx-hare ]] ||
    git clone --recurse https://github.com/Seagate/cortx-hare.git &&
        ln -s cortx-hare hare
cd hare
echo 'Building and installing Hare...'
make && make devinstall
cd -

echo 'Creating block devices...'
sudo mkdir -p /var/motr
for i in {0..9}; do
    sudo dd if=/dev/zero of=/var/motr/disk$i.img bs=1M seek=9999 count=1
    sudo losetup /dev/loop$i /var/motr/disk$i.img
done

echo 'Preparing CDF (Cluster Description File)...'
[[ -f singlenode.yaml ]] || cp hare/cfgen/examples/singlenode.yaml ./
sed 's/localhost/cmu/' -i singlenode.yaml
sed 's/data_iface: eth./data_iface: eth0/' -i singlenode.yaml

echo
echo 'Now you are ready to start the singlenode Motr cluster!'
echo 'To start run: hctl bootstrap --mkfs singlenode.yaml'
echo 'To check:     hctl status'
echo 'To shutdown:  hctl shutdown'
