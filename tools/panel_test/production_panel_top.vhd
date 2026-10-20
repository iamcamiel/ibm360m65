library ieee;
use ieee.std_logic_1164.all;
library unisim;
use unisim.vcomponents.all;
entity PRODUCTION_PANEL_TOP is
  port(clk_fpga_p,clk_fpga_n : in std_logic;
    disp_shift_i : in std_logic_vector(0 to 7);
    disp_clk_o,disp_latch_n_o : out std_logic;
    disp_shift_o : out std_logic_vector(0 to 5);
    led_4 : out std_logic_vector(0 to 3));
end;
architecture RTL of PRODUCTION_PANEL_TOP is
  signal clk200,clk100,ready,reset,power_off,phase,parity : std_logic;
  signal switches : std_logic_vector(191 downto 0);
begin
  input_clock : IBUFGDS generic map(IOSTANDARD=>"LVDS_25")
    port map(I=>clk_fpga_p,IB=>clk_fpga_n,O=>clk200);
  core_clock : entity work.CORE_CLOCK100
    port map(clk200_i=>clk200,reset_i=>'0',clk100_o=>clk100,ready_o=>ready);
  reset<=not ready;
  diagnostic : entity work.PRODUCTION_PANEL_CORE
    port map(clk=>clk100,reset=>reset,shift_i=>disp_shift_i,
      sck=>disp_clk_o,latch_n=>disp_latch_n_o,shift_o=>disp_shift_o,
      power_off=>power_off,phase_o=>phase,switch_parity=>parity,switches_o=>switches);
  led_4<=ready & not power_off & parity & phase;
end;
