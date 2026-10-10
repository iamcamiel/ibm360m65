library ieee;
use ieee.std_logic_1164.all;

-- Negative-input, negative-output one-shot. One tick is one 10 ns core edge.
-- A held low input triggers once; pulse expiry does not require its release.
entity NSSN5500US is
  port (clk, rst, hlt, i : in std_logic; o : out std_logic);
end NSSN5500US;

architecture Behavioral of NSSN5500US is
  signal remaining : integer range 0 to 549999 := 0;
  signal previous_input : std_logic := '1';
begin
  process(clk)
  begin
    if rising_edge(clk) then
      if rst = '1' then
        remaining <= 0;
        previous_input <= '1';
        o <= '1';
      elsif hlt = '0' then
        previous_input <= i;
        if remaining /= 0 then
          remaining <= remaining - 1;
          o <= '0';
        elsif previous_input = '1' and i = '0' then
          remaining <= 549999;
          o <= '0';
        else
          o <= '1';
        end if;
      end if;
    end if;
  end process;
end Behavioral;
