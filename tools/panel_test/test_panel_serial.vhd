library ieee;
use ieee.std_logic_1164.all;

entity TEST_PANEL_SERIAL is end;
architecture TEST of TEST_PANEL_SERIAL is
  signal clk : std_logic:='0';
  signal reset : std_logic:='1';
  signal sck,latch_n,phase : std_logic;
  signal data : std_logic_vector(0 to 5);
  signal done : boolean:=false;
  signal frames, transitions : natural:=0;
begin
  process begin while not done loop clk<=not clk;wait for 5 ns;end loop;wait;end process;
  dut : entity work.PANEL_TEST_SERIAL generic map(HALF_CYCLES=>3,PATTERN_CYCLES=>1000)
    port map(clk=>clk,reset=>reset,sck=>sck,latch_n=>latch_n,data=>data,pattern_o=>phase);
  process(sck,latch_n,reset)
    type banks is array(0 to 5) of std_logic_vector(39 downto 0);
    variable shift : banks:=(others=>(others=>'0'));
    variable pulses : natural:=0;
    variable previous : std_logic:='0';
    variable current,expected : std_logic;
    variable low_since,last_sck : time:=0 ns;
  begin
    if reset='1' then pulses:=0;last_sck:=0 ns;
    elsif rising_edge(sck) then
      assert latch_n='1' report "SCK during latch pulse" severity failure;
      if pulses>0 then assert now-last_sck=60 ns report "serial period" severity failure;end if;
      last_sck:=now;
      for b in 0 to 5 loop shift(b):=shift(b)(38 downto 0)&data(b);end loop;
      pulses:=pulses+1;
    elsif falling_edge(latch_n) then
      assert pulses=40 report "frame must contain 40 serial clocks" severity failure;
      low_since:=now;
    elsif rising_edge(latch_n) then
      assert now-low_since=30 ns report "latch low pulse width" severity failure;
      current:=shift(0)(0);
      for b in 0 to 5 loop
        for n in 0 to 39 loop
          expected:=current;
          if (b+n) mod 2=1 then expected:=not expected;end if;
          assert shift(b)(n)=expected report "checkerboard or torn frame" severity failure;
        end loop;
      end loop;
      if frames>0 and current/=previous then transitions<=transitions+1;end if;
      previous:=current;frames<=frames+1;pulses:=0;last_sck:=0 ns;
    end if;
  end process;
  process
  begin
    wait for 37 ns;reset<='0';wait until frames=20;
    wait until rising_edge(sck);wait for 7 ns;reset<='1';wait for 20 ns;
    assert sck='0' and latch_n='1' and data="000000" report "reset output safety" severity failure;
    reset<='0';wait until frames=40;
    assert transitions>=8 report "checkerboard did not alternate" severity failure;
    report "PANEL_TEST_PASS 40 coherent frames, 40 clocks/frame, timing, alternation and mid-frame reset" severity note;
    done<=true;wait;
  end process;
end TEST;
