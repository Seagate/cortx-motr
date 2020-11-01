- [1.0 Prerequisites](#10-Prerequisites)
- [1.1 Setup Git on your Development Box](#11-Setup-Git-on-your-Development-Box)
- [1.2 Submit your changes](#12-Submit-your-Changes)
   * [1.2.1 Clone the cortx-motr repository](#121-Clone-the-cortx-motr-repository)
   * [1.2.2 Code Commits](#122-Code-commits)
   * [1.2.3 Create a Pull Request](#123-Create-a-Pull-Request)
- [1.3 Run Jenkins and System Tests](#13-Run-Jenkins-and-System-Tests)
- [FAQs](FAQs)

Contributing to the cortx-motr repository is a three-step process where you'll need to:

1. [Clone the cortx-motr repository](#121-Clone-the-cortx-motr-repository)
2. [Commit your Code](#122-Code-commits)
3. [Create a Pull Request](#123-Create-a-Pull-Request)

## 1.0 Prerequisites

<details>
  <summary>Before you begin</summary>
    <p>

Before you set up your GitHub, you'll need to
1. Generate the SSH key on your development box using:

    ```shell

    $ ssh-keygen -o -t rsa -b 4096 -C "your email-address"
    ```
2. Add the SSH key to your GitHub Account:
   1. Copy the public key: `id_rsa.pub`. By default, your public key is located at `{YOUR_HOME_DIR}/.ssh/id_rsa.pub`
   2. Navigate to [GitHub SSH key settings](https://github.com/settings/keys) on your GitHub account.
   3. Paste the SSH key you generated in Step 1.
   4. Click **Add SSH key** to store your SSH key.

   </p>
    </details>

## 1.1 Setup Git on your Development Box

**Before you begin:** Update Git to the latest version. 

Once you've installed the prerequisites, follow these steps to set up Git on your Development Box:

1. Set up git config options using:

   ```shell

   $ git config --global user.name ‘Your Name’
   $ git config --global user.email ‘Your.Name@Domain_Name’
   $ git config --global color.ui auto
   $ git config --global credential.helper cache
   ```
## 1.2. Submit your Changes

Before you can work on a GitHub feature, you'll need to clone the cortx-motr repository.

### 1.2.1 Clone the cortx-motr repository

You'll need to **Fork** the cortx-motr repository to clone it into your private GitHub repository. Follow these steps to clone the repository to your gitHub account:
1. Navigate to the 'cortx-motr' repository homepage on GitHub.
2. Click **Fork**.
3. Run the following commands in Shell:

   `git clone --recursive git@github.com:your-GitHub-Id/cortx-motr.git`

   Or

   `git clone --recursive https://github.com/your-GitHub-Id/cortx-motr.git`

4. Check out to the “main” branch using:

   `$ git checkout main`

   `$ git checkout -b "your-local-branch-name"`
### 1.2.2 Code Commits

You can make changes to the code and save them in your files.

1. Use the command below to add files that need to be pushed to the git staging area:

    `$ git add foo/somefile.cc`

:page_with_curl: **Notes:** 

- Before sending your patches for review, rebase them on top of:

   `origin/main`
   
   1. Then, check them at least with:

       `$ ./scripts/m0 run-ut`
   
   2. Ideally, run the complete tests with:

      `$ ./scripts/m0 check-everything`

2. To commit your code changes use:

   `$ git commit -s -m 'comment'` - enter your GitHub Account ID and an appropriate Feature or Change description in comment.


3. Check out your git log to view the details of your commit and verify the author name using:  `$ git log` and `$git show`

    :page_with_curl: **Note:** If you need to change the author name for your commit, refer to the GitHub article on [Changing author info](https://docs.github.com/en/github/using-git/changing-author-info).

4. To Push your changes to GitHub, use: `$ git push origin 'your-local-branch-name'`

Your output will look like the **Sample Output** below:

   ```shell

   Enumerating objects: 4, done.
   Counting objects: 100% (4/4), done.
   Delta compression using up to 2 threads
   Compressing objects: 100% (2/2), done.
   Writing objects: 100% (3/3), 332 bytes | 332.00 KiB/s, done.
   Total 3 (delta 1), reused 0 (delta 0)
   remote: Resolving deltas: 100% (1/1), completed with 1 local object.
   remote:
   remote: Create a pull request for 'your-local-branch-name' on GitHub by visiting:
   remote: https://github.com/<your-GitHub-Id>/cortx-motr/pull/new/<your-local-branch-name>
   remote: To github.com:<your-GitHub-Id>/cortx-motr.git
   * [new branch] <your-local-branch-name> -> <your-local-branch-name>
   ```

### 1.2.3 Create a Pull Request

1. Once you Push changes to GitHub, the output will display a URL for creating a Pull Request, as shown in the sample code above.

   :page_with_curl: **Note:** To resolve conflicts, follow the troubleshooting steps mentioned in git error messages.
2. You'll be redirected to GitHib remote.
3. Select **main** from the Branches/Tags drop-down list.
4. Click **Create pull request** to create the pull request.
5. Add reviewers to your pull request to review and provide feedback on your changes.

## 1.3 Run Jenkins and System Tests

Creating a pull request automatically triggers Jenkins jobs and System tests. To familiarize yourself with jenkins, please visit the [Jenkins wiki page](https://en.wikipedia.org/wiki/Jenkins_(software)).

## FAQs

**Q.** How do I rebase my local branch to the latest main branch?

**A** Follow the steps mentioned below:

```shell

$ git pull origin main
$ git submodule update --init --recursive
$ git checkout 'your-local-branch'
$ git pull origin 'your-remote-branch-name'
$ git submodule update --init --recursive
$ git rebase origin/main
```

**Q** How do I address reviewer comments?

**A** If you need to address comments from the reviewer, commit your changes then rebase your patches on top of main. Finally submit your patches with:

  `$ git push origin -u your-local-branch-name`

Github will automatically update your review request.

## You're All Set & You're Awesome!

We thank you for stopping by to check out the CORTX Community. We are fully dedicated to our mission to build open source technologies that help the world save unlimited data and solve challenging data problems. Join our mission to help reinvent a data-driven world. 

### Contribute to CORTX Motr

Please [contribute to the CORTX Motr](https://github.com/Seagate/cortx/blob/main/doc/SuggestedContributions.md) initiative and join our movement to make data storage better, efficient, and more accessible. 

Refer to the [Motr Coding Style Guide](../dev/doc/coding-style.md) and the [CORTX Contribution Guide](https://github.com/Seagate/cortx/blob/main/CONTRIBUTING.md) to get started with your first contribution.

### Reach Out to Us

You can reach out to us with your questions, feedback, and comments through our CORTX Communication Channels:

- Join our CORTX-Open Source Slack Channel to interact with your fellow community members and gets your questions answered. [![Slack Channel](https://img.shields.io/badge/chat-on%20Slack-blue)](https://join.slack.com/t/cortxcommunity/shared_invite/zt-femhm3zm-yiCs5V9NBxh89a_709FFXQ?)
- If you'd like to contact us directly, drop us a mail at cortx-questions@seagate.com.
