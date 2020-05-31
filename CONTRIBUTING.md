## How to contribute to ACT

(with thanks to the [Ruby on Rails](https://github.com/rails/rails) guide from which this borrows heavily)

### **Is this a build issue?**

* Please be sure to follow the exact build instructions in the [README](https://github.com/asyncvlsi/act).

* Check the [.circleci config](https://github.com/asyncvlsi/act/tree/master/.circleci) to see how to build ACT on a clean installation of a few OS variants. Any commits to the master branch are automatically built on those variants. If the current ACT build is not passing, then the developers are already working on the issue so please don't open a new one.

* If you have an operating system that differs substantially from a standard Unix platform, please start a discussion on our [Mattermost site](http://avlsi.csl.yale.edu/act/doku.php) on the developer channel rather than opening an issue.

* Please note that if the developers do not already have access to a machine with your specific configuration, it will be difficult for them to be able to replicate your problem. A Mattermost discussion might be the best way to proceed in this case.

### **Did you find a bug?**

* **Ensure the bug was not already reported** by searching on GitHub under [Issues](https://github.com/asyncvlsi/act/issues).

* If you're unable to find an open issue addressing the problem, [open a new one](https://github.com/asyncvlsi/act/issues/new). 
   * Be sure to include a **title and clear description**, as much relevant information as possible, and **an example** ACT file along with what you expect and the erroneous behavior.
   * For examples of good small test cases, please take a look at the test suite in the act/test/ directories.
   * Note that we would like to add your test case to this suite once the issue has been resolved, so please provide a test case where that will be possible and doesn't reveal anything confidential/etc.
   * Then, please don't get your hopes up! We will get to it as soon as possible but, unless the issue is a critical one, it is likely to take some time for resolution.  

### **Are you requesting a feature?**

* Please don't use Github issues as a way to request a feature.

* We have a [Mattermost site](http://avlsi.csl.yale.edu/act/doku.php) where you can self signup, and that is the most appropriate venue for feature requests.
* Note that the current feature set in the ACT core language has evolved through long experience, so be prepared for a robust discussion before any feature makes it to the TODO list for ACT.

* Once your feature request goes through the discussion phase, please don't be discouraged if your feature isn't immediately prioritized for implementation.


### **Are you requesting a new tool?**

* There are many tools under development, and many more on the wishlist. Please open a discussion with the developers on the  [Mattermost site](http://avlsi.csl.yale.edu/act/doku.php) on the feature request channel. Also indicate if you
are interested in developing the tool.


### **Did you write a patch that fixes a bug?**

* Open a new GitHub pull request with the patch.

* Ensure the PR description clearly describes the problem and solution.
   * Include the relevant issue number if applicable.
   * Please include at least a few small test cases (see above) that exercise your code and demonstrate that the bug has been fixed.

### **Did you fix whitespace, format code, or make a purely cosmetic patch?**

Changes that are cosmetic in nature and do not add anything substantial to the stability or functionality of ACT will generally not be accepted (the Ruby on Rails guide has a [nice explanation](https://github.com/rails/rails/pull/13771#issuecomment-32746700) as to why this is the case).

### **Do you intend to add a new feature or change an existing one?**

* Suggest your change on our [Mattermost site](http://avlsi.csl.yale.edu/act/doku.php) on the feature request channel. Indicate that you plan to develop the feature.
* Please do not start writing code without a robust discussion on the channel. Note that feature additions/changes that are likely to break older ACT designs are unlikely to be accepted.
* Do not open an issue on GitHub until you have collected positive feedback about the change. GitHub issues are primarily intended for bug reports and fixes.

### **Do you have questions about the source code?**

* Ask any question on the [Mattermost site](http://avlsi.csl.yale.edu/act/doku.php) on the developer channel.

### **Do you want to contribute to the ACT documentation?**

* Please take a look at the [existing documentation](http://avlsi.csl.yale.edu/act/doku.php), and identify the section you would like to contribute to.
* All the documentation uses [dokuwiki](http://dokuwiki.org), so please familiarize yourself with the syntax used.
* Then, please start a discussion on the [Mattermost site](http://avlsi.csl.yale.edu/act/doku.php) on the developer channel.

ACT is a volunteer effort. We would love your help!

Thanks!
