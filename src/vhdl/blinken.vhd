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
   generic (BLINK_HALF_CYCLES : positive := 50000000); -- 0.5 s at 100 MHz
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

			  panel_snapshot_o : out STD_LOGIC_VECTOR(735 downto 0);
			  panel_valid_o : out STD_LOGIC;
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

  -- Observation only: reconstruct the exact bits presented on each SCK edge.
  type led_banks is array (0 to 5) of std_logic_vector(0 to 39);
  type switch_banks is array (0 to 7) of std_logic_vector(0 to 23);
  signal scanned_switches : switch_banks := (others => (others => '1'));
  signal serial_output : std_logic_vector(0 to 5) := (others => '0');
  signal blink_count : natural range 0 to BLINK_HALF_CYCLES-1 := 0;
  signal waiting_blink : std_logic := '1';
  signal serialized_leds : led_banks := (others => (others => '0'));
  signal frame_generation : unsigned(31 downto 0) := (others => '0');
  signal pwr : STD_LOGIC := '0';
  -- The reset/startup scan precedes the first input-chain latch pulse.
  signal scan_armed : std_logic := '0';
begin
	process (clk, rst_i)
        variable captured : std_logic_vector(735 downto 0);
        variable switches : std_logic_vector(0 to 191);
        variable next_power : std_logic;
	begin
        if rst_i = '1' then
            switch_meta <= (others => '1'); switch_sync <= (others => '1');
            serialized_leds <= (others => (others => '0'));
            frame_generation <= (others => '0');
            panel_snapshot_o <= (others => '0'); panel_valid_o <= '0';
            blink_count <= 0; waiting_blink <= '1';
            clk2 <= '0'; counter <= 0; pwr <= '0';
            scan_armed <= '0';
            l0 <= (others => '0'); l1 <= (others => '0'); l2 <= (others => '0');
            l3 <= (others => '0'); l4 <= (others => '0'); l5 <= (others => '0');
            scanned_switches(0) <= (others => '1'); scanned_switches(1) <= (others => '1');
            scanned_switches(2) <= (others => '1'); scanned_switches(3) <= (others => '1');
            scanned_switches(4) <= (others => '1'); scanned_switches(5) <= (others => '1');
            scanned_switches(6) <= (others => '1'); scanned_switches(7) <= (others => '1');
            disp_clk_o <= '0'; disp_latch_n_o <= '1'; serial_output <= (others => '0');
        elsif rising_edge(clk) then
            switch_meta <= disp_shift_i; switch_sync <= switch_meta;
            panel_valid_o <= '0';
            if configured = '0' then pwr <= '0'; end if;
            -- Configuration waits blink only the red Power Off lamp pair.
            -- This counter does not gate or change the serial scan clock.
            if configured = '1' then
                blink_count <= 0; waiting_blink <= '1';
            elsif blink_count = BLINK_HALF_CYCLES-1 then
                blink_count <= 0; waiting_blink <= not waiting_blink;
            else
                blink_count <= blink_count + 1;
            end if;
            if enable_i = '1' then
            if clk2 = '1' and counter = 41 then
                -- Apply active-high power buttons once per complete input frame.
                -- Off wins simultaneous presses; configuration loss holds reset.
                next_power := pwr;
                if configured = '0' or scan_armed = '0' then
                    next_power := '0';
                elsif scanned_switches(0)(12) = '1' then
                    next_power := '0';
                elsif scanned_switches(0)(11) = '1' then
                    next_power := '1';
                end if;
                pwr <= next_power;
                scan_armed <= '1';
                -- Freeze all six LED words for the entire following serial frame.
			if (configured = '0') then
				l0 <= (others=>'0');
				l1 <= (others=>'0');
				l2 <= (others=>'0');
				l3 <= (38 => waiting_blink, 39 => waiting_blink, others => '0');
				l4 <= (others=>'0');
				l5 <= (others=>'0');
			elsif (next_power = '0') then
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
            end if;


			if (clk2 = '1') then
				if (counter = 41) then
                    -- A complete scan, after all 24 inputs and 40 outputs.
                    -- DWORD 1: generation; 2: status; 3: enable divider.
                    -- DWORDs 4..11: switches; 12..23: low/high LED pairs.
                    -- Panel bit N always maps to integer bit N in these words.
                    captured := (others => '0');
                    captured(31 downto 0) := std_logic_vector(frame_generation + 1);
                    captured(32) := configured;
                    captured(33) := next_power;
                    captured(34) := not next_power; -- ALD panel reset
                    captured(35) := '1'; -- complete frame
                    captured(36) := scanned_switches(0)(11); -- Power On, active high input
                    captured(37) := scanned_switches(0)(12); -- Power Off
                    captured(38) := scanned_switches(0)(14); -- Load
                    captured(39) := waiting_blink and not configured;
                    captured(57) := '1'; -- latch high at this completed frame
                    for bank in 0 to 7 loop
                        captured(40+bank) := switch_sync(bank);
                        captured(48+bank) := switch_meta(bank);
                    end loop;
                    captured(95 downto 64) := std_logic_vector(to_unsigned(512,32));
                    switches := scanned_switches(0) & scanned_switches(1) & scanned_switches(2) & scanned_switches(3) & scanned_switches(4) & scanned_switches(5) & scanned_switches(6) & scanned_switches(7);
                    for bank in 0 to 7 loop
                        for bitno in 0 to 23 loop
                            captured((3+bank)*32+bitno) := switches(bank*24+bitno);
                        end loop;
                    end loop;
                    for bank in 0 to 5 loop
                        for bitno in 0 to 39 loop
                            captured((11+bank*2)*32+bitno) := serialized_leds(bank)(bitno);
                        end loop;
                    end loop;
                    panel_snapshot_o <= captured; panel_valid_o <= '1';
                    frame_generation <= frame_generation + 1;
					disp_latch_n_o <= '1';
					counter <= 0;
				else
					if (counter < 40) then
                        for bank in 0 to 5 loop
                            serialized_leds(bank)(39-counter) <= serial_output(bank);
                        end loop;
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
					if (counter < 24) then
						scanned_switches(0)(23-counter) <= switch_sync(0);
						scanned_switches(1)(23-counter) <= switch_sync(1);
						scanned_switches(2)(23-counter) <= switch_sync(2);
						scanned_switches(3)(23-counter) <= switch_sync(3);
						scanned_switches(4)(23-counter) <= switch_sync(4);
						scanned_switches(5)(23-counter) <= switch_sync(5);
						scanned_switches(6)(23-counter) <= switch_sync(6);
						scanned_switches(7)(23-counter) <= switch_sync(7);
					end if;
					if (counter < 40) then
						serial_output(0) <= l0(39-counter);
						serial_output(1) <= l1(39-counter);
						serial_output(2) <= l2(39-counter);
						serial_output(3) <= l3(39-counter);
						serial_output(4) <= l4(39-counter);
						serial_output(5) <= l5(39-counter);
					else
						serial_output <= "000000";
					end if;
				end if;
				clk2 <= '1';
			end if;
		    end if; -- display enable
		end if;
	end process;

	disp_shift_o <= serial_output;
	sw0 <= scanned_switches(0);
	sw1 <= scanned_switches(1);
	sw2 <= scanned_switches(2);
	sw3 <= scanned_switches(3);
	sw4 <= scanned_switches(4);
	sw5 <= scanned_switches(5);
	sw6 <= scanned_switches(6);
	sw7 <= scanned_switches(7);
	power_off <= not pwr;
end Behavioral;

