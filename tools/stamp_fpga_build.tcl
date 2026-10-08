# Run with xtclsh (ISE 14.7), or tclsh, immediately before synthesis.
# An explicit SOURCE_DATE_EPOCH makes the timestamp reproducible.
if {$argc != 1} { error "usage: stamp_fpga_build.tcl /path/to/fpga_build.vhd" }
set path [lindex $argv 0]
set epoch [clock seconds]
if {[info exists env(SOURCE_DATE_EPOCH)]} { set epoch $env(SOURCE_DATE_EPOCH) }
set date [clock format $epoch -gmt 1 -format %Y%m%d]
set time [clock format $epoch -gmt 1 -format 00%H%M%S]
set file [open $path r]
set content [read $file]
close $file
foreach {name value} [list M65_BUILD_DATE $date M65_BUILD_TIME $time] {
    set pattern [format {(constant %s[^\n]*:= x")[0-9A-Fa-f]{8}(";)} $name]
    if {[regsub -all $pattern $content "\\1${value}\\2" content] != 1} {
        error "missing or duplicate build constant $name in $path"
    }
}
set file [open $path w]
puts -nonewline $file $content
close $file
puts "FPGA build timestamp: $date $time UTC"
