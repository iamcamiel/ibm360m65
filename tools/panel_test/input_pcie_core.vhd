library ieee;
use ieee.std_logic_1164.all;
use ieee.numeric_std.all;

-- Standalone diagnostic: sampled inputs are never assigned a button polarity.
entity INPUT_PCIE_CORE is
  generic(SCAN_ENABLE_CYCLES : positive := 1024;
          PATTERN_CYCLES : positive := 100000000);
  port(clk, reset : in std_logic;
       shift_i : in std_logic_vector(0 to 7);
       sck, latch_n, power_off, phase_o : out std_logic;
       shift_o : out std_logic_vector(0 to 5);
       snapshot_o : out std_logic_vector(735 downto 0);
       snapshot_valid_o : out std_logic);
end;
architecture rtl of INPUT_PCIE_CORE is
  type led_banks is array(0 to 5) of std_logic_vector(0 to 39);
  signal lamps : led_banks;
  signal divider : natural range 0 to SCAN_ENABLE_CYCLES-1 := SCAN_ENABLE_CYCLES/2-1;
  signal timer : natural range 0 to PATTERN_CYCLES-1 := 0;
  signal phase, held_phase, enable, latch : std_logic := '0';
  signal payload : std_logic_vector(735 downto 0);
begin
  enable <= '1' when divider=SCAN_ENABLE_CYCLES-1 and reset='0' else '0';
  latch_n <= latch;
  phase_o <= held_phase;
  -- Replace the production snapshot's fixed divider metadata with this divider.
  snapshot_o <= payload(735 downto 96) &
                std_logic_vector(to_unsigned(SCAN_ENABLE_CYCLES,32)) & payload(63 downto 0);
  process(clk)
  begin
    if rising_edge(clk) then
      if reset='1' then
        divider<=SCAN_ENABLE_CYCLES/2-1; timer<=0; phase<='0'; held_phase<='0';
      else
        if divider=SCAN_ENABLE_CYCLES-1 then divider<=0; else divider<=divider+1; end if;
        if timer=PATTERN_CYCLES-1 then timer<=0; phase<=not phase; else timer<=timer+1; end if;
        if latch='0' then held_phase<=phase; end if;
      end if;
    end if;
  end process;
  banks : for b in 0 to 5 generate
    bits : for n in 0 to 39 generate
      even_bit : if (n+b) mod 2=0 generate lamps(b)(n)<=held_phase; end generate;
      odd_bit : if (n+b) mod 2=1 generate lamps(b)(n)<=not held_phase; end generate;
    end generate;
  end generate;
  blinken : entity work.BLINKEN generic map(DIAGNOSTIC_FORCE_LEDS=>true)
    port map(clk=>clk, rst_i=>reset, enable_i=>enable,
      disp_clk_o=>sck, disp_latch_n_o=>latch, disp_shift_o=>shift_o, disp_shift_i=>shift_i,
      sw0=>open,sw1=>open,sw2=>open,sw3=>open,sw4=>open,sw5=>open,sw6=>open,sw7=>open,
      li0=>lamps(0),li1=>lamps(1),li2=>lamps(2),li3=>lamps(3),li4=>lamps(4),li5=>lamps(5),
      configured=>'1', power_off=>power_off,
      panel_snapshot_o=>payload, panel_valid_o=>snapshot_valid_o);
end;
