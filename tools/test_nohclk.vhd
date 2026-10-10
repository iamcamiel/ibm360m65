library ieee;
use ieee.std_logic_1164.all;

-- Run with the production timing primitives at one update per 10 ns edge.
entity test_nohclk is end;
architecture test of test_nohclk is
  signal clk : std_logic := '0';
  signal rst : std_logic := '1';
  signal hlt, stimulus, osc, td20, td40 : std_logic := '0';
  signal write_gate : std_logic := '0';
  signal address : std_logic_vector(0 to 4) := "00011";
  signal write_data, read_data : std_logic_vector(0 to 31);
  signal write_parity, read_parity : std_logic_vector(0 to 3);
begin
  clk <= not clk after 5 ns;
  oscillator : entity work.OSC5MC port map(clk, rst, hlt, '0', osc);
  delay20 : entity work.TD20NS port map(clk, rst, hlt, stimulus, td20);
  delay40 : entity work.TD40NS port map(clk, rst, hlt, stimulus, td40);
  memory : entity work.LS_MEMORY port map(clk, rst, hlt, address, address,
    write_gate, write_data, write_parity, read_data, read_parity);
  process
    variable previous_rise : time := 0 ns;
    variable rises : integer := 0;
    variable held20, held40, heldosc : std_logic;
    procedure tick is
    begin
      wait until rising_edge(clk);
      wait for 1 ns;
    end procedure;
  begin
    write_data <= x"12345678";
    write_parity <= "1010";
    tick;
    assert read_data = x"00000000" report "LS reset failed" severity failure;
    rst <= '0';
    stimulus <= '1';
    write_gate <= '1';
    tick;
    assert td20 = '0' and td40 = '0' report "Delay skipped a step" severity failure;
    write_gate <= '0';
    tick;
    assert td20 = '1' and td40 = '0' report "20 ns delay failed" severity failure;
    assert read_data = x"12345678" and read_parity = "1010"
      report "LS did not update on consecutive core edges" severity failure;
    tick;
    assert td40 = '0' report "40 ns delay shortened" severity failure;
    tick;
    assert td40 = '1' report "40 ns delay failed" severity failure;
    hlt <= '1';
    held20 := td20; held40 := td40; heldosc := osc;
    stimulus <= '0'; write_gate <= '1'; write_data <= x"DEADBEEF";
    for i in 1 to 5 loop
      tick;
      assert td20 = held20 and td40 = held40 and osc = heldosc
        report "Timing state advanced while halted" severity failure;
      assert read_data = x"12345678" report "LS advanced while halted" severity failure;
    end loop;
    rst <= '1'; tick;
    assert td20 = '0' and td40 = '0' and osc = '0' and read_data = x"00000000"
      report "Reset must override halt" severity failure;
    hlt <= '0'; rst <= '0'; write_gate <= '0';
    for i in 1 to 5 loop
      wait until rising_edge(osc);
      if rises > 0 then
        assert now - previous_rise = 200 ns report "Oscillator is not 200 ns" severity failure;
      end if;
      previous_rise := now;
      rises := rises + 1;
    end loop;
    report "NOHCLK_TEST_PASS 10ns steps 200ns oscillator delays memory halt reset" severity note;
    wait;
  end process;
end;
