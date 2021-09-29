# About

The script s3setup.sh sets up the S3 stack with custom
builds of hare and motr to test and fix integrational
issues.
The script works on top of the setup deployed by the
Cortx deployment pipeline for a single CentOS-7.9 VM.
When the setup is deployed on a target VM the script
can be run to substitude hare and motr with development
versions built from sources locally and dev-installed.
Now the DTM0 dev branches are used to build hare and motr.

# How to use this piece of masterpeace?

First of all you need to navigate to
https://ssc-cloud.colo.seagate.com/ui/service/catalogs
and order a VM "G1 - LDRr2 - CentOS-7.9" with the following
recommended parameters:
- 8 CPUs

- 16GB RAM

- 5 additional disks

- 50GB disk size

The second step is to set root password for the target VM
and run the following deployment pipeline:
http://eos-jenkins.mero.colo.seagate.com/job/Cortx-Deployment/job/VM-Deployment-1Node-CentOS-7.9/

When the pipeline is comleted copy the directory
$MOTR_DIR/dtm0/it or simply clone motr repo (NOTE: do not
use the path /root/cortx/cortx-motr as it is reserved
by the script) to the VM. Now the s3setup.sh can be run.

# What the s3setup.sh script does?

By default (without passed options) the script does the
following steps:
- Create the directory /root/cortx and clone there hare
  and motr repos, checkout DTM0 branches.

- Stop the cluster using Pacemaker (as it is left running
  by the deployment pipeline).

- Backup configuration files as they will be overwritten
  duuring further dev-install.

- Remove motr and hare RPMs.

- Build and dev-install motr and hare.

- Restore configuration files, patch the CDF to have 3 IOs.

- Get s3bench utility.

- Bootstrap the cluster using hare.

There are several options that modify default behavior:
- --no-bootstrap: do all steps listed above but do not
  bootstrap the cluster;

- --s3bench: in addition to default steps run s3bench as an
  S3 client, stop the cluster and gather ADDB2 data;

- --s3workload: do not do default steps, the only done steps
  are bootstrap the cluster, run s3bench, stop the cluster
  and gather ADDB2 data (i.e. development/debugging mode),
  NOTE: the cluster should be shut down before calling this
  script with that option.
