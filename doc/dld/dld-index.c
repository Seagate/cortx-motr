/*
 * Copyright (c) 2020 Seagate Technology LLC and/or its Affiliates
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *
 * For any questions about this software or licensing,
 * please email opensource@seagate.com or cortx-questions@seagate.com.
 *
 */

/**

<!-- Note that the extra-long lines cannot be helped here -->

@page DLDIX Detailed Designs

Detailed designs, ordered alphabetically by title:

- @subpage NetRQProvDLD "Auto-Provisioning of Receive Message Queue Buffers DLD" <!-- net/tm_provision.c -->
- @subpage CMDLD "Copy Machine DLD" <!-- cm/cm.c -->
- @subpage CPDLD "Copy Packet DLD"  <!-- cm/cp.c -->
- @subpage conf "DLD of configuration caching" <!-- conf/obj.c -->
- @subpage DLD_conf_schema "DLD for configuration schema" <!-- conf/schema.h -->
- @subpage DLD-pools-in-conf-schema "DLD of Pool in Configuration Schema" <! -- conf/objs/pool.c -->
- @subpage conf-diter-dld "DLD of configuration directory iterator" <! -- conf/diter.c -->
- @subpage dtm "Distributed transaction manager" <!-- dtm/dtm.h -->
- @subpage data_integrity "Data integrity using checksum" <!-- lib/checksum.h -->
- @subpage m0_long_lock-dld "FOM Long lock DLD" <!-- fop/fom_long_lock.h -->
- <i>I/O Related</i>
  - @subpage DLD-bulk-server "DLD of Bulk Server" <!-- ioservice/io_foms.c -->
  - @subpage rmw_io_dld "DLD for read-modify-write IO requests" <!-- m0t1fs/linux_kernel/file_internal.h -->
  - @subpage io_bulk_client "IO bulk transfer Detailed Level Design" <!-- ioservice/io_fops.c -->
  - @subpage IOFOLDLD "IO FOL DLD" <!-- ioservice/io_fops.c -->
  - @subpage iosnsrepair "I/O with SNS and SNS repair" <!-- m0t1fs/linux_kernel/
file.c -->
  - @subpage SNSCMDLD "SNS copy machine DLD" <!-- sns/cm/cm.c -->
  - @subpage DIXCMDLD "DIX copy machine DLD" <!-- dix/cm/cm.c -->
- @subpage Layout-DB "Layout DB DLD" <!-- layout/layout_db.c -->
- @subpage LNetDLD "LNet Transport DLD" <!-- net/lnet/lnet_main.c -->
- @subpage net-test "Motr Network Benchmark" <!-- net/test/main.c -->
- @subpage m0t1fs "M0T1FS detailed level design specification" <!-- m0t1fs/linux_kernel/m0t1fs.h -->
- @subpage rpc-layer-core-dld "RPC layer core DLD" <!-- rpc/rpc.h -->
- @subpage spiel-dld "SPIEL API DLD" <!-- spiel/spiel.h -->
- @subpage cas-dld "The catalogue service (CAS)" <!-- cas/service.c -->
- @subpage fis-dld "Fault Injection at run time" <!-- fis/fi_service.h -->
- @subpage dtm0br-dld "DTM0 basic recovery SM" <!-- dtm0/recovery.c -->

Detailed designs should use the <i>@subpage DLD "Motr DLD Template"
<!-- doc/dld-template.c --> </i> as a style guide.

For documentation links, please refer to this file :
doc/motr-design-doc-list.rst

 */
