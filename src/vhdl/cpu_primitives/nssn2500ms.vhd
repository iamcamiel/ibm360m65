-- * IBM 360 Model 65 Emulator
-- * Copyright (C) 2024 Camiel Vanderhoeven
-- *
-- * This program is free software: you can redistribute it and/or modify
-- * it under the terms of the GNU General Public License as published by
-- * the Free Software Foundation, either version 3 of the License, or
-- * (at your option) any later version.
-- *
-- * This program is distributed in the hope that it will be useful,
-- * but WITHOUT ANY WARRANTY; without even the implied warranty of
-- * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
-- * GNU General Public License for more details.
-- *
-- * You should have received a copy of the GNU General Public License
-- * along with this program.  If not, see <http://www.gnu.org/licenses/>.

library ieee;
use ieee.std_logic_1164.all;

-- Non-retriggerable one-shot; one enabled core edge is 10 ns.
-- Input edges during a pulse do not extend it. Held input does not hold output.
entity NSSN2500MS is
  port (clk, rst, hlt, i : in std_logic; o : buffer std_logic);
end NSSN2500MS;

architecture Behavioral of NSSN2500MS is
  signal remaining : integer range 0 to 249999999 := 0;
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
          remaining <= 249999999;
          o <= '0';
        else
          o <= '1';
        end if;
      end if;
    end if;
  end process;
end Behavioral;
