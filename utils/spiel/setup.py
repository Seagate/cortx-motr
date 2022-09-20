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

from distutils.core import setup, Extension

motr = Extension('motr',
                 define_macros=[('M0_INTERNAL', ''), ('M0_EXTERN', 'extern')],
                 include_dirs=['../../', '/home/srg4/sk/c_code/c_code_master/dma_ip_drivers-master/XDMA/linux-kernel/tools'],
                 sources=['motr.c'],
                 extra_compile_args=['-w', '-include', 'config.h'])


setup(name='motr', version='1.0',
      description='Auxiliary definitions used by m0spiel',
      author='Igor Perelyotov', author_email='<igor.m.perelyotov@seagate.com>',
      ext_modules=[motr])
