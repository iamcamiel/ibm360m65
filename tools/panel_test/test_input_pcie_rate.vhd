library ieee;
use ieee.std_logic_1164.all;
use ieee.numeric_std.all;
entity test_input_pcie_rate is end;
architecture test of test_input_pcie_rate is
  signal clk,reset,sck,latch_n,off,phase,valid : std_logic:='0';
  signal so : std_logic_vector(0 to 5);
  signal payload : std_logic_vector(735 downto 0);
  signal done : boolean:=false;
begin
  process begin while not done loop clk<=not clk;wait for 5 ns;end loop;wait;end process;
  panel : entity work.INPUT_PCIE_CORE port map(clk=>clk,reset=>reset,
    shift_i=>"10101010",sck=>sck,latch_n=>latch_n,shift_o=>so,
    power_off=>off,phase_o=>phase,snapshot_o=>payload,snapshot_valid_o=>valid);
  process
    variable start,t : time;
  begin
    reset<='1';wait for 37 ns;reset<='0';
    wait until rising_edge(latch_n);
    start:=now;
    for bitno in 0 to 39 loop
      wait until rising_edge(sck);
      if bitno>0 then assert now-t=10240 ns report "production serial low pulse" severity failure;end if;
      t:=now;wait until falling_edge(sck);
      assert now-t=10240 ns report "production serial high pulse" severity failure;t:=now;
    end loop;
    wait until falling_edge(latch_n);t:=now;
    wait until rising_edge(latch_n);
    assert now-t=10240 ns report "production latch pulse" severity failure;
    assert now-start=860160 ns report "production scan frame" severity failure;
    wait until rising_edge(valid);t:=now;
    wait until rising_edge(valid);
    assert now-t=860160 ns report "production complete frame period" severity failure;
    assert payload(95 downto 64)=x"00000400" report "production divider" severity failure;
    for bank in 0 to 7 loop
      for n in 0 to 23 loop
        if bank mod 2=0 then assert payload((3+bank)*32+n)='1' severity failure;
        else assert payload((3+bank)*32+n)='0' severity failure;end if;
      end loop;
    end loop;
    report "INPUT_PCIE_RATE_PASS exact 48.828125 kHz, 860.16 us frame, divider and all raw banks" severity note;
    done<=true;wait;
  end process;
end;
