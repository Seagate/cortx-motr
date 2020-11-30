==========
Overview
==========

The overarching goal for motr configuration is ease of use. This manifests as autodiscovery, autotuning, and alerts.

************************
Functional Description
************************

**Autodiscovery**: Neo hardware has primitives for querying available hardware. As Motr services start up we can get a detailed list of attached hardware and network settings (e.g. ip addr). Additionally, we assume if devices are added “hot” we will get a notification from the OS (e.g. new raid disk inserted). Items to autodiscover: disk uuids, nics, node metadata (id, rack location, etc). Attached disks not in disk.exclude list (see below) are scanned for M0 storage containers so container IDs are discovered as well.

**Autotuning**: Motr software has QOS priority concepts; these may need to tie in to hardware drivers / stats counters. Also, different sets of defaults might be set for various “regimes” of operation: if a node “looks” like it has the drive / hardware configuration to be e.g. an MDS, it should have an MDS-like set of defaults (profile).

**Alerts**: if something happens that Motr doesn’t have a clear resolution, an alert should be raised to the sysadmin via Trinity. This does not include e.g. standard recovery scenarios – alerts should be kept to a minimum. (System monitoring falls outside the scope of configuration.) Events that might cause data loss (“I see a new drive has been added to my raid group. I see no Motr data on it currently, but it does appear to have been formatted in the past. Shall I erase this disk and use it as additional Motr storage?”), or a decision that needs to be made by a human (“A new empty drive has been added, shall I add it to group X or group Y?).



