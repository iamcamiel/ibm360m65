library ieee;
use ieee.std_logic_1164.all;
entity TEST_PRODUCTION_PANEL is end;
architecture TEST of TEST_PRODUCTION_PANEL is
  signal clk : std_logic:='0';
  signal reset : std_logic:='1';
  signal sck,latch_n,power_off,phase,parity : std_logic;
  signal si : std_logic_vector(0 to 7):=(others=>'1');
  signal so : std_logic_vector(0 to 5);
  signal sw : std_logic_vector(191 downto 0);
  signal done : boolean:=false;
  signal frames,checked,changes : natural:=0;
  signal mode : natural range 0 to 2:=0; -- ON, released, OFF
  type switch_banks is array(0 to 7) of std_logic_vector(0 to 23);
  function inputs(m : natural) return switch_banks is
    variable banks : switch_banks:=(others=>(others=>'1'));
  begin
    for b in 1 to 7 loop
      for n in 0 to 23 loop
        if (b+n) mod 3=0 then banks(b)(n):='0';end if;
      end loop;
    end loop;
    if m=0 then banks(0)(11):='0';elsif m=2 then banks(0)(12):='0';end if;
    return banks;
  end;
begin
  process begin while not done loop clk<=not clk;wait for 5 ns;end loop;wait;end process;
  dut : entity work.PRODUCTION_PANEL_CORE generic map(PATTERN_CYCLES=>100000)
    port map(clk=>clk,reset=>reset,shift_i=>si,sck=>sck,latch_n=>latch_n,
      shift_o=>so,power_off=>power_off,phase_o=>phase,switch_parity=>parity,switches_o=>sw);
  -- Model eight 24-bit 597 chains: parallel load at latch low, shift at SCK rise.
  process
    variable held : switch_banks;
    variable bitno : natural:=0;
  begin
    held:=inputs(mode);
    for b in 0 to 7 loop si(b)<=held(b)(23);end loop;
    loop
      wait on sck,latch_n,reset;
      if reset='1' or falling_edge(latch_n) then
        held:=inputs(mode);bitno:=0;
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
    variable shifted : led_banks:=(others=>(others=>'0'));
    variable pulses : natural:=0;
    variable last_sck,low_since,last_latch : time:=0 ns;
    variable first_power,previous_phase,current,expected : std_logic:='0';
    variable power_changed : boolean:=false;
  begin
    if reset='1' then pulses:=0;last_sck:=0 ns;last_latch:=0 ns;
    elsif rising_edge(sck) then
      if pulses=0 then first_power:=power_off;power_changed:=false;
      else assert now-last_sck=10240 ns report "production SCK period" severity failure;end if;
      if power_off/=first_power then power_changed:=true;end if;
      last_sck:=now;pulses:=pulses+1;
      for b in 0 to 5 loop shifted(b):=shifted(b)(38 downto 0)&so(b);end loop;
    elsif falling_edge(latch_n) then
      assert pulses=40 report "production frame length" severity failure;
      low_since:=now;
    elsif rising_edge(latch_n) then
      assert now-low_since=5120 ns report "production latch pulse" severity failure;
      if last_latch/=0 ns then assert now-last_latch=430080 ns report "production refresh period" severity failure;end if;
      last_latch:=now;
      if frames>=2 and first_power='0' and not power_changed then
        current:=shifted(0)(0);
        for b in 0 to 5 loop
          for n in 0 to 39 loop
            expected:=current;if (b+n) mod 2=1 then expected:=not expected;end if;
            assert shifted(b)(n)=expected report "production checkerboard coherence" severity failure;
          end loop;
        end loop;
        if checked>0 and current/=previous_phase then changes<=changes+1;end if;
        previous_phase:=current;checked<=checked+1;
      end if;
      frames<=frames+1;pulses:=0;last_sck:=0 ns;
    end if;
  end process;
  process
    variable expected : switch_banks;
  begin
    wait for 37 ns;reset<='0';wait until frames=3;wait for 1 ns;
    assert power_off='0' report "physical Power On path" severity failure;
    expected:=inputs(0);
    for b in 0 to 7 loop
      for n in 0 to 23 loop
        assert sw(191-b*24-n)=expected(b)(n) report "all eight switch banks scanned" severity failure;
      end loop;
    end loop;
    mode<=2;wait until frames=6;wait for 1 ns;
    assert power_off='1' report "physical Power Off path" severity failure;
    mode<=0;wait until frames=9;wait for 1 ns;
    assert power_off='0' report "Power On recovery" severity failure;
    mode<=1;wait until frames=13;wait for 1 ns;
    assert power_off='0' report "released button retains power" severity failure;
    mode<=0;wait until rising_edge(sck);wait for 7 ns;reset<='1';wait for 31 ns;
    assert sck='0' and latch_n='1' and power_off='1' report "production mid-frame reset" severity failure;
    reset<='0';wait until frames=17;wait for 1 ns;
    assert power_off='0' and checked>=7 and changes>=2 report "production checkerboard recovery/alternation" severity failure;
    report "PRODUCTION_PANEL_PASS exact scan timing, all 192 switch bits, power controls, coherent checkerboards, alternation and reset" severity note;
    done<=true;wait;
  end process;
end;
