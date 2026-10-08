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

library IEEE;
use IEEE.STD_LOGIC_1164.ALL;
use IEEE.NUMERIC_STD.ALL;
USE ieee.std_logic_arith.all;


entity DOUBLESS200NS is
    Port ( clk : in  STD_LOGIC;
			  rst : in STD_LOGIC;
			  hlt : in STD_LOGIC;
		     i : in STD_LOGIC;
			  o : buffer STD_LOGIC
           );
end DOUBLESS200NS;

architecture Behavioral of DOUBLESS200NS is
    signal cnt : integer range 0 to 19 := 0;
	 signal triggered : STD_LOGIC := '0';
begin
	process (clk)
	begin
		if (clk'event and clk = '1') then
			if (rst='1') then
				o<='0';
				cnt <= 0;
				triggered <= '0';
			elsif (hlt='0') then
				if (cnt = 0) then
					if (i='0') then
						triggered <= '0';
						o <= '0';
					else
						if (triggered = '0') then
							triggered <= '1';
							cnt <= 19;
							o <= '1';
						end if;
					end if;
				else
					o <= '1';
					cnt <= cnt - 1;
				end if;
			end if;	 
		end if;
	end process;
end Behavioral;
