#!/bin/sh

mkdir -p $PWD/Tk/Irsim/
cat > $PWD/Tk/Irsim/TkStartup.tcl << EOF
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
}
EOF

# Just make local copies
cp -n /usr/local/cad/bin/tclkit .
cp -n /usr/local/cad/bin/CloudTk.kit .

# Default port 8015 tcp
./tclkit ./CloudTk.kit 
exit $?
