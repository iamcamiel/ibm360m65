library ieee;
use ieee.std_logic_1164.all;
use ieee.numeric_std.all;

entity test_display_clock is end;
architecture test of test_display_clock is
  signal clk : std_logic := '0';
  signal reset : std_logic := '1';
  signal enable : std_logic;
  signal count : integer range 0 to 511 := 255;
  signal done : boolean := false;
  signal shift_in : std_logic_vector(0 to 7) := (others => '1');
  signal shift_out : std_logic_vector(0 to 5);
  signal display_clk, latch_n, power_off : std_logic;
  type switch_banks is array (0 to 7) of std_logic_vector(0 to 23);
  signal sw : switch_banks;
  signal lamps : std_logic_vector(0 to 39) := (others => '1');
  signal configured : std_logic := '1';
begin
  enable <= '1' when count = 511 and reset = '0' else '0';
  dut : entity work.BLINKEN port map (
    clk => clk, rst_i => reset, enable_i => enable,
    disp_clk_o => display_clk, disp_latch_n_o => latch_n,
    disp_shift_o => shift_out, disp_shift_i => shift_in,
    sw0 => sw(0), sw1 => sw(1), sw2 => sw(2), sw3 => sw(3),
    sw4 => sw(4), sw5 => sw(5), sw6 => sw(6), sw7 => sw(7),
    li0 => lamps, li1 => lamps, li2 => lamps, li3 => lamps, li4 => lamps, li5 => lamps,
    panel_snapshot_o => open, panel_valid_o => open,
    configured => configured, power_off => power_off);
  process
  begin while not done loop clk <= not clk; wait for 5 ns; end loop; wait; end process;
  process(clk)
  begin
    if rising_edge(clk) then
      if reset = '1' then count <= 255;
      elsif count = 511 then count <= 0;
      else count <= count + 1;
      end if;
    end if;
  end process;
  process
    variable last_update : time := 0 ns;
    procedure tick is
    begin
      loop wait until rising_edge(clk); exit when enable = '1'; end loop;
      if last_update /= 0 ns then
        assert now-last_update = 5120 ns report "display update period changed" severity failure;
      end if;
      last_update := now; wait for 1 ns;
    end;
    procedure frame(power_button : natural; press : boolean) is
    begin
      -- 40 serial bits plus latch slots 40/41; two updates per serial bit.
      for bitno in 0 to 41 loop
        shift_in <= (others => '1');
        if press and bitno = power_button then shift_in(0) <= '0'; end if;
        -- Alternate patterns on other switch banks, stable well before sample.
        for bank in 1 to 7 loop
          if (bitno+bank) mod 2 = 0 then shift_in(bank) <= '0'; end if;
        end loop;
        tick; -- falling display phase samples switches and drives lamp bits
        assert display_clk = '0' report "serial clock falling phase" severity failure;
        if bitno = 41 then
          assert latch_n = '0' report "latch low phase" severity failure;
        elsif bitno < 24 then
          for bank in 1 to 7 loop
            assert sw(bank)(23-bitno) = shift_in(bank) report "switch scan bit order" severity failure;
          end loop;
        end if;
        tick; -- rising display phase
        if bitno < 40 then assert display_clk = '1' severity failure; end if;
        if bitno = 41 then assert latch_n = '1' report "latch high phase" severity failure; end if;
      end loop;
    end;
  begin
    wait for 37 ns;
    assert power_off = '1' and display_clk = '0' and latch_n = '1'
      report "display reset outputs" severity failure;
    reset <= '0';
    frame(12, true); assert power_off = '0' report "power-on switch" severity failure;
    -- All-ones lamps must shift to each of the six panel chains while powered on.
    for bitno in 0 to 39 loop
      shift_in <= (others => '1'); tick;
      assert shift_out = "111111" report "lamp scan data" severity failure;
      tick;
    end loop;
    for bitno in 40 to 41 loop tick; tick; end loop;
    frame(11, true); assert power_off = '1' report "power-off switch" severity failure;
    configured <= '0'; frame(12, true);
    assert power_off = '1' report "unconfigured panel enabled CPU" severity failure;
    reset <= '1'; wait for 31 ns;
    assert display_clk = '0' and latch_n = '1' and power_off = '1'
      report "display reset recovery" severity failure;
    report "DISPLAY_CLOCK_TEST_PASS" severity note; done <= true; wait;
  end process;
end test;
