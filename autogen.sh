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
regex="^([0-9]+)\.([0-9]+)\.([0-9]+)$"

while getopts "hv:" arg; do
  case $arg in
    h)
      echo "Usage:"
      echo "   -h            Help"
      echo "   -v VERSION    RPM version number"
      echo "                 Must be numeric with two dot(.)"
      echo "                 Example: 1.3.20"
      exit 0
      ;;
    v)
      version=$OPTARG
      echo RPM version: "$version"
      if [[ "$version" =~ $regex ]]
      then
        MAJOR=$(echo "$version" | cut -d "." -f 1)
        MINOR=$(echo "$version" | cut -d "." -f 2)
        PATCH=$(echo "$version" | cut -d "." -f 3)
        sed -i "/m4_define(\[M0_VERSION_MAJOR],/c\m4_define([M0_VERSION_MAJOR],[$MAJOR])" configure.ac
        sed -i "/m4_define(\[M0_VERSION_MINOR],/c\m4_define([M0_VERSION_MINOR],[$MINOR])" configure.ac
        sed -i "/m4_define(\[M0_VERSION_PATCH],/c\m4_define([M0_VERSION_PATCH],[$PATCH])" configure.ac
      else
        echo "RPM version must be numeric with two dot(.). Example: 1.3.20"
        exit 1
      fi
      ;;
    *)
      ;;
  esac
done
autoreconf --install --force
