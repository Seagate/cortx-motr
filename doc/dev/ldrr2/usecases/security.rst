*********************
Security Requirements
*********************


SEC-10 Only required ports should remain open on public networks HTTP, HTTPS, UDS, …
------------------------------------------------------------------------------------

* Open 'allowed' HTTP/HTTPS/FTP/... port with right credentials.
        - The system should allow the User access to the system through the 
          'opened' ports after validating the credentials.
        - Do not allow more than 'X' sessions of the authenticated User at any 
          point in time.


SEC-20 A customer shouldn’t have root or “sudo all” permissions on the cluster
------------------------------------------------------------------------------

* Run UNIX shell (sh/bash/ksh/csh/zsh/...)as sudo on any node of the cluster.
        - The system should not allow any of the shell commands to be executed as super-user.

* Run 'allowed' commands as sudo on any node of the cluster.
        - The system should only allow those commands to be executed as a super user.
        - Make sure those commands in turn cannot launch shell within them since
          that would again provide sudo enabled shell access.


SEC-40 Every LR cluster should have unique node-level Linux credentials which must be handled in a secure manner by Seagate support.
------------------------------------------------------------------------------------------------------------------------------------
I interpret this as each node of the same cluster should have different password for 'root' login

* Admin login on each node.
        - Confirm each node has a different password for Admin login.

* 'root' password of any node cannot be derived from the password of the peer
  nodes
        - No node-specific information should be embedded in the password for
          that node.


SEC-45 A support person using support credentials should be able to SSH between all cluster nodes using shared keys
-------------------------------------------------------------------------------------------------------------------

* Login to one of the nodes from  public network using support credentials and
  from this node then login to any other node of the same cluster with the same
  support credentials.
        - The target node should allow password-less login for the same support
          user.


SEC-50 It should be possible to configure a dedicated signed certificate and certificate authority for the S3 API service
------------------------------------------------------------------------------------------------------------------------- 

* Login using aws to each s3 server node in the cluster and access every bucket
        - The User should be able to access bucket from all the s3servers.


SEC-70 LR must provide detailed audit log for all CSM activities, including time and user name
----------------------------------------------------------------------------------------------

* User or Sytem admin logs in the system does some CSM operations and logs out of
  the system
        - CORTX should record the login/logout date/time and CSM activities
          as performed by the User.

* User or Sytem admin tries to login to the system using incorrect credentials
        - CORTX should record the date/time and machine details from which this 
          failed login attempt was executed.


SEC-80 Password should not be logged in any audit server logs
-------------------------------------------------------------

* User/System Admin logins to the CSM with correct credentials.
        - CORTX should allow the login and record the details of this login such
          as date/time and host name but it should not record the password used
          for authenticating the User.


SEC-130 Define Security Vulnerability Handling process
------------------------------------------------------

TBD


