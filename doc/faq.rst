GitHub
------

#. Q: I couldn't fetch from GitHub with the following error message::

        $ git fetch
        ERROR: The `Seagate' organization has enabled or enforced SAML SSO. To access
        this repository, you must use the HTTPS remote with a personal access token
        or SSH with an SSH key and passphrase
        that has been allowed for this organization. Visit
        https://help.github.com/articles/authenticating-to-a-github-organization-with-saml-single-sign-on/ for more information.

        fatal: Could not read from remote repository.

        Please make sure you have the correct access rights
        and the repository exists.

   A: Try https://help.github.com/en/github/authenticating-to-github/authorizing-an-ssh-key-for-use-with-saml-single-sign-on.

#. Q: How to add ssh key that could be used with GitHub?

   A: Go to https://github.com/settings/keys, then generate and add ssh key as
   usual, then press "Enable SSO" -> "Seagate" -> "Authorize".

#. Q: How to fix ``warning: ignoring broken ref refs/remotes/origin/HEAD`` in
   ``git branch`` after changing our primary branch?

   A: Edit ``.git/refs/remotes/origin/HEAD`` and change the branch in the file
   to the new primary branch.


Mero -> Motr rename
-------------------

#. Q: Motr build fails with the following message::

        ...
          CCLD     motr/libmotr-altogether.la
          CCLD     motr/m0d-altogether
          CCLD     rpc/it/m0rpcping-altogether
        Checking xcode protocol... /work/motr/xcode/.libs/lt-m0protocol: error while loading shared libraries: libmotr.so.1: cannot open shared object file: No such file or directory
        make[2]: *** [all-local] Error 127
        make[1]: *** [all-recursive] Error 1
        make: *** [all] Error 2

   A: Remove ``/etc/ld.so.conf.d/mero.conf``, then rebuild Motr after ``git
   clean -dfx`` (WARNING: removes all files that are not staged and are not in
   the repo).
