Working on crakoon
==================
This document contains some notes on how crakoon development is handled. This
refers to versioning, branching strategies, etc.

It might contain bugs, errors, inconsistencies or false information. Please let
me know if you find any of those. The rules can be changed at any time (except
for the versioning system).

Versioning
----------
Versioning of the crakoon API and ABI is according to the
`Semantic Versioning`_ guidelines. The `checksymbols.sh` script can be used to
validate the list of exported symbols for the C library.

The C++ library API should be considered to be unstable in-between minor
releases for now.

.. _Semantic Versioning: http://semver.org/

Branching
---------
Git branches are used to work on several versions of crakoon concurrently. After
every release of a minor version (e.g. 1.0.0), a branch is created is created
to work on the next minor release (e.g. 1.1).

Golden rule: never force a push. Ever.

Whenever a version is to be released, a tag named after this version (e.g.
"1.0.1") is created on the minor branch (in this case the "1.0" branch), and a
release is created using an appropriate Jenkins job, providing the tag name as a
parameter.

Whenever a change is required, first determine the oldest (minor) version in
which this change should be contained. Commit the change to this branch, or use
this branch as root for a feature branch (see below).

When one or more changes have been incorporated in a minor version branch, this
branch should be merged (with the `--no-ff` flag) into the branch of the next
minor version, and so on.

Whenever you'll be working on a feature for a non-trivial amount of time, create
a branch rooted at the minor version in which this feature should be contained
(taking API/ABI versioning into account, obviously). This branch should be named
after the Jira ticket related to the change (e.g. `ARAKOON-321`).

Don't rebase feature-branches except on their merge point during an interactive
rebase session.

Try not to merge in other branches into feature branches unless strictly
required.

Once a feature has been completed in a feature branch, it can be merged into the
minor branch it was rooted at (also using `--no-ff`). You can use some
interactive rebasing to clean up the branch history before this merge.

When a feature is postponed towards a later release than the one the feature
branch was rooted at, rebase the feature branch to the target branch before
merging it.

Q: I want to add a simple feature to the 1.1 cycle before a 1.1.0 release
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
::

    $ git checkout 1.1
    $ # edit / commit / repeat
    $ git push origin 1.1

If the push fails (because someone else already made some changes in-between),
and your change doesn't warrant a proper branched line of development (up to
your discretion), rebase your changes before pushing::

    $ git rebase origin/1.1
    $ git push origin 1.1

Q: I want to fix some documentation formatting in the 1.0 cycle after a 1.0.0 release
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
::

    $ git checkout 1.0
    $ # edit / commit / repeat
    $ git checkout 1.1
    $ git merge --no-ff 1.0
    $ git push origin 1.0 1.1

Don't merge the 1.0 changes into 1.1 if this is not strictly required for 1.1
development, and more changes to 1.0 are to be expected. try to limit the
number of merges between 1.N and 1.(N + 1) as much as possible.

Q: A bug has been found which needs to be fixed in the 1.0 cycle, and we're at 1.3 already
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
::

    $ git checkout 1.0
    $ # edit / commit / repeat
    $ git checkout 1.1
    $ git merge --no-ff 1.0
    $ git checkout 1.2
    $ git merge --no-ff 1.1
    $ git checkout 1.3
    $ git merge --no-ff 1.2
    $ git push origin 1.0 1.1 1.2 1.3

Now you better get ready to create some releases ;-)

Q: I want to work on a non-trivial feature ARAKOON-321 planned for the 1.1 cycle before a 1.1.0 release
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
::

    $ git checkout -b ARAKOON-321 1.1
    $ # edit / commit / repeat
    $ git push origin ARAKOON-321:ARAKOON-321

Once the feature has been completed (still before a 1.1.0 release)::

    $ # Optional: interactive rebase
    $ # git checkout ARAKOON-321
    $ # git rebase -i `git merge-base HEAD 1.1`
    $ git checkout 1.1
    $ git merge --no-ff ARAKOON-321
    $ git branch -d ARAKOON-321
    $ git push origin 1.1:1.1 :ARAKOON-321

Q: I was working on a non-trivial feature ARAKOON-321 planned for the 1.1 cycle but didn't make it until 1.3
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
::

    $ git checkout 1.3
    $ git merge --no-ff ARAKOON-321
    $ git branch -d ARAKOON-321
    $ git push origin 1.3:1.3 :arakoon-321


Commit guidelines
-----------------
Take a look at `this document`_ for some notes on commit messages etc.

.. _this document: https://gist.github.com/c26f685982a7e33a0785

TODO
----
- Code style
