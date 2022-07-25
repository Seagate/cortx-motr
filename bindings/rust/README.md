# Motr Rust binding
The repository contains a RUST binding of CORTX Motr. It supports creating, reading and writing to a motr object.

__This guide assumes you run all the commands as root user:__ ``sudo -s``

## Get Motr sources
To use the API, you first need to compile CORTX Motr from source.

You can follow this guide to compile Motr from source: https://github.com/Seagate/cortx-motr/blob/main/doc/Quick-Start-Guide.rst

After that, please follow this guide to compile Hare and set up Motr cluster: https://github.com/Seagate/cortx-hare/blob/main/README.md

## Install RUST and Cargo

``curl --proto '=https' --tlsv1.2 -sSf https://sh.rustup.rs | sh``

Use the default installation. After that, please run ``source $HOME/.cargo/env``.

You can run ``cargo --version`` to confirm that RUST and Cargo has been successfully installed on your machine.

## Install clang

Our RUST API requires clang version higher than clang 5. To install it, run:

```shell
sudo yum install -y centos-release-scl 
sudo yum install -y llvm-toolset-7.0
source /opt/rh/llvm-toolset-7.0/enable
```

You can check clang version by running ``clang --version``. Now ``clang version 7.0.1`` should have been installed.

## Build Motr Rust API

```shell
cd /root/rust_motr
cargo build
```

## Test Motr Rust API

The tests are written in ``src/client.rs``. Before running the tests, you first need to edit ``src/client.rs`` modify the cluster parameters in the tests.
(In ``src/client.rs``, search for ``ha_addr_str``)

For example, here is my cluster parameters after running ``hctl status``:

```shell
[root@motr rust_motr]# hctl status
Bytecount:
    critical : 0
    damaged : 0
    degraded : 0
    healthy : 0
Data pool:
    # fid name
    0x6f00000000000001:0x0 'the pool'
Profile:
    # fid name: pool(s)
    0x7000000000000001:0x0 'default': 'the pool' None None
Services:
    localhost  (RC)
    [started]  hax                 0x7200000000000001:0x0          inet:tcp:10.140.82.80@22001
    [started]  confd               0x7200000000000001:0x1          inet:tcp:10.140.82.80@21002
    [started]  ioservice           0x7200000000000001:0x2          inet:tcp:10.140.82.80@21003
    [unknown]  m0_client_other     0x7200000000000001:0x3          inet:tcp:10.140.82.80@22501
    [unknown]  m0_client_other     0x7200000000000001:0x4          inet:tcp:10.140.82.80@22502
```

Therefore, I will modify the cluster parameters in ``src/client.rs`` into:

```Rust
    static ha_addr_str: &str = "inet:tcp:10.140.82.80@22001";
    static local_addr_str: &str = "inet:tcp:10.140.82.80@22501";
    static profile_fid_str: &str = "0x7000000000000001:0x0";
    static process_fid_str: &str = "0x7200000000000001:0x3";
```

Then you can run the tests.

You can first test creating an object in CORTX Motr:

``cargo test client::tests::test_create -- --nocapture``

You can then test writing data to the object which you have just created:

``cargo test client::tests::test_write -- --nocapture``

This will write 8192 "X" characters to the object. The API by default uses 4KB as block size, so the 8192 char string will be writen into 2 blocks in CORTX.

You can then test reading data:

``cargo test client::tests::test_read -- --nocapture``

This will read 5000 characters from the object which you have just written to, starting from offset 400. The read data is outputed to a file ``temp.txt`` in your current folder. You can check that file and see its size is 5000 Bytes and contains 5000 "X" characters.

## Tested By

July 25, 2022: Meng Wang (mengwanguc@gmail.com) in CentOS 7.9.2009 using motr commit 9bd5714463fe404bdff5041c5e47e59d0b22a620.