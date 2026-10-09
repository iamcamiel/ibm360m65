library ieee;
use ieee.std_logic_1164.all;

-- Standalone electrical panel diagnostic; no ALD CPU or PCIe logic.
entity PANEL_TEST_SERIAL is
  generic (HALF_CYCLES : positive := 50000;
           PATTERN_CYCLES : positive := 100000000);
  port (clk, reset : in std_logic;
        sck, latch_n : out std_logic;
        data : out std_logic_vector(0 to 5);
        pattern_o : out std_logic);
end PANEL_TEST_SERIAL;

architecture RTL of PANEL_TEST_SERIAL is
  signal divider : natural range 0 to HALF_CYCLES-1 := 0;
  signal timer : natural range 0 to PATTERN_CYCLES-1 := 0;
  signal phase, frame_phase, high_half : std_logic := '0';
  signal step : natural range 0 to 41 := 0;
begin
  pattern_o <= frame_phase;
  process(clk, reset)
    variable selected : std_logic;
  begin
    if reset='1' then
      divider<=0; timer<=0; phase<='0'; frame_phase<='0';
      high_half<='0'; step<=0; sck<='0'; latch_n<='1'; data<=(others=>'0');
    elsif rising_edge(clk) then
      if timer=PATTERN_CYCLES-1 then timer<=0; phase<=not phase;
      else timer<=timer+1; end if;
      if divider=HALF_CYCLES-1 then
        divider<=0;
        if high_half='0' then
          sck<='0';
          if step=41 then latch_n<='0'; data<=(others=>'0');
          elsif step<40 then
            -- Capture a pattern once, so a 1 s transition never tears a frame.
            selected:=frame_phase;
            if step=0 then frame_phase<=phase; selected:=phase; end if;
            for bank in 0 to 5 loop
              if ((39-step+bank) mod 2)=0 then data(bank)<=selected;
              else data(bank)<=not selected; end if;
            end loop;
          else data<=(others=>'0'); end if;
          high_half<='1';
        else
          if step=41 then latch_n<='1'; step<=0;
          else
            if step<40 then sck<='1'; end if;
            step<=step+1;
          end if;
          high_half<='0';
        end if;
      else divider<=divider+1; end if;
    end if;
  end process;
end RTL;
