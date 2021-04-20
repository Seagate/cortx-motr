==================
MOTR - CI Workflow
==================


****************************
MOTR - Multi Branch Pipeline
****************************

This Continuous Integration (CI) pipeline is used to test the source code before merging.

In the Motr GitHub repository, when a Pull Request (PR) is created to complete a merge with the target branch, this pipeline is triggered by polling hook with an interval of 5 minutes.

The following tests are performed:

- Code Coverage

  - Expected Run Time (approximate): 1 – 2 minutes

  - Expected Result: SUCCESS

    Information related to CppCheck warnings is updated in the PR status section.

- Motr - Test

  - Expected Run Time (approximate): 6 – 7 hours
  - Expected Result – SUCCESS, FAILED, or UNSTABLE
  - If the test is executed without failures, the result is **SUCCESS**.
  - The result of the test is **FAILED**, due to one or more of the following:

    - RPM build failure due to source code issue
    - Test script fail limit (more than 12 tests fail)
    - Infrastructure related issues (for example, node space issue)

  - If more than one test is failed, the result of the test is **UNSTABLE**.

  **Note**: The status of the GitHub PR is **SUCCESS**, if the build is either **SUCCESS** or **UNSTABLE**.

  .. image:: motr-test.png

Pipeline Information
====================

Information related to the pipeline is detailed below.

- Source Code repo: `https://github.com/Seagate/cortx-motr <https://github.com/Seagate/cortx-motr>`_

- Jenkins Pipeline: `http://eos-jenkins.colo.seagate.com/job/Release_Engineering/job/github-work/job/GitHub%20Multibranch%20Jobs/job/motr-multibranch/ <http://eos-jenkins.colo.seagate.com/job/Release_Engineering/job/github-work/job/GitHub%20Multibranch%20Jobs/job/motr-multibranch/>`_

- Artifacts: The Junit Test results and CppCheck report are available in the build page
- Test Environment: HW Node
- Cleanup

  -  Workspace Cleanup: On every post build
  -  Build Cleanup: 100 builds or 30 days

Test Config
================

The test config can be found by navigating to the below link.

- `.xperior/testds/motr-single_tests.yaml <https://github.com/Seagate/cortx-motr/blob/main/.xperior/testds/motr-single_tests.yaml>`_

PR Build Accelerator
====================

The below mentioned job is used to schedule an immediate build for any given PR.

Job Location: `http://eos-jenkins.colo.seagate.com/job/Release_Engineering/job/github-work/job/GitHub%20Multibranch%20Jobs/job/motr-pr-build-accelerator/ <http://eos-jenkins.colo.seagate.com/job/Release_Engineering/job/github-work/job/GitHub%20Multibranch%20Jobs/job/motr-pr-build-accelerator/>`_

Parameters
----------
- PULL_REQUEST_ID

  - For the provided pull request ID, the corresponding build is promoted to top the top of queue.

- FORCE_RUN
  
  - When checked, the build is scheduled immediately by terminating any running build.

- BUILD_PRIORITY

  - Expected values range from one to five. This parameter is required when multiple builds are scheduled with different priorities.

Limitations
===========

- There are six HW nodes. Hence, on some occasions, there is queue of PR builds.
- The jenkins pipeline configuration uses five minutes of polling time to detect the PR changes. Hence, there would be a delay of five minutes in updating the PR status.

References
==========

- Pipeline setup and improvements: `EOS - 9662 <https://jts.seagate.com/browse/EOS-9662>`_ and `EOS-9898 <https://jts.seagate.com/browse/EOS-9898>`_
- Xperior: `roman_grigoryev_xperior_lad14_seagate.pdf <https://www.eofs.eu/_media/events/lad14/08_roman_grigoryev_xperior_lad14_seagate.pdf>`_

Test Node Setup (configuration)
===============================
The test setup of Motr is described in the diagram below.

Jenkins Test Nodes
------------------
- Test Controller Node
 
  - ssc-vm-0185.colo.seagate.com

- Available Test Nodes

  - `smc12-m11.colo.seagate.com  <http://eos-jenkins.colo.seagate.com/computer/motr-remote-controller-smc12-m11/>`_
  - `smc13-m11.colo.seagate.com  <http://eos-jenkins.colo.seagate.com/computer/motr-remote-controller-smc13-m11/>`_
  - `smc15-m11.colo.seagate.com <http://eos-jenkins.colo.seagate.com/computer/motr-remote-controller-smc15-m11/>`_
  - `smc16-m11.colo.seagate.com  <http://eos-jenkins.colo.seagate.com/computer/motr-remote-controller-smc16-m11/>`_
  - `smc28-m10.colo.seagate.com   <http://eos-jenkins.colo.seagate.com/computer/motr-remote-controller-smc28-m10/>`_
  - `smc29-m10.colo.seagate.com  <http://eos-jenkins.colo.seagate.com/computer/motr-remote-controller-smc29-m10/>`_


****************************
MOTR - Development Pipeline
****************************
This pipeline is used to build Motr RPMS from the Dev branch.

- The dev pipeline gets triggered by polling hook in an interval of five minutes, whenever code is pushed to Motr dev branch.
- S3 server and HARE are dependent on the Motr version. Hence, the build pipeline gets triggered in the post build step.

Pipeline Information
====================
The pipeline information is mentioned below.

- Source Code repo: `https://github.com/Seagate/cortx-motr/tree/dev <https://github.com/Seagate/cortx-motr/tree/dev>`_
- Jenkins Pipeline: `http://eos-jenkins.colo.seagate.com/job/Cortx-Dev/job/RHEL-7.7/job/Motr/ <http://eos-jenkins.colo.seagate.com/job/Cortx-Dev/job/RHEL-7.7/job/Motr/>`_
- Artifact location: `http://cortx-storage.colo.seagate.com/releases/cortx/github/dev/rhel-7.7.1908/<{RELEASE_NUMBER}_motr <http://cortx-storage.colo.seagate.com/releases/cortx/github/dev/rhel-7.7.1908/%3C%7BRELEASE_NUMBER%7D_motr>`_
- Build Environment: Docker Container [ RHEL 7.7 Host ]  

**Note**: The build artifact contains Motr Dev RPMS along with the release rpms of other components.

Pipeline Notification
=====================

- If the build is triggered successfully, an email is sent to the committer.
- If failure occurs, a notification is sent to `grp.motr.gatekeepers@seagate.com <mailto:grp.motr.gatekeepers@seagate.com>`_

***********************
MOTR - Release Pipeline
***********************
This pipeline is used to build Motr RPMS from the release branch. As per the new workflow, the code will be merged with the release branch on Tuesday and Thursday. This pipeline builds the Motr RPM on the scheduled Tuesday or Thursday merge.

Pipeline Information
====================
The pipeline information is mentioned below.

- Source Code repo: `https://github.com/Seagate/cortx-motr/tree/release  <https://github.com/Seagate/cortx-motr/tree/release>`_

- Jenkins Pipeline: `http://eos-jenkins.colo.seagate.com/job/Release_Engineering/job/github-work/job/Motr/ <http://eos-jenkins.colo.seagate.com/job/Release_Engineering/job/github-work/job/Motr/>`_
- Artifact location: `http://cortx-storage.colo.seagate.com/releases/cortx/github/release/rhel-7.7.1908/ <http://cortx-storage.colo.seagate.com/releases/cortx/github/release/rhel-7.7.1908/>`_
- Build Environment: Docker Container [ RHEL 7.7 Host ]

Pipeline Notification
=====================

- If the build is triggered successfully, an email is sent to the committer.
- If failure occurs, a notification is sent to `grp.motr.gatekeepers@seagate.com <mailto:grp.motr.gatekeepers@seagate.com>`_
