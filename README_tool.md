# The ACT tools

Repositories that contain standalone ACT tools that link against the core
ACT library can be built using the standard Makefile setup in the act repo.
Unless otherwise specified in the README, the way to build an ACT tool
is:

   * Set the environment variables ACT_HOME and VLSI_TOOLS_SRC as per the
     build instructions in the core ACT library.
 
   * Run make depend, followed by make to build the tools. make install
     will install the files in the ACT_HOME install directory.

## More information:

More detailed documentation is available here:
    http://avlsi.csl.yale.edu/act/
    
A first ACT tutorial:
    http://avlsi.csl.yale.edu/act/doku.php?id=tutorial:basicprs
