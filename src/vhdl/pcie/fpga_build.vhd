library ieee;
use ieee.std_logic_1164.all;

-- Keep interface and FPGA revisions in step with hercules/m65_fpga_version.h.
-- stamp_fpga_build.tcl fills the UTC timestamp before synthesis.
package fpga_build is
  constant M65_INTERFACE_VERSION : std_logic_vector(31 downto 0) := x"00010002";
  constant M65_FPGA_VERSION : std_logic_vector(31 downto 0) := x"00010001";
  constant M65_BUILD_MAGIC : std_logic_vector(31 downto 0) := x"4D363542"; -- M65B
  constant M65_BUILD_DATE : std_logic_vector(31 downto 0) := x"00000000"; -- YYYYMMDD BCD, UTC
  constant M65_BUILD_TIME : std_logic_vector(31 downto 0) := x"00000000"; -- 00HHMMSS BCD, UTC
end package;
