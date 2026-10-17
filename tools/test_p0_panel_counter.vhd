library ieee;
use ieee.std_logic_1164.all;
use ieee.numeric_std.all;

entity test_p0_panel_counter is end;
architecture test of test_p0_panel_counter is
  signal clk, reset, halt, p0 : std_logic := '0';
  signal selected : std_logic;
  signal contacts : std_logic_vector(17 to 21) := "11111";
  signal normal : std_logic_vector(0 to 39) := x"123456789A";
  signal lamps, small_lamps : std_logic_vector(0 to 39);
begin
  selected <= '1' when contacts = "11111" else '0';
  full_width : entity work.P0_PANEL_COUNTER
    port map (clk, reset, halt, p0, selected, normal, lamps);
  wrap_width : entity work.P0_PANEL_COUNTER generic map (COUNT_BITS => 4)
    port map (clk, reset, halt, p0, selected, normal, small_lamps);

  process
    procedure tick is
    begin
      clk <= '0'; wait for 5 ns; clk <= '1'; wait for 5 ns;
    end;
    procedure pulse is
    begin
      p0 <= '0'; tick; p0 <= '1'; tick;
    end;
    procedure count_is(n : natural) is
    begin
      assert lamps(0 to 35) = std_logic_vector(to_unsigned(n, 36))
        report "36-bit count/order differs" severity failure;
      assert small_lamps(0 to 35) = std_logic_vector(to_unsigned(n mod 16, 36))
        report "Modulo-width wrap differs" severity failure;
      assert lamps(36 to 39) = normal(36 to 39)
        report "Status lamps changed" severity failure;
    end;
  begin
    reset <= '1'; p0 <= '1'; tick;
    reset <= '0'; tick; count_is(0); -- Held-high reset release is not a rise.
    for i in 1 to 12 loop tick; end loop; count_is(0);
    pulse; count_is(1);
    assert lamps(35) = '1' and lamps(0) = '0'
      report "Least significant bit must be rightmost" severity failure;
    for i in 1 to 10 loop tick; end loop; count_is(1);
    p0 <= '0'; tick; count_is(1); -- Falling edges do not count.
    halt <= '1';
    for i in 1 to 8 loop pulse; end loop; count_is(1);
    halt <= '0'; tick; count_is(1); -- No count on releasing held-high P0.
    pulse; count_is(2);

    -- Each of the five physical positions passes ALD lamps through unchanged;
    -- count accumulation is independent of the selected display.
    for position in 17 to 21 loop
      contacts <= (others => '1'); contacts(position) <= '0'; tick;
      assert lamps = normal and small_lamps = normal
        report "Non-counter position was overridden" severity failure;
      pulse;
    end loop;
    contacts <= "11111"; tick; count_is(7);
    normal(36 to 39) <= "0101"; tick; count_is(7);
    for i in 1 to 9 loop pulse; end loop; count_is(16);
    assert lamps(31) = '1' and lamps(32 to 35) = "0000"
      report "Counter carry or display bit order differs" severity failure;

    -- Deep carry with the actual36-bit datapath and repeated short-width wrap.
    reset <= '1'; tick; reset <= '0'; tick; count_is(0);
    for i in 1 to 65536 loop pulse; end loop; count_is(65536);
    halt <= '1'; reset <= '1'; tick; count_is(0); -- Reset wins halt.
    halt <= '0'; reset <= '0'; tick; count_is(0);
    pulse; count_is(1);
    report "P0_PANEL_COUNTER_TEST_PASS" severity note;
    wait;
  end process;
end test;
