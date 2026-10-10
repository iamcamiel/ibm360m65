# Run with the ISE 14.7 environment loaded: xtclsh tools/build_fpga.tcl ROOT
if {$argc != 1} { error "usage: build_fpga.tcl /path/to/ibm360m65" }
set root [file normalize [lindex $argv 0]]
cd $root/xise/ipcore_dir
puts [exec coregen -b endpoint_blk_plus_v1_15.xco -p coregen.cgp -r 2>@1]
cd $root
puts [exec xtclsh $root/tools/stamp_fpga_build.tcl $root/src/vhdl/pcie/fpga_build.vhd 2>@1]
puts [exec xtclsh $root/tools/distribute_ros_address.tcl $root/gen/ald/360_rx.vhd 2>@1]
set project_path $root/xise/ibm360.xise
set file [open $project_path r]
set project_xml [read $file]
close $file
# The board UCF already includes the PCIe physical and timing constraints.
# The generic endpoint UCF selects different pins and overrides the board file
# when ISE passes both files to ngdbuild. Remove it before opening the project.
set generic_ucf {<file xil_pn:name="../src/vhdl/pcie/xilinx_pci_exp_blk_plus_1_lane_ep_xc5vlx110t-ff1136-1.ucf"[^>]*>[^<]*(?:<association[^>]*/>[^<]*)*</file>}
if {[regsub $generic_ucf $project_xml {} project_xml]} {
    set file [open $project_path w]
    puts -nonewline $file $project_xml
    close $file
    puts "PCIE_BOARD_CONSTRAINTS removed generic endpoint UCF"
}
set sources {}
foreach {entry path} [regexp -all -inline {<file xil_pn:name="([^"]+)"} $project_xml] {
    lappend sources [file normalize [file join $root/xise $path]]
}
proc add_source {path} {
    global sources
    set path [file normalize $path]
    if {[lsearch -exact $sources $path] < 0} {
        xfile add $path
        lappend sources $path
    }
}
project open $project_path
add_source $root/src/vhdl/pcie/fpga_build.vhd
add_source $root/src/vhdl/core_clock100.vhd
add_source $root/src/vhdl/cdc_mailbox.vhd
foreach path [glob $root/xise/ipcore_dir/endpoint_blk_plus_v1_15/source/*.v] { add_source $path }
foreach name {PIO_TO_CTRL PIO_64_RX_ENGINE PIO_64_TX_ENGINE} {
    add_source $root/xise/ipcore_dir/endpoint_blk_plus_v1_15/example_design/$name.vhd
}
project set {Implementation Top} {Architecture|IBM360|Behavioral}
project set {Target UCF File Name} $root/src/ucf/ibm360.ucf
# Keep register duplication available to synthesis and placement.
project set {Register Duplication} true -process {Synthesize - XST}
project set {Register Duplication} On -process {Map}
project set {Enable Multi-Threading} 2 -process {Map}
project set {Enable Multi-Threading} 4 -process {Place & Route}
puts "ROS_ADDRESS_DISTRIBUTION XST_MAX_FANOUT=DEFAULT MAP_MAX_FANOUT=DEFAULT MAP_THREADS=2 PAR_THREADS=4"
set result [process run {Generate Programming File}]
puts "BUILD_RESULT $result"
project close
if {!$result} { exit 1 }
# Build completion alone does not prove that the PCIe connector was selected.
puts [exec python $root/tools/audit_pcie_pins.py --pcf $root/xise/IBM360.pcf --json $root/xise/pcie-pin-audit.json 2>@1]
