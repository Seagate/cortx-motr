Motr repository management
==========================

The key words "MUST", "MUST NOT", "REQUIRED", "SHALL", "SHALL
NOT", "SHOULD", "SHOULD NOT", "RECOMMENDED",  "MAY", and
"OPTIONAL" in this document are to be interpreted as described in
RFC 2119.

Definitions
-----------

Developer - a person who writes code.

Gatekeeper - a developer who has landing permissions.

General guidelines
------------------

1. The longer a code is outside the base branch, the longer AUTHOR needs to
   maintain it.
2. Small and trivial changes are landed faster than large or non-trivial.
3. If there is some isolated refactoring or small improvement in a feature
   branch, it's always better to create a separete PR to land the small change
   to the base branch.

Git rules
---------

1. Branching

   1. There is ``dev`` branch. This is the branch development branches SHOULD
      be branched off. This is the branch where new patches SHOULD be landed
      first.
   2. There are release branches. They are branched off ``dev`` at some point
      in time. A commit from ``dev`` MAY be ported to a release branch.
   3. There are development branches. They SHOULD be created for development.

2. Gatekeeping

   1. All commits to ``dev`` or release branches MUST be landed by a
      gatekeeper.
   2. There is no gatekeeping for development branches.

GitHub workflow
---------------

1. The repo SHOULD cloned from GitHub. Please make sure to use ``git clone
   --recursive``.
2. A development branch SHOULD be created off ``dev``.
3. Development continues in the branch. All kinds of `git commit rewrites
   <https://git-scm.com/book/en/v2/Git-Tools-Rewriting-History>`_ MAY be used
   without limitations.  The branch MAY be published to GitHub during the
   development and it MAY be updated at any time.
4. When the code is ready for review a pull request SHOULD be created.
5. After pull request is created git commit rewrites and force pushes MUST NOT
   be used. If some commits are not needed in PR anymore ``git revert`` MAY be
   used to remove commits as an alternative to ``git rebase``.
6. Pull request code review is done on GitHub. How to choose reviewers: TBD.
7. After review is complete and there are no CI test failures, introduced in
   the patch, a gatekeeper MUST be assigned for review.
8. The gatekeeper either approves the pull request and then lands it or leaves
   review comments and the patch goes back to the review phase.
9. After PR is landed to ``dev`` it MAY be cherry-picked on top of a release
   branch and then a new PR for this release branch SHOULD be created. If there
   is no need for additional review for a release branch the code review part
   is skipped.  Everything else is applicable just like with a regular PR.

Development workflow
--------------------

.. code-block::

      start
        v a development branch is created
        +-------------------------+
        | development in a branch |<---------------------------------<+
        +-------------------------+                                   ^
        v branch is ready       v the development branch is modified  |
        | for review            +>----------------------------------->+
        +---------------+
        | PR is created |
        +---------------+
        V add 1 or more reviewers
        +-------------------------------+
        | PR review: wait for reviewers |
        +-------------------------------+
        V               author has   ^  v
        | all           nothing left |  | PR changes required
        | reviewers     to address   ^  v
        | approve,              +--------------------------------+
        | review                | PR review: wait for the author |<---------<+
        | is complete           +--------------------------------+           ^
        V                            ^          ^   ^   V  author addresses  |
        +-------------------+  FAIL  |          |   |   |  review comments   |
      +>| CI check (author) |>------>v          |   |   +>------------------>+
      ^ +-------------------+                   |   |
      | V PASS                                  |   |
      | +--------------------+  review comments ^   |
      | | Gatekeeping review |>---------------->+   |
      | +--------------------+                      |
      | V DECISION: land the PR.                    |
      | +-----------------------+   FAIL            |
      | | CI check (gatekeeper) |>----------------->+
      | +-----------------------+
      | V PASS
      | +--------+
      | |+------+|
      | || DONE ||
      | |+------+|
      | +--------+
      |     v A decision is made to backport the patch.
      ^     | New PR is created for the target branch.
      +<---<+


Development workflow details
----------------------------

Definitions
...........

Base branch
  The branch where the primary development happens. Currently it's ``dev``.

Feature branch
  The branch that is used for feature development.

PR
  `Pull request
  <https://docs.github.com/en/github/collaborating-with-issues-and-pull-requests/about-pull-requests>`_
  on GitHub.
  PR is created from a feature branch.
  PR is created for a target branch. Usually base branch is the target branch.


Roles
.....

The same developer MAY have different roles in different PRs.
The same developer MAY have different roles in the same PR.

AUTHOR
   Developer that actually creates changes to the repo.

REVIEWER
   Developer that review the changes.
   REVIEWERS MAY add comments and request changes.
   REVIEWERS MAY add code to the PR branch.

GATEKEEPER
   Developer that has permissions to merge PR to PR's base branch.
   GATEKEEPER MAY do anything AUTHOR or REVIEWER could do.


States and transitions
......................

start
   - this is the initial state;
   - AUTHOR clones Motr repo before the work on a feature starts;
   - AUTHOR SHOULD create a branch in our primary repo `Seagate/cortx-motr
     <https://github.com/Seagate/cortx-motr>`_ on GitHub;
   - AUTHOR MAY use a fork for PR. Disadvantage is that only the AUTHOR would
     be able to update the PR, which may require several extra round-trips
     during gatekeeping review and gatekeeping CI check.

start -> development in a branch
   AUTHOR creates a development branch off the base branch.

development in a branch
   - AUTHOR develops feature in the feature branch.
   - AUTHOR MAY modify the feature branch in any way, including adding commits,
     rebases, merges, amends etc.
   - AUTHOR SHOULD push the feature branch to remote.
   - AUTHOR MAY use ``push --force`` if needed.
   - AUTHOR MAY make a decision to create PR to start code review.

development in a branch -> PR is created
   - The feature branch MUST have 1 or more commits;
   - The feature branch SHOULD have 1 commit unless there is a very good reason
     to have more;
   - AUTHOR MUST make a decision to start code review;
   - AUTHOR MUST create a PR on GitHub for the base branch.

PR is created
   In this state PR is on GitHub with 0 REVIEWERS.

PR is created -> PR review: wait for reviewers
   This state transtion happens when AUTHOR or GATEKEEPER adds a reviewer to
   the PR.
   REVIEWERS MAY also add themselves.

PR review: wait for reviewers
   In this state AUTHOR waits for PR comments from REVIEWERS.

   - REVIEWERS MAY post review comments;
   - review comments SHOULD posted to GitHub PR;
   - each REVIEWER MUST press "Resolve conversation" button on each
     conversation, initially initiated by the REVIEWER, after all topics in the
     conversation are resolved;
   - each REVIEWER MUST press "Review changes" on GitHub eventually and either
     "Approve" or "Request changes".

PR review: wait for reviewers -> PR review: wait for the author
   This state transition happens if at least one of the following is true:

   - there are review comments from REVIEWERS that are not taken care of;
   - changes to PR are required;
   - CI fails.

PR review: wait for the author
   This is the state where AUTHOR addresses review comments from REVIEWERS.
   The AUTHOR:

   - MAY add commits to the PR branch if code changes are needed;
   - MAY use ``git revert`` in the PR branch;
   - MAY merge target branch to the PR branch;
   - MAY update PR summary or description;
   - MUST press "Resolve conversation" button on each conversation, initially
     initiated by the AUTHOR, after all topics in the conversation are
     resolved;
   - MUST press "Re-request review" button for each REVIEWER or GATEKEEPER,
     who requested changes or posted review comments that are fully addressed;
   - MUST NOT force push to the PR branch.

PR review: wait for the author -> PR review: wait for reviewers
   If all of the following is true this state transition happens.

   - there is nothing left in PR for AUTHOR to reply to;
   - no further fixes are needed;
   - CI is passing.

PR review: wait for reviewers -> CI check (author)
   This state transiton happens when all of the following conditions are true:

   - all reviewers "Approve" the PR;
   - all PR conversations are resolved.

CI check (author)
   In this state AUTHOR MUST check if CI passes. The result of the check
   determines next state transition.

CI check (author) -> Gatekeeping review
   This state transition happens if AUTHOR checks CI and CI passes.
   AUTHOR MAY notify a GATEKEEPER that PR is ready for review by adding the
   GATEKEEPER to the "Assignees" list.
   GATEKEEPERS MUST check PRs from time to time to select PRs that are ready
   for Gatekeeping review.

CI check (author) -> PR review: wait for the author
   If CI check fails AUTHOR MUST either fix the PR or find someone who can fix
   the CI.

Gatekeeping review
   GATEKEEPER MUST make a decision regarding PR: either land it or post review
   comments that AUTHOR MUST address. GATEKEEPER MUST check at least the
   following before making the decision:

   - PR summary and description accurately describe the change made by PR;
   - PR doesn't change something not mentioned in PR summary or description;
   - all PR conversations are resolved correctly;

Gatekeeping review -> PR review: wait for the author
   If GATEKEEPER decides that PR is not ready for landing GATEKEEPER adds
   comments and requests changes from the AUTHOR. In this case GATEKEEPER also
   becomes one of REVIEWERS.

Gatekeeping review -> CI check (gatekeeper)
   If GATEKEEPER decides that PR is ready for landing then GATEKEEPER approves
   the PR. GATEKEEPER MUST check if CI passes for the patch.

CI check (gatekeeper)
   GATEKEEPER MUST check CI before landing the patch.
   GATEKEEPER SHOULD merge recent base branch into PR branch to get test
   results for what is going to be landed to the base branch.
   GATEKEEPER MUST make a decision: either land the patch to the base branch or
   post review comments that AUTHOR MUST address.

CI check (gatekeeper) -> DONE
   - if GATEKEEPER decides that PR is ready to be merged the PR is merged to
     the base branch;
   - "Squash and merge" button MUST be used to ensure linear history;
   - GATEKEEPER SHOULD use PR summary and PR description as commit message;
   - commit message MAY be changed by the GATEKEEPER before merging;
   - PR number in parentheses MUST be present at the end of commit summary line
     for the PR.

CI check (gatekeeper) -> PR review: wait for the author
   GATEKEEPER MUST post comments about what's wrong with the PR.
   AUTHOR MUST address the comments. In this case GATEKEEPER also
   becomes one of REVIEWERS.


DONE
   This is the final state. PR is closed.
   New comments MAY be added to the PR after it's closed.

DONE -> CI check (author)
   If there is a decision to port the patch to another branch either GATEKEEPER
   or AUTHOR MUST create a new branch with this patch, cherry-picked from the
   base branch to the target branch, then a new PR MUST be created for the
   target branch.
   All subsequent state changes use new PR as "the PR" and all new PRs (if
   there are multiple patches to backport) have independent state changes.
   Regardless of who posts the PR AUTHOR of the original PR (for the base
   branch) becomes the AUTHOR of the new PR.


GitHub account configuration
----------------------------

- Profile

  - Public profile - Name: set it to real name like in ``git config user.name``.

    This name is used in the commit message if a your PR is merged using
    "Squash and merge" option in GitHub UI.

- Emails

  - [ ] Keep my email addresses private

    The e-mail address it either easily accessible or it's in ``git log``
    already. Checking this option makes commit e-mails look ugly.


GitHub queries
--------------

`GitHub search for issues and PRs <https://docs.github.com/en/github/searching-for-information-on-github/searching-issues-and-pull-requests>`_.

.. list-table::
   :widths: 60 40
   :header-rows: 1

   * - Description
     - Link
   * - All PRs where I'm the AUTHOR
     - `is:open author:@me <https://github.com/Seagate/cortx-motr/issues?q=is%3Aopen+author%3A%40me+>`_



Gerrit -> GitHub transition
---------------------------

Gatekeeping
-----------
