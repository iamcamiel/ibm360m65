library ieee;
use ieee.std_logic_1164.all;
use ieee.numeric_std.all;

entity test_p0_panel_serial is end;
architecture test of test_p0_panel_serial is
  signal core : std_logic := '0';
  signal reset : std_logic := '1';
  signal halt, p0, enable : std_logic := '0';
  signal divider : natural range 0 to 7 := 0;
  signal di : std_logic_vector(0 to 7) := (others => '1');
  signal dout : std_logic_vector(0 to 5);
  signal sck, latch_n, off, valid, selected : std_logic;
  signal payload : std_logic_vector(735 downto 0);
  type sw_banks is array(0 to 7) of std_logic_vector(0 to 23);
  signal inputs : sw_banks := (others => (others => '1'));
  signal switches : sw_banks;
  signal lamps : std_logic_vector(0 to 39);
  signal serial_lamps : std_logic_vector(0 to 39) := (others => '0');
  signal checked_frames : natural := 0;
  signal done : boolean := false;
begin
  process begin while not done loop core <= not core; wait for 5 ns; end loop; wait; end process;
  process begin while not done loop p0 <= not p0; wait for 100 ns; end loop; wait; end process;
  enable <= '1' when divider = 7 and reset = '0' else '0';
  process(core)
  begin
    if rising_edge(core) then
      if reset = '1' or divider = 7 then divider <= 0; else divider <= divider + 1; end if;
    end if;
  end process;
  selected <= '1' when switches(4)(17 to 21) = "11111" else '0';
  counter : entity work.P0_PANEL_COUNTER port map
    (core, off, halt, p0, selected, x"000000000A", lamps);
  panel : entity work.BLINKEN port map
    (clk => core, rst_i => reset, enable_i => enable,
     disp_clk_o => sck, disp_latch_n_o => latch_n, disp_shift_o => dout, disp_shift_i => di,
     sw0 => switches(0), sw1 => switches(1), sw2 => switches(2), sw3 => switches(3),
     sw4 => switches(4), sw5 => switches(5), sw6 => switches(6), sw7 => switches(7),
     li0 => (others => '0'), li1 => (others => '0'), li2 => (others => '0'),
     li3 => (others => '0'), li4 => lamps, li5 => (others => '0'),
     configured => '1', power_off => off, panel_snapshot_o => payload, panel_valid_o => valid);
  -- Same independent physical input-chain model used by the panel regression.
  process(sck, latch_n, reset, inputs)
    variable n : natural range 0 to 24 := 0;
  begin
    if reset = '1' or latch_n = '0' then n := 0;
    elsif rising_edge(sck) then if n < 24 then n := n + 1; end if; end if;
    for b in 0 to 7 loop
      if n < 24 then di(b) <= inputs(b)(23-n); else di(b) <= '1'; end if;
    end loop;
  end process;
  process(sck, latch_n, reset)
    variable index : natural range 0 to 39 := 39;
  begin
    if reset = '1' then index := 39; serial_lamps <= (others => '0');
    elsif falling_edge(latch_n) then index := 39;
    elsif rising_edge(sck) then
      serial_lamps(index) <= dout(4);
      if index > 0 then index := index-1; end if;
    end if;
  end process;
  -- Compare the actual shifted bits with the complete word from the preceding
  -- frame latch, even though the live binary counter changes during the frame.
  process(core)
    variable frame_word, previous_lamps : std_logic_vector(0 to 39) := (others => '0');
  begin
    if rising_edge(core) then
      if reset = '1' then frame_word := (others => '0'); previous_lamps := (others => '0');
      else
        if valid = '1' then
          for i in 0 to 39 loop
            assert serial_lamps(i) = frame_word(i)
              report "Torn counter or wrong physical serial bit order" severity failure;
            assert payload(608+i) = serial_lamps(i)
              report "Panel snapshot differs from actual roller5 serial output" severity failure;
          end loop;
          if off = '1' then frame_word := (others => '0'); else frame_word := previous_lamps; end if;
          checked_frames <= checked_frames+1;
        end if;
        previous_lamps := lamps;
      end if;
    end if;
  end process;
  process
    variable held : std_logic_vector(0 to 35);
  begin
    inputs(0) <= (others => '0'); inputs(0)(11) <= '1';
    wait for 37 ns; reset <= '0';
    wait for 60 us; assert off = '0' and selected = '1' severity failure;
    inputs(0)(11) <= '0'; halt <= '1'; wait for 20 ns;
    held := lamps(0 to 35); wait for 40 us;
    assert lamps(0 to 35) = held report "Halt did not hold displayed count" severity failure;
    halt <= '0'; wait for 60 us;
    assert unsigned(lamps(0 to 35)) > unsigned(held) report "Count did not resume" severity failure;
    inputs(0)(12) <= '1'; wait for 30 us;
    assert off = '1' and lamps(0 to 35) = x"000000000" report "Power Off did not clear count" severity failure;
    assert checked_frames >= 20 severity failure;
    report "P0_PANEL_SERIAL_TEST_PASS" severity note;
    done <= true; wait;
  end process;
end test;
