#!/bin/sh

mkdir -p /usr/local/cad/bin/Tk/Irsim/
cat > /usr/local/cad/bin/Tk/Irsim/TkStartup.tcl << EOF
set ix [lsearch \$argv -display]
if {\$ix >= 0} {
    incr ix
    set env(DISPLAY) [lindex \$argv \$ix]
    set argc 0
    set argv {}
# source /usr/local/cad/bin/Tk/TkPool/TkPool.tcl
exec /usr/local/lib/irsim/tcl/tkcon.tcl \\
	-eval "source /usr/local/lib/irsim/tcl/console.tcl" \\
	-slave "package require Tk; set argc $#; set argv { $@ }; \\
	source /usr/local/lib/irsim/tcl/irsim.tcl"
EOF
cd /usr/local/cad/bin
./tclkit CloudTk.kit
exit $?
