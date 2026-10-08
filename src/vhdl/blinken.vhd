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


entity BLINKEN is
   port (
  			  clk : in  STD_LOGIC;
              rst_i, enable_i : in STD_LOGIC;

			  disp_clk_o     : out STD_LOGIC;
			  disp_latch_n_o : out STD_LOGIC;
			  disp_shift_o   : out STD_LOGIC_VECTOR(0 to 5);
			  disp_shift_i   : in  STD_LOGIC_VECTOR(0 to 7);

			  sw0     : out STD_LOGIC_VECTOR(0 to 23);
			  sw1     : out STD_LOGIC_VECTOR(0 to 23);
			  sw2     : out STD_LOGIC_VECTOR(0 to 23);
			  sw3     : out STD_LOGIC_VECTOR(0 to 23);
			  sw4     : out STD_LOGIC_VECTOR(0 to 23);
			  sw5     : out STD_LOGIC_VECTOR(0 to 23);
			  sw6     : out STD_LOGIC_VECTOR(0 to 23);
			  sw7     : out STD_LOGIC_VECTOR(0 to 23);

			  li0     : in STD_LOGIC_VECTOR(0 to 39);
			  li1     : in STD_LOGIC_VECTOR(0 to 39);
			  li2     : in STD_LOGIC_VECTOR(0 to 39);
			  li3     : in STD_LOGIC_VECTOR(0 to 39);
			  li4     : in STD_LOGIC_VECTOR(0 to 39);
			  li5     : in STD_LOGIC_VECTOR(0 to 39);

			  configured : in STD_LOGIC;

			  power_off : out STD_LOGIC
		   );
end BLINKEN;

architecture Behavioral of BLINKEN is
  signal clk2 : std_logic := '0';
  signal switch_meta, switch_sync : std_logic_vector(0 to 7) := (others => '1');
  attribute ASYNC_REG : string;
  attribute SHREG_EXTRACT : string;
  attribute ASYNC_REG of switch_meta, switch_sync : signal is "TRUE";
  attribute SHREG_EXTRACT of switch_meta, switch_sync : signal is "NO";
  signal counter : integer range 0 to 41 := 0;

  signal l0 : STD_LOGIC_VECTOR(0 to 39);
  signal l1 : STD_LOGIC_VECTOR(0 to 39);
  signal l2 : STD_LOGIC_VECTOR(0 to 39);
  signal l3 : STD_LOGIC_VECTOR(0 to 39);
  signal l4 : STD_LOGIC_VECTOR(0 to 39);
  signal l5 : STD_LOGIC_VECTOR(0 to 39);

  signal pwr : STD_LOGIC := '0';
begin
	process (clk, rst_i)
	begin
        if rst_i = '1' then
            switch_meta <= (others => '1'); switch_sync <= (others => '1');
            clk2 <= '0'; counter <= 0; pwr <= '0';
            l0 <= (others => '0'); l1 <= (others => '0'); l2 <= (others => '0');
            l3 <= (others => '0'); l4 <= (others => '0'); l5 <= (others => '0');
            sw0 <= (others => '1'); sw1 <= (others => '1');
            sw2 <= (others => '1'); sw3 <= (others => '1');
            sw4 <= (others => '1'); sw5 <= (others => '1');
            sw6 <= (others => '1'); sw7 <= (others => '1');
            disp_clk_o <= '0'; disp_latch_n_o <= '1'; disp_shift_o <= (others => '0');
        elsif rising_edge(clk) then
            switch_meta <= disp_shift_i; switch_sync <= switch_meta;
            if enable_i = '1' then
			if (configured = '0') then
				l0 <= (others=>'0');
				l1 <= (others=>'0');
				l2 <= (others=>'0');
				l3 <= (others=>'0');
				l4 <= (others=>'0');
				l5 <= (others=>'0');
			elsif (pwr = '0') then
				l0 <= (others=>'0');
				l1 <= (others=>'0');
				l2 <= (others=>'0');
				l3 <= "0000000000000000000000000000000000000011";
				l4 <= (others=>'0');
				l5 <= (others=>'0');
			else
				l0 <= li0;
				l1 <= li1;
				l2 <= li2;
				l3 <= li3;
				l4 <= li4;
				l5 <= li5;
			end if;


			if (clk2 = '1') then
				if (counter = 41) then
					disp_latch_n_o <= '1';
					counter <= 0;
				else
					if (counter < 40) then
						disp_clk_o <= '1';
					end if;
					counter <= counter + 1;
				end if;
				clk2 <= '0';
			else
				if (counter = 41) then
					disp_latch_n_o <= '0';
				else
					disp_clk_o <= '0';
					if (counter = 12 and switch_sync(0)='0' and configured='1') then
						pwr <= '1';
					elsif ((counter = 11 and switch_sync(0)='0') or configured='0') then
						pwr <= '0';
					end if;
					if (counter < 24) then
						sw0(23-counter) <= switch_sync(0);
						sw1(23-counter) <= switch_sync(1);
						sw2(23-counter) <= switch_sync(2);
						sw3(23-counter) <= switch_sync(3);
						sw4(23-counter) <= switch_sync(4);
						sw5(23-counter) <= switch_sync(5);
						sw6(23-counter) <= switch_sync(6);
						sw7(23-counter) <= switch_sync(7);
					end if;
					if (counter < 40) then
						disp_shift_o(0) <= l0(39-counter);
						disp_shift_o(1) <= l1(39-counter);
						disp_shift_o(2) <= l2(39-counter);
						disp_shift_o(3) <= l3(39-counter);
						disp_shift_o(4) <= l4(39-counter);
						disp_shift_o(5) <= l5(39-counter);
					else
						disp_shift_o <= "000000";
					end if;
				end if;
				clk2 <= '1';
			end if;
		    end if; -- display enable
		end if;
	end process;

	power_off <= not pwr;
end Behavioral;

