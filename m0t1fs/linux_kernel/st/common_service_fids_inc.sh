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

# service FID containers
IOS_FID_CON='^s|1'        # ioservice
MDS_FID_CON='^s|2'        # mdservice
RMS_FID_CON='^s|3'        # rmservice
SSS_FID_CON='^s|4'        # sss
 HA_FID_CON='^s|5'        # ha
SNSR_FID_CON='^s|6'       # sns repair
SNSB_FID_CON='^s|7'       # sns rebalance
CONF_FID_CON='^s|8'       # confd
ADDB_FID_CON='^s|9'       # addb
ADDB_CONF_FID_CON='^s|10' # addb for confd
ADDB_IO_FID_CON='^s|11'   # addb for ioservice
ADDB_MD_FID_CON='^s|12'   # addb for mdservice
CAS_FID_CON='^s|13'       # CAS service
ADDB_CAS_FID_CON='^s|14'  # addb for CAS service
DIXR_FID_CON='^s|15'      # DIX repare
DIXB_FID_CON='^s|16'      # DIX rebalance
FDMI_FID_CON='^s|17'      # fdmi service
DTM_FID_CON='^s|18'       # DTM service
