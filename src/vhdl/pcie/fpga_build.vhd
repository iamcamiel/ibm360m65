library ieee;
use ieee.std_logic_1164.all;

-- FPGA revision 1.41 evaluates KS021/KS121 wire cones within each Boolean pass.
-- The revision 1.40 diagnostic history write pipeline is retained.
-- Interface 1.7 retains ROSAR breakpoints, stepping and event metadata unchanged.
-- stamp_fpga_build.tcl fills the UTC timestamp before synthesis.
package fpga_build is
  constant M65_INTERFACE_VERSION : std_logic_vector(31 downto 0) := x"00010007";
  constant M65_FPGA_VERSION : std_logic_vector(31 downto 0) := x"00010029";
  constant M65_BUILD_MAGIC : std_logic_vector(31 downto 0) := x"4D363542"; -- M65B
  constant M65_BUILD_DATE : std_logic_vector(31 downto 0) := x"00000000"; -- YYYYMMDD BCD, UTC
  constant M65_BUILD_TIME : std_logic_vector(31 downto 0) := x"00000000"; -- 00HHMMSS BCD, UTC
end package;
