# act

This is the implementation of the ACT hardware description language.
(ACT = asynchronous circuit/compiler tools)

Build instructions:

   * The system must have libedit installed. For the yum package manager, the
     package is called libedit-devel; for apt-get, it is libeditline-dev
   * Create a directory where you'd like the tools to be installed. Example
     common locations include /usr/local/cad, /opt/cad, /opt/async
   * Set the environment variable ACT_HOME to point to the install directory.
   * Set the environment variable VLSI_TOOLS_SRC to the root of the source tree
     (i.e. the /path/to/act).
   * From the $VLSI_TOOLS_SRC directory, run
        ./configure $ACT_HOME
   * Run ./build

Language history:
  * ~1991 (?), a language for hierarchical production rules was developed at Caltech (A.J. Martin's group). This was dubbed "CAST" for Caltech Asynchronous Synthesis Tools.
  * ~1995, a new CAST language was designed and implemented by Rajit Manohar (student in A.J. Martin's group). This was used to implement the first high-performance asynchronous microprocessor
     * A version of this language was also re-implemented at a startup company started by Andrew Lines and Uri Cummings (also in Martin's group) called Fulcrum Microsystems.
     * (?) Some development continued at Caltech
  * ~2004, a language that provided the same functionality as CAST but with syntax for new features (not implemented) was developed at Cornell by Rajit Manohar, and this was called ACT (version 0; 0 because the new features were not supported yet)
     * ACT was used by Achronix Semiconductor, a startup company founded by Rajit Manohar along with some of his Ph.D. students
     * An open-source (GPLed) version of ACT (called "HACKT") was developed by David Fang (student in Rajit Manohar's group)
  * 2005-2014: ACT was used to implement a large number of asynchronous chips, including microprocessors, FPGAs, a GPS baseband engine, digital circuits in continuous-time signal processing hardware, etc.
  * 2010, planning for all the missing pieces and language modifications to ACT v0 started
  * 2011, initial template for the core language based on ACT v0 developed
  * 2017, actual implementation of the new features started
  * 2018, Most features of ACT v1 ready
  


Synopsys linkages:

If you would like to build the appropriate shared object file
for creating the ALINT trace file format using HSIM, then
you will need to do the following:
   - copy over coi.h and NSOutputInt.h to the common/ directory
   - uncomment two lines: 
	#LIBS2=....
     and
        #OBJS2=...

