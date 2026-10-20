library ieee;
use ieee.std_logic_1164.all;
entity TEST_PRODUCTION_PANEL is end;
architecture TEST of TEST_PRODUCTION_PANEL is
  signal clk : std_logic:='0';
  signal reset : std_logic:='1';
  signal si : std_logic_vector(0 to 7):=(others=>'1');
  signal so : std_logic_vector(0 to 5);
  signal sw : std_logic_vector(191 downto 0);
  signal sck,latch_n,power_off,phase,parity : std_logic;
  signal done,observe : boolean:=false;
  signal selected_bit : integer range -1 to 24:=-1;
  signal frames,checked : natural:=0;
  type switch_banks is array(0 to 7) of std_logic_vector(0 to 23);
  subtype stimulus_bit is integer range -1 to 24;
  function inputs(selected : integer) return switch_banks is
    variable v : switch_banks:=(others=>(others=>'1'));
  begin
    for b in 1 to 7 loop for n in 0 to 23 loop
      if (b+n) mod 3=0 then v(b)(n):='0';end if;
    end loop;end loop;
    if selected=24 then v(0):=(others=>'0');
    elsif selected>=0 then v(0)(selected):='0';end if;
    return v;
  end;
begin
  process begin while not done loop clk<=not clk;wait for 5 ns;end loop;wait;end process;
  dut : entity work.PRODUCTION_PANEL_CORE
    generic map(PATTERN_CYCLES=>200000,SCAN_ENABLE_CYCLES=>1024)
    port map(clk=>clk,reset=>reset,shift_i=>si,sck=>sck,latch_n=>latch_n,
      shift_o=>so,power_off=>power_off,phase_o=>phase,switch_parity=>parity,switches_o=>sw);
  process
    variable held : switch_banks;
    variable bitno : natural:=0;
  begin
    held:=inputs(selected_bit);
    for b in 0 to 7 loop si(b)<=held(b)(23);end loop;
    loop
      wait on sck,latch_n,reset;
      if reset='1' or falling_edge(latch_n) then
        held:=inputs(selected_bit);bitno:=0;
        for b in 0 to 7 loop si(b)<=held(b)(23);end loop;
      elsif rising_edge(sck) then
        bitno:=bitno+1;
        for b in 0 to 7 loop
          if bitno<24 then si(b)<=held(b)(23-bitno);else si(b)<='1';end if;
        end loop;
      end if;
    end loop;
  end process;
  process(sck,latch_n,reset)
    type led_banks is array(0 to 5) of std_logic_vector(39 downto 0);
    variable shifted : led_banks;
    variable pulses : natural:=0;
    variable last_sck,low_since,last_latch : time:=0 ns;
    variable held_phase,expected : std_logic;
    variable reference : switch_banks;
    variable index : natural;
  begin
    if reset='1' then pulses:=0;last_sck:=0 ns;last_latch:=0 ns;
    elsif rising_edge(sck) then
      if pulses=0 then held_phase:=phase;
      else assert now-last_sck=20480 ns report "polarity SCK period" severity failure;end if;
      last_sck:=now;pulses:=pulses+1;
      for b in 0 to 5 loop shifted(b):=shifted(b)(38 downto 0)&so(b);end loop;
    elsif falling_edge(latch_n) then
      assert pulses=40 report "polarity frame length" severity failure;low_since:=now;
    elsif rising_edge(latch_n) then
      assert now-low_since=10240 ns report "polarity latch pulse" severity failure;
      if last_latch/=0 ns then assert now-last_latch=860160 ns report "polarity refresh period" severity failure;end if;
      last_latch:=now;
      if observe then
        reference:=inputs(selected_bit);
        for b in 0 to 5 loop for n in 0 to 39 loop
          expected:='0';
          if n<36 then
            index:=n;if n>=24 then index:=n-16;end if;
            expected:=reference(0)(index);if b>=3 then expected:=not expected;end if;
          elsif b=0 then expected:=held_phase;
          elsif b=3 and n<38 then expected:=not reference(0)(11);
          elsif b=3 then expected:=not reference(0)(12);end if;
          assert shifted(b)(n)=expected report "raw/inverse/button lamp mapping" severity failure;
        end loop;end loop;
        checked<=checked+1;
      end if;
      frames<=frames+1;pulses:=0;last_sck:=0 ns;
    end if;
  end process;
  process
    variable target : natural;
    variable reference : switch_banks;
  begin
    wait for 37 ns;reset<='0';
    -- Released, every individual bit low, and entire serial input stuck low.
    for bitno in stimulus_bit loop
      observe<=false;selected_bit<=bitno;target:=frames+3;
      wait until frames>=target;wait for 1 ns;
      reference:=inputs(bitno);
      for b in 0 to 7 loop for n in 0 to 23 loop
        assert sw(191-b*24-n)=reference(b)(n) report "all switch bank bits" severity failure;
      end loop;end loop;
      observe<=true;wait until frames>=target+1;wait for 1 ns;
    end loop;
    observe<=false;selected_bit<=-1;wait until rising_edge(sck);wait for 7 ns;
    reset<='1';wait for 31 ns;
    assert sck='0' and latch_n='1' and power_off='1' report "polarity reset" severity failure;
    reset<='0';target:=frames+3;wait until frames>=target;observe<=true;
    wait until frames>=target+1;wait for 1 ns;
    assert checked>=27 report "polarity case coverage" severity failure;
    report "POLARITY_PANEL_PASS all 24 walking button bits, raw/inverse banks, Power On/Off lamps, all 192 inputs, stuck low, exact timing and reset" severity note;
    done<=true;wait;
  end process;
end;
