library ieee;
use ieee.std_logic_1164.all;

-- FPGA revision 1.10 corrects KW093 logout pulse polarity and clocked timing.
-- Interface 1.5 corrects the active-high panel pressed flags.
-- stamp_fpga_build.tcl fills the UTC timestamp before synthesis.
package fpga_build is
  constant M65_INTERFACE_VERSION : std_logic_vector(31 downto 0) := x"00010005";
  constant M65_FPGA_VERSION : std_logic_vector(31 downto 0) := x"0001000A";
  constant M65_BUILD_MAGIC : std_logic_vector(31 downto 0) := x"4D363542"; -- M65B
  constant M65_BUILD_DATE : std_logic_vector(31 downto 0) := x"00000000"; -- YYYYMMDD BCD, UTC
  constant M65_BUILD_TIME : std_logic_vector(31 downto 0) := x"00000000"; -- 00HHMMSS BCD, UTC
end package;
