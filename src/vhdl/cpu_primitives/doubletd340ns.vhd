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


entity DOUBLETD340NS is
    Port ( clk : in  STD_LOGIC;
			  rst : in STD_LOGIC;
			  hlt : in STD_LOGIC;
			  i : in STD_LOGIC;
			  o : buffer STD_LOGIC
           );
end DOUBLETD340NS;

architecture Behavioral of DOUBLETD340NS is
  signal td : STD_LOGIC_VECTOR(1 to 33) := (others=> '0'); 
begin
	process (clk)
	begin
		if (clk'event and clk = '1') then
			if (rst='1') then
				o<='0';
				td <= (others=>'0');
			elsif (hlt='0') then
				o <= td(33);
				td(33) <= td(32);
				td(32) <= td(31);
				td(31) <= td(30);
				td(30) <= td(29);
				td(29) <= td(28);
				td(28) <= td(27);
				td(27) <= td(26);
				td(26) <= td(25);
				td(25) <= td(24);
				td(24) <= td(23);
				td(23) <= td(22);
				td(22) <= td(21);
				td(21) <= td(20);
				td(20) <= td(19);
				td(19) <= td(18);
				td(18) <= td(17);
				td(17) <= td(16);
				td(16) <= td(15);
				td(15) <= td(14);
				td(14) <= td(13);
				td(13) <= td(12);
				td(12) <= td(11);
				td(11) <= td(10);
				td(10) <= td(9);
				td(9) <= td(8);
				td(8) <= td(7);
				td(7) <= td(6);
				td(6) <= td(5);
				td(5) <= td(4);
				td(4) <= td(3);
				td(3) <= td(2);
				td(2) <= td(1);
				td(1) <= i;
			end if;
		end if;
	end process;
end Behavioral;
