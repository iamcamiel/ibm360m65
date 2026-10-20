library ieee;
use ieee.std_logic_1164.all;
use ieee.numeric_std.all;
entity test_cpu_diagnostic_priority is end;
architecture test of test_cpu_diagnostic_priority is
 signal clk, reset, valid, reply, halted : std_logic := '0';
 signal finished : boolean := false;
 signal state : std_logic_vector(1023 downto 0) := (others=>'0');
 signal cmd, result : std_logic_vector(31 downto 0) := (others=>'0');
begin
 process begin while not finished loop clk<=not clk;wait for 5 ns;end loop;wait;end process;
 dut: entity work.CPU_DIAGNOSTICS port map(clk,reset,state,cmd,valid,result,reply,halted);
 process
  procedure command(op,arg : natural) is
  begin
   wait until falling_edge(clk);cmd<=std_logic_vector(to_unsigned(op,8)) & std_logic_vector(to_unsigned(arg,24));valid<='1';
   wait until rising_edge(clk);wait for 1 ns;assert reply='1' severity failure;
   wait until falling_edge(clk);valid<='0';
  end;
 begin
  reset<='1';wait for 25 ns;reset<='0';state(17)<='1';state(11 downto 0)<=x"A48";wait for 40 ns;
  command(9,16#3A48#);command(10,1);command(1,17); -- breakpoint and check, auto halt
  -- Check and breakpoint coincide; RELEASE must not win.
  state(20)<='1';state(12)<='1';cmd<=x"04000000";valid<='1';
  wait until rising_edge(clk);wait for 1 ns;
  assert result=x"E0000003" and halted='1' report "event lost to release" severity failure;
  wait until falling_edge(clk);valid<='0';command(7,6);
  assert result=x"00000003" report "simultaneous check/breakpoint attribution" severity failure;
  command(7,0);assert result=x"00000001" severity failure;
  command(9,0);command(1,0);command(12,0);
  state(11 downto 0)<=x"A49";cmd<=x"01000000";valid<='1';
  wait until rising_edge(clk);wait for 1 ns;
  assert result=x"E0000003" and halted='1' report "step lost to ARM" severity failure;
  wait until falling_edge(clk);valid<='0';command(7,6);
  assert result=x"00000008" severity failure;
  command(7,0);assert result=x"00000002" report "step generation" severity failure;
  report "CPU_DIAGNOSTIC_PRIORITY_TEST_PASS" severity note;finished<=true;wait;
 end process;
end;
