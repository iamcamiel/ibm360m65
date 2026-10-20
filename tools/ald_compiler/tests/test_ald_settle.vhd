library ieee;
use ieee.std_logic_1164.all;
use ieee.numeric_std.all;
use std.textio.all;

entity test_ald_settle is end;
architecture test of test_ald_settle is
  signal clk : std_logic := '0';
  signal rst, hlt, gate_i, a, n, b, clock_result : std_logic := '0';
  signal input_i, up : std_logic_vector(0 to 3);
  signal down : std_logic_vector(3 downto 0);
  signal done : boolean := false;
begin
  clk <= not clk after 5 ns when not done else '0';
  dut : entity work.ald port map(clk => clk, rst => rst, hlt => hlt,
    P_gate => gate_i, P_input => input_i, P_result_a => a, M_result_n => n,
    P_result_b => b, P_clock_result => clock_result, P_vector_up => up,
    P_vector_down => down);
  process
    file vectors : text open read_mode is "settle-vectors.txt";
    variable row : line;
    variable reset_v, halt_v, input_v, gate_v, a_v, n_v, b_v, c_v, up_v, down_v : integer;
    variable count : natural := 0;
    procedure check_outputs is
    begin
      assert a = std_logic'val(a_v+2) and n = std_logic'val(n_v+2)
        and b = std_logic'val(b_v+2) and clock_result = std_logic'val(c_v+2)
        and up = std_logic_vector(to_unsigned(up_v,4))
        and down = std_logic_vector(to_unsigned(down_v,4))
        report "Two-pass C++ reference mismatch at cycle " & integer'image(count) severity failure;
    end;
  begin
    while not endfile(vectors) loop
      readline(vectors,row);
      read(row,reset_v); read(row,halt_v); read(row,input_v); read(row,gate_v);
      read(row,a_v); read(row,n_v); read(row,b_v); read(row,c_v); read(row,up_v); read(row,down_v);
      wait until falling_edge(clk);
      rst <= std_logic'val(reset_v+2); hlt <= std_logic'val(halt_v+2);
      input_i <= std_logic_vector(to_unsigned(input_v,4)); gate_i <= std_logic'val(gate_v+2);
      wait until rising_edge(clk); wait for 1 ns;
      check_outputs;
      -- Changing live external inputs must not change the settled snapshot,
      -- including during halt and immediately after synchronous reset.
      input_i <= not input_i; gate_i <= not gate_i;
      wait for 2 ns; check_outputs;
      count := count+1;
    end loop;
    report "ALD_SETTLE_TEST_PASS 1024 C++ two-pass cycles; cross-section feedback, CLOCK snapshot, vectors, alias, halt, reset" severity note;
    done <= true;
    wait;
  end process;
end;
