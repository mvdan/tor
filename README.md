## Branches

* **master**: Upstream (Tor) master branch

* **consdiff-1**: First feature branch, including old-repo-cleaned-4 on top of
  master and more work.

* **consdiff-N**: More iterations of consdiff work rebased on top of a newer
  master branch and a cleaned up history.

* **old-repo**: Exact branch copy from the old `tor_sample` repo

* **old-repo-cleaned**: Like old-repo, but after removing unwanted files with
  `filter-branch`

* **old-repo-cleaned-2**: Like old-repo-cleaned, but with `tor_sample.c`
  merged into `client.c` in the history

* **old-repo-cleaned-3**: Like old-repo-cleaned-2, but with a dozen commits
  squashed and some lazy commit messages reworded. Also, client.c and server.c
  are now removed when consdiff.c is created.

* **old-repo-cleaned-4**: Like old-repo-cleaned-3, but prepared to be merged
  into a feature branch with Tor - files moved where they should be, tests
  integrated, etc.
