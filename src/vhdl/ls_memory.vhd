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


entity LS_MEMORY is
    Port ( clk : in STD_LOGIC;
			  rst : in STD_LOGIC;
			  hlt : in STD_LOGIC;
	        r_a : in STD_LOGIC_VECTOR (0 to 4);
			  w_a : in STD_LOGIC_VECTOR (0 to 4);
			  w_g : in STD_LOGIC;
			  w_d : in STD_LOGIC_VECTOR(0 to 31);
			  w_p : in STD_LOGIC_VECTOR(0 to 3);
			  r_d : buffer STD_LOGIC_VECTOR(0 to 31);
			  r_p : buffer STD_LOGIC_VECTOR(0 to 3)
           );
end LS_MEMORY;

architecture Behavioral of LS_MEMORY is
  type d_mem is array(0 to 31) of std_logic_vector(0 to 31);
  type p_mem is array(0 to 31) of std_logic_vector(0 to 3);
  signal d : d_mem;
  signal p : p_mem;
begin
	process (clk)
	begin
		if (clk'event and clk = '1') then
			if (rst='1') then
				r_d <= (others=>'0');
				r_p <= (others=>'0');
				d <= (others=>(others=>'0'));
				p <= (others=>(others=>'0'));
			elsif (hlt='0') then
				r_d <= d(to_integer(unsigned(r_a)));
				r_p <= p(to_integer(unsigned(r_a)));
				if (w_g = '1') then
					d(to_integer(unsigned(w_a))) <= w_d;
					p(to_integer(unsigned(w_a))) <= w_p;
				end if;
			end if;
		end if;
	end process;
end Behavioral;
