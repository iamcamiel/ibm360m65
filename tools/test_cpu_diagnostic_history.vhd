library ieee;
use ieee.std_logic_1164.all;
use ieee.numeric_std.all;
entity test_cpu_diagnostic_history is end;
architecture test of test_cpu_diagnostic_history is
 signal clk, reset, valid, reply, halted, old_reply, old_halted : std_logic := '0';
 signal finished : boolean := false;
 signal state : std_logic_vector(1023 downto 0) := (others=>'0');
 signal cmd, result, old_result : std_logic_vector(31 downto 0) := (others=>'0');
begin
 process begin while not finished loop wait for 5 ns;clk<=not clk;end loop;wait;end process;
 dut: entity work.CPU_DIAGNOSTICS port map(clk,reset,state,cmd,valid,result,reply,halted);
 reference: entity work.CPU_DIAGNOSTICS_REFERENCE port map(clk,reset,state,cmd,valid,old_result,old_reply,old_halted);
 process
  variable checked_edges : natural := 0;
 begin
  wait until rising_edge(clk);wait for 1 ns;
  checked_edges:=checked_edges+1;
  assert reply=old_reply and halted=old_halted report "history pipeline changed reply/halt edge" severity failure;
  if reply='1' then assert result=old_result report "history pipeline changed command data" severity failure;end if;
  if finished then wait;end if;
 end process;
 process
  variable word, last_flags : std_logic_vector(31 downto 0);
  procedure edge(op : integer := -1; arg : natural := 0) is
  begin
   wait until falling_edge(clk);
   if op<0 then valid<='0';else cmd<=std_logic_vector(to_unsigned(op,8)) & std_logic_vector(to_unsigned(arg,24));valid<='1';end if;
   wait until rising_edge(clk);wait for 2 ns;
  end;
  procedure read_history(index : natural; variable value : out std_logic_vector(31 downto 0)) is
  begin
   edge(6,index);assert reply='0' report "history read latency changed" severity failure;
   edge;assert reply='1' report "history response missing" severity failure;value:=result;
  end;
  procedure fresh is
  begin reset<='1';edge;edge;reset<='0';state<=(others=>'0');state(17)<='1';edge;edge;end;
 begin
  fresh;edge(1,0);
  -- A freeze on the entry edge followed immediately by a read of that entry.
  state(11 downto 0)<=x"123";edge(2);
  read_history(0,word);assert word=x"00020123" report "pending final entry not forwarded" severity failure;
  read_history(1,word);read_history(2,word);
  -- Consecutive writes, three wraps, coherent flags/ticks and final-entry read.
  fresh;edge(1,0);
  for i in 1 to 799 loop state(11 downto 0)<=std_logic_vector(to_unsigned(i,12));state(20)<=not state(20);edge;end loop;
  state(11 downto 0)<=std_logic_vector(to_unsigned(800,12));edge(2);
  edge(7,3);assert result=x"00000100" report "ring count" severity failure;
  edge(7,4);assert result=x"00000020" report "ring head" severity failure;
  for i in 0 to 255 loop
   read_history(i*4,word);assert unsigned(word(11 downto 0))=to_unsigned(i+545,12) report "ring order" severity failure;
   read_history(i*4+1,word);read_history(i*4+2,word);read_history(i*4+3,word);assert word=x"00000000" severity failure;
  end loop;
  -- Rearm on a recording edge discards that old-arm entry and pending address.
  fresh;edge(1,0);state(11 downto 0)<=x"100";edge;
  state(11 downto 0)<=x"101";edge(1,0);state(11 downto 0)<=x"102";edge(2);
  read_history(0,word);assert word(11 downto 0)=x"102" report "rearm retained stale entry" severity failure;
  edge(7,3);assert result=x"00000001" severity failure;
  -- Reset with a pending write makes it logically inaccessible.
  fresh;edge(1,0);state(11 downto 0)<=x"300";edge;reset<='1';edge;reset<='0';edge;
  edge(1,0);state(11 downto 0)<=x"301";edge(2);read_history(0,word);assert word(11 downto 0)=x"301" severity failure;
  -- A check event must beat a same-edge command; immediate history read sees it.
  fresh;edge(1,17);state(12)<='1';edge(4);assert result=x"E0000003" and halted='1' severity failure;
  read_history(0,word);assert word(24)='1' and word(12)='1' report "check trigger flag lost" severity failure;
  edge(7,6);assert result=x"00000001" severity failure;
  -- Step and breakpoint halt/capture edges remain identical to the reference.
  edge(4);edge(9,16#3456#);edge(10,1);state(11 downto 0)<=x"456";edge(1,0);
  state(20)<='1';edge;edge(7,6);assert result=x"00000002" severity failure;
  edge(3);edge(9,0);edge(1,0);edge(12,0);state(11 downto 0)<=x"457";edge;
  edge(7,6);assert result=x"00000008" and halted='1' severity failure;
  report "CPU_DIAGNOSTIC_HISTORY_PASS: exact reference responses; immediate forwarding, consecutive writes, three wraps, rearm/reset, trigger/step priority" severity note;
  finished<=true;wait;
 end process;
end;
