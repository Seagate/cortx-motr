Using HSM
=========

HSM stands for Hierarchical Storage Management. The concept and design are discussed
in this paper: [Hierarchical Storage Management - SAGE 2016](https://github.com/Seagate/cortx/blob/main/doc/PDFs/2016_February_SAGE_WP3_HSM_for_SAGE_Concept_and_Architecture_v1_for_open_use_compressed.pdf).


The m0hsm tool available in this directory allows to
create composite objects in Motr, write/read to/from them and move them
between the tiers (pools). Here is how to use the tool:

1. Set the following environment variables:

   ```bash
   export CLIENT_PROFILE="<0x7000000000000001:0x480>"    # profile id
   export CLIENT_HA_ADDR="172.18.1.24@o2ib:12345:34:101" # ha-agent address
   export CLIENT_LADDR="172.18.1.24@o2ib:12345:41:322"   # local address
   export CLIENT_PROC_FID="<0x7200000000000001:0xdf>"    # process id
   ```

   Profile id of the cluster and ha-agent address on your client node can
   be checked with `hctl status` command. As well as all addresses and
   processes ids configured in the cluster. Consult with the cluster system
   administrator about which of them you can use.

2. Initialize the composite layout index:

   ```Text
   $ m0composite "$CLIENT_LADDR" "$CLIENT_HA_ADDR" "$CLIENT_PROFILE" "$CLIENT_PROC_FID"
   ```

   Note: this should be done one time only after the cluster bootstrap.

3. Configure pools ids of the tiers in ~/.hsm/config file:

   ```Text
   M0_POOL_TIER1 = <0x6f00000000000001:0xc74> # NVME
   M0_POOL_TIER2 = <0x6f00000000000001:0xc8a> # SSDs
   M0_POOL_TIER3 = <0x6f00000000000001:0xca5> # HDDs
   ```

   The exact ids can be taken from the output of `hctl status` command.

Now you are ready to use the HSM feature.

First test using m0hsm shell:

```Text
$ ./m0hsm shell
```

Type `help` for the list of available commands and their syntax:

```Text
m0hsm> help
Usage: m0hsm <action> <fid> [...]
  actions:
    create <fid> <tier>
    show <fid>
    dump <fid>
    write <fid> <offset> <len> <seed>
    write_file <fid> <path>
    read <fid> <offset> <len>
    copy <fid> <offset> <len> <src_tier> <tgt_tier> [options: mv,keep_prev,w2dest]
    move <fid> <offset> <len> <src_tier> <tgt_tier> [options: keep_prev,w2dest]
    stage <fid> <offset> <len> <tgt_tier> [options: mv,w2dest]
    archive <fid> <offset> <len> <tgt_tier> [options: mv,keep_prev,w2dest]
    release <fid> <offset> <len> <tier> [options: keep_latest]
    multi_release <fid> <offset> <len> <max_tier> [options: keep_latest]
    set_write_tier <fid> <tier>

  <fid> parameter format is [hi:]lo. (hi == 0 if not specified.)
  The numbers are read in decimal, hexadecimal (when prefixed with `0x')
  or octal (when prefixed with `0') formats.
```

Create an object on tier 2: `create <fid> <tier_idx>`

```Text
m0hsm> create 0x1000000 2
Composite object successfully created with id=0:0x1000000
```

Display initial state: `show <fid>`

```Text
m0hsm> show 0x1000000
- gen 0, tier 2, extents:  (writable)
```

Write data to it: `write <fid> <offset> <length> <seed>`

```Text
m0hsm> write 0x1000000 0x0 0x1000  42
4096 bytes successfully written at offset 0 (object id=0:0x1000000)
```

HSM in action: move data from tier 2 to tier 3:
`move <fid> <offset> <length> <src_tier> <tgt_tier> [options]`

```Text
m0hsm> move 0x1000000 0 0xFFFF 2 3
Archiving extent [0-0xfff] (gen 0) from tier 2 to tier 3
4096 bytes successfully copied from subobj <0xffffff02:0x1000000> to <0xffffff03:0x1000000> at offset 0
Extent [0-0xfff] (gen 0) successfully released from tier 2
```

Check final state:

```Text
m0hsm> show 0x1000000
- gen 1, tier 2, extents:  (writable)
- gen 0, tier 3, extents: [0->0xfff]
```
