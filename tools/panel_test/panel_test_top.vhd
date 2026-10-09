library ieee;
use ieee.std_logic_1164.all;
library unisim;
use unisim.vcomponents.all;

entity PANEL_TEST_TOP is
  port (clk_fpga_p, clk_fpga_n : in std_logic;
        disp_clk_o, disp_latch_n_o : out std_logic;
        disp_shift_o : out std_logic_vector(0 to 5);
        led_4 : out std_logic_vector(0 to 3));
end PANEL_TEST_TOP;

architecture RTL of PANEL_TEST_TOP is
  signal clk200, clk100, ready, reset, sck, latch_n, phase : std_logic;
begin
  input_clock : IBUFGDS generic map(IOSTANDARD=>"LVDS_25")
    port map(I=>clk_fpga_p,IB=>clk_fpga_n,O=>clk200);
  core_clock : entity work.CORE_CLOCK100
    port map(clk200_i=>clk200,reset_i=>'0',clk100_o=>clk100,ready_o=>ready);
  reset<=not ready;
  panel : entity work.PANEL_TEST_SERIAL
    generic map(HALF_CYCLES=>500)
    port map(clk=>clk100,reset=>reset,sck=>sck,latch_n=>latch_n,
             data=>disp_shift_o,pattern_o=>phase);
  disp_clk_o<=sck; disp_latch_n_o<=latch_n;
  led_4<=ready & phase & sck & latch_n;
end RTL;
