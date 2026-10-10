library ieee;
use ieee.std_logic_1164.all;

-- Diagnostic-only wrapper around the private forced-LED BLINKEN copy.
entity PRODUCTION_PANEL_CORE is
  generic(PATTERN_CYCLES : positive := 100000000;
          SCAN_ENABLE_CYCLES : positive := 512);
  port(clk,reset : in std_logic;
       shift_i : in std_logic_vector(0 to 7);
       sck,latch_n,power_off,phase_o,switch_parity : out std_logic;
       shift_o : out std_logic_vector(0 to 5);
       switches_o : out std_logic_vector(191 downto 0));
end;
architecture RTL of PRODUCTION_PANEL_CORE is
  type switch_banks is array(0 to 7) of std_logic_vector(0 to 23);
  type led_banks is array(0 to 5) of std_logic_vector(0 to 39);
  signal sw : switch_banks;
  signal lamps : led_banks;
  signal frame_switches : std_logic_vector(0 to 23):=(others=>'1');
  signal enable,latch,phase,frame_phase : std_logic:='0';
  signal divider : natural range 0 to SCAN_ENABLE_CYCLES-1 := SCAN_ENABLE_CYCLES/2-1;
  signal timer : natural range 0 to PATTERN_CYCLES-1 := 0;
  signal parity_banks : std_logic_vector(0 to 7):=(others=>'0');
  function parity(v : std_logic_vector) return std_logic is
    variable p : std_logic:='0';
  begin for n in v'range loop p:=p xor v(n);end loop;return p;end;
begin
  enable<='1' when divider=SCAN_ENABLE_CYCLES-1 and reset='0' else '0';
  latch_n<=latch;phase_o<=frame_phase;
  switches_o<=sw(0)&sw(1)&sw(2)&sw(3)&sw(4)&sw(5)&sw(6)&sw(7);
  process(clk)
  begin
    if rising_edge(clk) then
      if reset='1' then
        divider<=SCAN_ENABLE_CYCLES/2-1;timer<=0;phase<='0';frame_phase<='0';
        frame_switches<=(others=>'1');parity_banks<=(others=>'0');switch_parity<='0';
      else
        if divider=SCAN_ENABLE_CYCLES-1 then divider<=0;else divider<=divider+1;end if;
        if timer=PATTERN_CYCLES-1 then timer<=0;phase<=not phase;else timer<=timer+1;end if;
        -- All switch samples are complete before this inter-frame latch pulse.
        if latch='0' then frame_switches<=sw(0);frame_phase<=phase;end if;
        for b in 0 to 7 loop parity_banks(b)<=parity(sw(b));end loop;
        switch_parity<=parity(parity_banks);
      end if;
    end if;
  end process;
  banks : for b in 0 to 5 generate
    bits : for n in 0 to 35 generate
      direct : if n<24 generate
        raw : if b<3 generate lamps(b)(n)<=frame_switches(n);end generate;
        inverted : if b>=3 generate lamps(b)(n)<=not frame_switches(n);end generate;
      end generate;
      duplicate_buttons : if n>=24 generate
        raw : if b<3 generate lamps(b)(n)<=frame_switches(n-16);end generate;
        inverted : if b>=3 generate lamps(b)(n)<=not frame_switches(n-16);end generate;
      end generate;
    end generate;
    heartbeat : if b=0 generate lamps(b)(36 to 39)<=(others=>frame_phase);end generate;
    button_lights : if b=3 generate
      lamps(b)(36 to 37)<=(others=>not frame_switches(11));
      lamps(b)(38 to 39)<=(others=>not frame_switches(12));
    end generate;
    unused : if b/=0 and b/=3 generate lamps(b)(36 to 39)<=(others=>'0');end generate;
  end generate;
  panel : entity work.BLINKEN generic map(DIAGNOSTIC_FORCE_LEDS=>true)
    port map(clk=>clk,rst_i=>reset,enable_i=>enable,
      disp_clk_o=>sck,disp_latch_n_o=>latch,disp_shift_o=>shift_o,disp_shift_i=>shift_i,
      sw0=>sw(0),sw1=>sw(1),sw2=>sw(2),sw3=>sw(3),sw4=>sw(4),sw5=>sw(5),sw6=>sw(6),sw7=>sw(7),
      li0=>lamps(0),li1=>lamps(1),li2=>lamps(2),li3=>lamps(3),li4=>lamps(4),li5=>lamps(5),
      configured=>'1',panel_snapshot_o=>open,panel_valid_o=>open,power_off=>power_off);
end;
