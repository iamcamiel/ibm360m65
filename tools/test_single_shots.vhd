library ieee;
use ieee.std_logic_1164.all;

-- Behavioral contract shared by GHDL and ISE ISim. No DUT state is accessed.
entity single_shot_check is
  generic (NAME : string; TICKS : positive;
           INPUT_ACTIVE, OUTPUT_ACTIVE : std_logic;
           FULL_CASES : boolean := true);
  port (clk : in std_logic; rst, hlt, i : out std_logic;
        o : in std_logic; done : out std_logic; errors : out natural);
end;

architecture test of single_shot_check is
begin
  process
    variable failures : natural := 0;
    procedure step(input_value, expected : std_logic;
                   reset_value : std_logic := '0';
                   halt_value : std_logic := '0') is
    begin
      i <= input_value; rst <= reset_value; hlt <= halt_value;
      wait until rising_edge(clk); wait for 1 ns;
      if o /= expected then
        if failures < 3 then
          report NAME & " unexpected output " & std_logic'image(o) &
                 " expected " & std_logic'image(expected) severity note;
        end if;
        failures := failures + 1;
      end if;
      wait until falling_edge(clk);
    end procedure;
    constant INACTIVE : std_logic := not INPUT_ACTIVE;
    constant QUIET : std_logic := not OUTPUT_ACTIVE;
  begin
    done <= '0'; errors <= 0;
    step(INACTIVE, QUIET, '1', '1'); -- Reset takes precedence over halt.
    step(INACTIVE, QUIET);
    -- A held input must produce exactly TICKS active enabled edges.
    for n in 1 to TICKS loop step(INPUT_ACTIVE, OUTPUT_ACTIVE); end loop;
    for n in 1 to 4 loop step(INPUT_ACTIVE, QUIET); end loop;
    step(INACTIVE, QUIET);
    if FULL_CASES then
      -- A one-edge trigger must still produce the full pulse.
      step(INPUT_ACTIVE, OUTPUT_ACTIVE);
      for n in 2 to TICKS loop step(INACTIVE, OUTPUT_ACTIVE); end loop;
      step(INACTIVE, QUIET);
      -- Edges inside a pulse neither extend it nor queue another pulse.
      step(INPUT_ACTIVE, OUTPUT_ACTIVE); step(INACTIVE, OUTPUT_ACTIVE);
      for n in 3 to TICKS loop step(INPUT_ACTIVE, OUTPUT_ACTIVE); end loop;
      for n in 1 to 4 loop step(INPUT_ACTIVE, QUIET); end loop;
      step(INACTIVE, QUIET);
      -- Halt freezes the pulse and input sampling for ten edges.
      step(INPUT_ACTIVE, OUTPUT_ACTIVE);
      for n in 1 to 10 loop step(INACTIVE, OUTPUT_ACTIVE, '0', '1'); end loop;
      for n in 2 to TICKS loop step(INPUT_ACTIVE, OUTPUT_ACTIVE); end loop;
      step(INPUT_ACTIVE, QUIET); step(INACTIVE, QUIET);
      -- Reset cancels an active pulse; a held active input can trigger anew.
      step(INPUT_ACTIVE, OUTPUT_ACTIVE);
      step(INPUT_ACTIVE, QUIET, '1', '1');
      for n in 1 to TICKS loop step(INPUT_ACTIVE, OUTPUT_ACTIVE); end loop;
      step(INPUT_ACTIVE, QUIET); step(INACTIVE, QUIET);
      -- Sample the inactive level on the last active edge, then accept a
      -- new edge at expiry rather than requiring an extra idle rearm edge.
      for n in 1 to TICKS - 1 loop step(INPUT_ACTIVE, OUTPUT_ACTIVE); end loop;
      step(INACTIVE, OUTPUT_ACTIVE);
      for n in 1 to TICKS loop step(INPUT_ACTIVE, OUTPUT_ACTIVE); end loop;
      step(INPUT_ACTIVE, QUIET);
    end if;
    errors <= failures; done <= '1';
    report "SINGLE_SHOT_RESULT " & NAME & " errors=" & integer'image(failures);
    wait;
  end process;
end;
