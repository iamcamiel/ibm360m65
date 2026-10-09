library ieee;
use ieee.std_logic_1164.all;
entity TEST_PANEL_RATE is end;
architecture TEST of TEST_PANEL_RATE is
  signal clk : std_logic:='0';
  signal reset : std_logic:='1';
  signal sck,latch_n,phase : std_logic;
  signal data : std_logic_vector(0 to 5);
  signal done : boolean:=false;
begin
  process begin while not done loop clk<=not clk;wait for 5 ns;end loop;wait;end process;
  dut : entity work.PANEL_TEST_SERIAL generic map(HALF_CYCLES=>500)
    port map(clk=>clk,reset=>reset,sck=>sck,latch_n=>latch_n,data=>data,pattern_o=>phase);
  process
    variable previous_sck,previous_latch : time:=0 ns;
  begin
    wait for 37 ns;reset<='0';
    for frame in 1 to 2 loop
      for pulse in 1 to 40 loop
        wait until rising_edge(sck);
        if pulse>1 then assert now-previous_sck=10 us report "100 kHz serial period" severity failure;end if;
        previous_sck:=now;
        wait until falling_edge(sck);
        assert now-previous_sck=5 us report "5 us serial high time" severity failure;
      end loop;
      wait until falling_edge(latch_n);previous_sck:=now;
      wait until rising_edge(latch_n);
      assert now-previous_sck=5 us report "5 us latch low time" severity failure;
      if frame>1 then assert now-previous_latch=420 us report "0.42 ms refresh period" severity failure;end if;
      previous_latch:=now;
    end loop;
    report "PANEL_RATE_PASS 100 kHz serial, 5 us high/latch, 0.42 ms frames" severity note;
    done<=true;wait;
  end process;
end TEST;
