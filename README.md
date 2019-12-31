# act
[![CircleCI](https://circleci.com/gh/asyncvlsi/act.svg?style=svg)](https://circleci.com/gh/asyncvlsi/act)

This is the implementation of the ACT hardware description language.
(ACT = asynchronous circuit/compiler tools)

System requirements:

   * The system must have libedit installed. For the yum package manager, the
     package is called libedit-devel; for apt-get, it is libeditline-dev. Some
     systems have *both* packages. In that case please use libedit, not libeditline.
   * The system should have zlib installed      
   * The system should have the macro pre-processing package m4 installed

Build instructions:

   * Create a directory where you'd like the tools to be installed. Example
     common locations include /usr/local/cad, /opt/cad, /opt/async
   * Set the environment variable ACT_HOME to point to the install directory.
   * Set the environment variable VLSI_TOOLS_SRC to the root of the source tree
     (i.e. the /path/to/act).
   * From the $VLSI_TOOLS_SRC directory, run
        ./configure $ACT_HOME
   * Run ./build

If there is an issue building the software and you want to do a clean build, use
"make realclean"

Once you've built the tools, run "make install" to install the files, and  "make runtest" to run through a set of test cases.

More detailed documentation is available here:
    http://avlsi.csl.yale.edu/act/
    
A first ACT tutorial:
    http://avlsi.csl.yale.edu/act/doku.php?id=tutorial:basicprs


Installation instructions are available here:
    http://avlsi.csl.yale.edu/act/doku.php?id=install
