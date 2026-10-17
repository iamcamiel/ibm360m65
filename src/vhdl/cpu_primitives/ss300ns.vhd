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
entity SS300NS is
  port (clk, rst, hlt, i : in std_logic; o : buffer std_logic);
end SS300NS;

architecture Behavioral of SS300NS is
  signal remaining : integer range 0 to 29 := 0;
  signal previous_input : std_logic := '0';
begin
  process(clk)
  begin
    if rising_edge(clk) then
      if rst = '1' then
        remaining <= 0;
        previous_input <= '0';
        o <= '0';
      elsif hlt = '0' then
        previous_input <= i;
        if remaining /= 0 then
          remaining <= remaining - 1;
          o <= '1';
        elsif previous_input = '0' and i = '1' then
          remaining <= 29;
          o <= '1';
        else
          o <= '0';
        end if;
      end if;
    end if;
  end process;
end Behavioral;
