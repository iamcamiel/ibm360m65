# Remove the previous blanket synthesis constraint after ALD generation.
# ROS address fanout uses the default synthesis and placement handling.
# Executable RX logic, register latency and clock enables stay unchanged.
if {$argc != 1} { error "usage: distribute_ros_address.tcl /path/to/360_rx.vhd" }
set path [lindex $argv 0]
set file [open $path r]
set content [read $file]
close $file
set declaration {  signal P_temp1203 : STD_LOGIC_VECTOR (0 to 11) := "000000000000";}
if {[llength [regexp -all -inline {signal P_temp1203 :} $content]] != 1 ||
    [string first $declaration $content] < 0} {
    error "ROS address driver declaration changed: inspect $path before constraining it"
}
set attributes {  -- ROS_ADDRESS_DISTRIBUTION_BEGIN
  -- Replicate the existing ROS address registers; no extra pipeline stage.
  attribute max_fanout : integer;
  attribute max_fanout of P_temp1203 : signal is 64;
  -- ROS_ADDRESS_DISTRIBUTION_END}
if {[string first "ROS_ADDRESS_DISTRIBUTION_BEGIN" $content] >= 0} {
    if {[string first $attributes $content] < 0} {
        error "ROS address distribution attributes differ from the requested constraint"
    }
    set content [string map [list "\n$attributes" ""] $content]
    if {[string first "ROS_ADDRESS_DISTRIBUTION_BEGIN" $content] >= 0} {
        error "could not remove the previous ROS address distribution attributes"
    }
    set file [open "$path.tmp" w]
    puts -nonewline $file $content
    close $file
    file rename -force "$path.tmp" $path
}
puts "ROS_ADDRESS_DISTRIBUTION driver=P_temp1203 XST_MAX_FANOUT=DEFAULT MAP_MAX_FANOUT=DEFAULT"
