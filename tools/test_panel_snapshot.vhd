library ieee;
use ieee.std_logic_1164.all;
use ieee.numeric_std.all;

entity test_panel_snapshot is end;
architecture test of test_panel_snapshot is
  signal core, pci : std_logic := '0';
  signal reset : std_logic := '1';
  signal done : boolean := false;
  signal configured : std_logic := '0';
  signal di : std_logic_vector(0 to 7) := (others => '1');
  signal enable : std_logic := '0';
  signal divider : natural range 0 to 7 := 0;
  signal dout : std_logic_vector(0 to 5);
  signal sck, latch_n, off, valid, busy : std_logic;
  signal payload : std_logic_vector(735 downto 0);
  type sw_banks is array (0 to 7) of std_logic_vector(0 to 23);
  signal inputs : sw_banks := (others => (others => '0'));
  type led_banks is array (0 to 5) of std_logic_vector(0 to 39);
  signal switches : sw_banks;
  signal lamps : led_banks;
  signal sampled_leds : led_banks := (others => (others => '0'));
  signal serial_index : natural range 0 to 39 := 39;
  signal address, waddress : std_logic_vector(8 downto 0) := (others => '0');
  signal rd, wrd, wd : std_logic_vector(31 downto 0) := (others => '0');
  signal we : std_logic := '0';
  signal io_int, io_resp, hi, lo, resp, size : std_logic_vector(31 downto 0);
begin
  process begin while not done loop core <= not core; wait for 5 ns; end loop; wait; end process;
  process begin wait for 3 ns; while not done loop pci <= not pci; wait for 8 ns; end loop; wait; end process;
  -- Faster generic for simulation; production default is 50,000,000 core edges.
  panel : entity work.BLINKEN generic map (BLINK_HALF_CYCLES => 5000)
    port map (clk => core, rst_i => reset, enable_i => enable,
      disp_clk_o => sck, disp_latch_n_o => latch_n, disp_shift_o => dout, disp_shift_i => di,
      sw0 => switches(0), sw1 => switches(1), sw2 => switches(2), sw3 => switches(3),
      sw4 => switches(4), sw5 => switches(5), sw6 => switches(6), sw7 => switches(7),
      li0 => lamps(0), li1 => lamps(1), li2 => lamps(2), li3 => lamps(3),
      li4 => lamps(4), li5 => lamps(5), configured => configured, power_off => off,
      panel_snapshot_o => payload, panel_valid_o => valid);
  enable <= '1' when divider = 7 and reset = '0' else '0';
  process(core)
  begin
    if rising_edge(core) then
      if reset = '1' or divider = 7 then divider <= 0;
      else divider <= divider + 1; end if;
    end if;
  end process;
  -- Model independent 24-bit input chains, with time for the two-stage synchronizer.
  process(sck, latch_n, reset, inputs)
    variable n : natural range 0 to 24 := 0;
  begin
    if reset = '1' or latch_n = '0' then n := 0;
    elsif rising_edge(sck) then if n < 24 then n := n + 1; end if; end if;
    for b in 0 to 7 loop
      if n < 24 then di(b) <= inputs(b)(23-n); else di(b) <= '1'; end if;
    end loop;
  end process;
  regs : entity work.EP_MEM port map (
    clk_i => pci, core_clk_i => core, cdc_reset_i => reset, cdc_busy_o => busy,
    a_rd_a_i_0 => address, a_rd_d_o_0 => rd, b_wr_a_i_0 => waddress,
    b_wr_d_i_0 => wd, b_wr_en_i_0 => we, b_rd_d_o_0 => wrd,
    panel_snapshot_i => payload, panel_valid_i => valid,
    P_reg_io_int => io_int, P_reg_io_resp => io_resp, P_reg_se_rdata_hi => hi,
    P_reg_se_rdata_lo => lo, P_reg_se_resp => resp, P_reg_se_size => size,
    P_reg_ext => x"10203040", P_reg_io_cmd => x"00000000", P_reg_se_addr => x"00000000",
    P_reg_se_cmd => x"00000000", P_reg_se_wdata_hi => x"00000000", P_reg_se_wdata_lo => x"00000000");
  -- Independent observation of the physical serial outputs at SCK rising edges.
  process(sck, reset, latch_n)
  begin
    if reset = '1' then serial_index <= 39; sampled_leds <= (others => (others => '0'));
    elsif rising_edge(latch_n) then serial_index <= 39;
    elsif rising_edge(sck) then
      for b in 0 to 5 loop sampled_leds(b)(serial_index) <= dout(b); end loop;
      if serial_index > 0 then serial_index <= serial_index-1; end if;
    end if;
  end process;
  process(valid)
  begin
    if rising_edge(valid) then
      for b in 0 to 7 loop
        for n in 0 to 23 loop
          assert payload((3+b)*32+n) = switches(b)(n) report "switch snapshot bit order" severity failure;
        end loop;
      end loop;
      for b in 0 to 5 loop
        for n in 0 to 39 loop
          assert payload((11+2*b)*32+n) = sampled_leds(b)(n)
            report "snapshot differs from actual serialized lamp bits" severity failure;
        end loop;
      end loop;
    end if;
  end process;
  process
    variable before, afterword : std_logic_vector(31 downto 0);
    variable held : std_logic_vector(735 downto 0);
    procedure read_word(n : natural) is
    begin address <= std_logic_vector(to_unsigned(n,9));
      for k in 1 to 3 loop wait until rising_edge(pci); end loop; wait for 1 ns;
    end;
    procedure capture is
    begin read_word(256); assert rd = x"504E4C31" severity failure; read_word(257); end;
    procedure check_red(onoff : std_logic) is
    begin
      capture; before := rd;
      for b in 0 to 5 loop
        read_word(268+2*b); assert rd = x"00000000" report "unexpected waiting low LEDs" severity failure;
        read_word(269+2*b);
        if b = 3 and onoff = '1' then assert rd = x"000000C0" severity failure;
        else assert rd = x"00000000" report "unexpected waiting high LEDs" severity failure; end if;
      end loop;
      read_word(258); assert rd(0) = '0' and rd(1) = '0' and rd(7) = onoff
        report "waiting state must not enable CPU" severity failure;
    end;
  begin
    for b in 0 to 5 loop
      for n in 0 to 39 loop
        if (n+2*b) mod 7 = 0 then lamps(b)(n) <= '1'; else lamps(b)(n) <= '0'; end if;
      end loop;
    end loop;
    wait for 37 ns; reset <= '0';
    wait for 20 us; check_red('1');
    wait for 50 us; check_red('0');
    wait for 50 us; check_red('1');
    configured <= '1'; wait for 15 us;
    capture; read_word(258); assert rd(0) = '1' and rd(7) = '0' and rd(1) = '0' severity failure;
    -- Press Power On and Load; distinct input patterns in the other banks.
    inputs(0)(11) <= '1'; inputs(0)(14) <= '1';
    inputs(2) <= (others => '1'); inputs(4) <= (others => '1'); inputs(6) <= (others => '1');
    wait for 15 us;
    assert off = '0' report "power on not decoded" severity failure;
    capture; read_word(258);
    assert rd(4) = '1' and rd(5) = '0' and rd(6) = '1' report "active-high pressed flags" severity failure;
    inputs(0)(11) <= '0'; inputs(0)(14) <= '0'; wait for 15 us;
    assert off = '0' report "released buttons must retain power" severity failure;
    capture;
    for n in 1 to 23 loop read_word(256+n); held((n-1)*32+31 downto (n-1)*32) := rd; end loop;
    for b in 0 to 7 loop
      read_word(260+b);
      for n in 0 to 23 loop assert rd(n) = inputs(b)(n) report "raw input bank changed polarity" severity failure; end loop;
    end loop;
    for b in 0 to 5 loop
      read_word(268+2*b);
      for n in 0 to 31 loop assert rd(n) = lamps(b)(n) report "powered lamp mapping" severity failure; end loop;
      read_word(269+2*b);
      for n in 0 to 7 loop assert rd(n) = lamps(b)(n+32) severity failure; end loop;
    end loop;
    -- New scans and writes cannot mutate the captured view or CPU registers.
    inputs(1) <= (others => '1'); lamps <= (others => (others => '1')); wait for 15 us;
    for n in 1 to 23 loop
      waddress <= std_logic_vector(to_unsigned(256+n,9)); wd <= (others => '1'); we <= '1';
      read_word(256+n);
      assert rd = held((n-1)*32+31 downto (n-1)*32) report "held view changed" severity failure;
    end loop;
    we <= '0'; read_word(1); assert rd = x"00000000" report "diagnostic write modified CPU config" severity failure;
    capture; afterword := rd; assert unsigned(afterword) > unsigned(before) severity failure;
    read_word(268); assert rd = x"FFFFFFFF" report "new frame was not captured" severity failure;
    -- Off must win simultaneous active-high buttons; release must retain Off.
    inputs(0)(11) <= '1'; inputs(0)(12) <= '1'; wait for 15 us;
    assert off = '1' report "Power Off priority" severity failure;
    capture; read_word(258); assert rd(4) = '1' and rd(5) = '1' and rd(1) = '0' severity failure;
    inputs(0) <= (others => '0'); wait for 15 us;
    assert off = '1' report "released buttons enabled power" severity failure;
    -- Shared reset clears the observer, including an in-flight mailbox transfer.
    reset <= '1'; wait for 37 ns; reset <= '0'; capture; read_word(257);
    assert rd = x"00000000" report "reset retained stale panel frame" severity failure;
    report "PANEL_SNAPSHOT_TEST_PASS" severity note; done <= true; wait;
  end process;
end;
