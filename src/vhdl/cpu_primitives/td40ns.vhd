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


entity TD40NS is
    Port ( clk : in  STD_LOGIC;
			  rst : in STD_LOGIC;
			  hlt : in STD_LOGIC;
			  i : in STD_LOGIC;
			  o : buffer STD_LOGIC
           );
end TD40NS;

architecture Behavioral of TD40NS is
  signal td : STD_LOGIC_VECTOR(1 to 3) := "000"; 
begin
	process (clk)
	begin
		if (clk'event and clk = '1') then
			if (rst='1') then
				o<='0';
				td <= (others=>'0');
			elsif (hlt='0') then
				o <= td(3);
				td(3) <= td(2);
				td(2) <= td(1);
				td(1) <= i;
			end if;
		end if;
	end process;
end Behavioral;
