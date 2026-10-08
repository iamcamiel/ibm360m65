library ieee;
use ieee.std_logic_1164.all;
library unisim;
use unisim.vcomponents.all;

-- Core clock: board input 200 MHz, VCO 800 MHz, core 100 MHz.
entity CORE_CLOCK100 is
  port (clk200_i : in std_logic; reset_i : in std_logic;
        clk100_o : out std_logic; ready_o : out std_logic);
end CORE_CLOCK100;

architecture RTL of CORE_CLOCK100 is
  signal feedback_raw, feedback, clock_raw, clock100, locked : std_logic;
  signal ready_sync : std_logic_vector(1 downto 0) := "00";
  attribute ASYNC_REG : string;
  attribute ASYNC_REG of ready_sync : signal is "TRUE";
begin
  pll : PLL_BASE
    generic map (BANDWIDTH => "OPTIMIZED", CLKFBOUT_MULT => 4,
      CLKIN_PERIOD => 5.0, CLKOUT0_DIVIDE => 8, CLKOUT0_DUTY_CYCLE => 0.5,
      COMPENSATION => "SYSTEM_SYNCHRONOUS", DIVCLK_DIVIDE => 1,
      REF_JITTER => 0.010)
    port map (CLKIN => clk200_i, CLKFBIN => feedback,
      CLKFBOUT => feedback_raw, CLKOUT0 => clock_raw,
      CLKOUT1 => open, CLKOUT2 => open, CLKOUT3 => open,
      CLKOUT4 => open, CLKOUT5 => open, LOCKED => locked, RST => reset_i);
  feedback_buffer : BUFG port map (I => feedback_raw, O => feedback);
  core_buffer : BUFG port map (I => clock_raw, O => clock100);
  -- Assert reset on lock loss; release only after two stable core edges.
  process(clock100, locked, reset_i)
  begin
    if locked = '0' or reset_i = '1' then
      ready_sync <= "00";
    elsif rising_edge(clock100) then
      ready_sync <= ready_sync(0) & '1';
    end if;
  end process;
  clk100_o <= clock100;
  ready_o <= ready_sync(1);
end RTL;
