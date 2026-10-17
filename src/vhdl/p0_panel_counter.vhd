library ieee;
use ieee.std_logic_1164.all;
use ieee.numeric_std.all;

-- Observation only, sampled in the existing 100 MHz core domain.
entity P0_PANEL_COUNTER is
  generic (COUNT_BITS : positive := 36);
  port (clk_i, reset_i, halt_i, p0_i, select_i : in std_logic;
    normal_lamps_i : in std_logic_vector(0 to 39);
    lamps_o : out std_logic_vector(0 to 39));
end P0_PANEL_COUNTER;

architecture rtl of P0_PANEL_COUNTER is
  signal count : unsigned(COUNT_BITS-1 downto 0) := (others => '0');
  signal previous_p0 : std_logic := '0';
begin
  assert COUNT_BITS <= 36 report "P0 panel count must fit the 36 roller lamps" severity failure;
  process(clk_i)
  begin
    if rising_edge(clk_i) then
      -- Releasing a held-high phase must not manufacture an edge.
      previous_p0 <= p0_i;
      if reset_i = '1' then
        count <= (others => '0');
      elsif halt_i = '0' and p0_i = '1' and previous_p0 = '0' then
        count <= count + 1;
      end if;
    end if;
  end process;

  -- Opposite vector directions map the most significant count bit to lamp0.
  lamps_o(0 to 35) <= std_logic_vector(resize(count, 36)) when select_i = '1'
                      else normal_lamps_i(0 to 35);
  lamps_o(36 to 39) <= normal_lamps_i(36 to 39);
end rtl;
