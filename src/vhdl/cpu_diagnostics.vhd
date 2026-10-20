library ieee;
use ieee.std_logic_1164.all;
use ieee.numeric_std.all;

-- Observation uses the settled state immediately BEFORE each core edge.
-- Only halt_o feeds the CPU. PCIe, panel scan and this recorder keep clocking.
entity CPU_DIAGNOSTICS is
  port (clk_i, reset_i : in std_logic;
    state_i : in std_logic_vector(1023 downto 0);
    command_i : in std_logic_vector(31 downto 0);
    command_valid_i : in std_logic;
    response_o : out std_logic_vector(31 downto 0);
    response_valid_o : out std_logic;
    halt_o : out std_logic);
end CPU_DIAGNOSTICS;

architecture rtl of CPU_DIAGNOSTICS is
  type history_type is array(0 to 255) of std_logic_vector(95 downto 0);
  signal history : history_type;
  attribute ram_style : string;
  attribute ram_style of history : signal is "block";
  -- Pipeline only the BRAM write port. Logical head/count and event capture
  -- retain their original edge; same-address reads forward the pending entry.
  signal history_write_valid : std_logic := '0';
  signal history_write_address : unsigned(7 downto 0) := (others=>'0');
  signal history_write_data : std_logic_vector(95 downto 0) := (others=>'0');
  signal previous, before_event, at_event : std_logic_vector(1023 downto 0) := (others=>'0');
  signal ticks, event_time : unsigned(63 downto 0) := (others=>'0');
  signal generation : unsigned(31 downto 0) := (others=>'0');
  signal head : unsigned(7 downto 0) := (others=>'0');
  signal count : natural range 0 to 256 := 0;
  signal armed, frozen, halted, halt_on_event : std_logic := '0';
  signal mask, changed : std_logic_vector(3 downto 0) := (others=>'0');
  -- Configuration changes are accepted only while disarmed. ARM resets hits.
  signal breakpoint_config : std_logic_vector(13 downto 0) := (others=>'0');
  signal breakpoint_target : unsigned(23 downto 0) := to_unsigned(1,24);
  signal breakpoint_hits, event_hits : unsigned(23 downto 0) := (others=>'0');
  signal event_breakpoint_config : std_logic_vector(13 downto 0) := (others=>'0');
  signal event_breakpoint_target : unsigned(23 downto 0) := (others=>'0');
  signal event_reason : std_logic_vector(4 downto 0) := (others=>'0');
  signal stepping, step_p0 : std_logic := '0';
  signal step_start_ros, event_step_start_ros : std_logic_vector(11 downto 0) := (others=>'0');
  signal response : std_logic_vector(31 downto 0) := (others=>'0');
  signal response_valid : std_logic := '0';
  signal history_read_pending : std_logic := '0';
  signal history_read : std_logic_vector(95 downto 0);
  signal history_lane : natural range 0 to 3 := 0;
begin
  halt_o <= halted;
  response_o <= response;
  response_valid_o <= response_valid;

  -- A dedicated RAM process keeps its write enable a single registered net.
  -- Reset invalidates logical contents using count, as before; RAM is not reset.
  process(clk_i)
  begin
    if rising_edge(clk_i) then
      if history_write_valid='1' then
        history(to_integer(history_write_address)) <= history_write_data;
      end if;
    end if;
  end process;

  process(clk_i)
    variable transition : std_logic_vector(3 downto 0);
    variable trigger, check_trigger, breakpoint_hit, breakpoint_trigger, step_trigger, running, record_edge : boolean;
    variable next_hits : unsigned(23 downto 0);
    variable op, index, slot : natural;
    variable entry_flags, status : std_logic_vector(31 downto 0);
  begin
    if rising_edge(clk_i) then
      response_valid <= '0';
      history_write_valid <= '0';
      if reset_i='1' then
        previous <= (others=>'0'); before_event <= (others=>'0'); at_event <= (others=>'0');
        ticks <= (others=>'0'); event_time <= (others=>'0'); generation <= (others=>'0');
        head <= (others=>'0'); count <= 0; armed <= '0'; frozen <= '0'; halted <= '0';
        halt_on_event <= '0'; mask <= (others=>'0'); changed <= (others=>'0');
        breakpoint_config <= (others=>'0'); breakpoint_target <= to_unsigned(1,24);
        breakpoint_hits <= (others=>'0'); event_hits <= (others=>'0');
        event_breakpoint_config <= (others=>'0'); event_breakpoint_target <= (others=>'0');
        event_reason <= (others=>'0'); stepping<='0'; step_p0<='0';
        step_start_ros<=(others=>'0'); event_step_start_ros<=(others=>'0');
        response <= (others=>'0'); history_read_pending <= '0';
      else
        ticks <= ticks+1;
        previous <= state_i;
        -- Suppress reset/configuration edges, without discarding frozen evidence.
        running := state_i(16)='0' and state_i(17)='1';
        transition := (state_i(15 downto 12) xor previous(15 downto 12)) and mask;
        check_trigger := armed='1' and frozen='0' and running and previous(16)='0'
          and previous(17)='1' and transition/="0000";
        record_edge := state_i(11 downto 0)/=previous(11 downto 0)
          or (state_i(20)='1' and previous(20)='0');

        -- Bit13 selects P0 rising samples (including held-address cycles).
        -- Otherwise count entry from a different observed ROSAR. Neither mode
        -- counts every 100 MHz edge; both ignore reset/unconfigured/halt state.
        breakpoint_hit := armed='1' and frozen='0' and halted='0'
          and running and previous(16)='0' and previous(17)='1'
          and breakpoint_config(12)='1'
          and state_i(11 downto 0)=breakpoint_config(11 downto 0)
          and ((breakpoint_config(13)='1' and state_i(20)='1' and previous(20)='0')
            or (breakpoint_config(13)='0' and state_i(11 downto 0)/=previous(11 downto 0)));
        next_hits := breakpoint_hits;
        if breakpoint_hit and breakpoint_hits/=x"FFFFFF" then next_hits:=breakpoint_hits+1; end if;
        breakpoint_trigger := breakpoint_hit and next_hits=breakpoint_target;
        step_trigger := stepping='1' and halted='0' and running
          and previous(16)='0' and previous(17)='1'
          and ((step_p0='1' and state_i(20)='1' and previous(20)='0')
            or (step_p0='0' and state_i(11 downto 0)/=step_start_ros));
        trigger := check_trigger or breakpoint_trigger or step_trigger;
        if breakpoint_hit then breakpoint_hits<=next_hits; end if;

        -- Record ROS changes AND P0 edges (including repeated-address cycles).
        -- Last entry also marks a check event even if ROS/P0 did not change.
        if armed='1' and frozen='0' and halted='0' and running and (record_edge or trigger) then
          entry_flags := state_i(31 downto 0);
          entry_flags(24) := '0';
          if trigger then entry_flags(24) := '1'; end if;
          history_write_valid <= '1';
          history_write_address <= head;
          history_write_data <= std_logic_vector(ticks) & entry_flags;
          head <= head+1;
          if count<256 then count<=count+1; end if;
        end if;
        if trigger then
          before_event <= previous; at_event <= state_i; event_time <= ticks;
          changed <= transition; generation <= generation+1;
          event_reason <= (others=>'0');
          if check_trigger then event_reason(0)<='1'; end if;
          if breakpoint_trigger then event_reason(1)<='1'; end if;
          if step_trigger then
            if step_p0='1' then event_reason(4)<='1'; else event_reason(3)<='1'; end if;
          end if;
          event_step_start_ros<=step_start_ros;
          stepping<='0';
          event_hits <= next_hits;
          event_breakpoint_config <= breakpoint_config;
          event_breakpoint_target <= breakpoint_target;
          frozen <= '1'; armed <= '0';
          -- Registered halt stops subsequent ALD updates; the detection edge
          -- itself can execute. Captured before/at states are never replaced.
          if halt_on_event='1' or stepping='1' then halted<='1'; end if;
        end if;

        -- Synchronous BRAM read: second edge selects the requested DWORD.
        if history_read_pending='1' then
          case history_lane is
            when 0 => response<=history_read(31 downto 0);
            when 1 => response<=history_read(63 downto 32);
            when 2 => response<=history_read(95 downto 64);
            when others => response<=(others=>'0');
          end case;
          response_valid <= '1'; history_read_pending <= '0';
        elsif command_valid_i='1' then
          op := to_integer(unsigned(command_i(31 downto 24)));
          index := to_integer(unsigned(command_i(23 downto 0)));
          response <= x"00000000"; response_valid <= '1';
          -- Commands on an event edge must not rearm, release or overwrite it.
          if trigger then response <= x"E0000003";
          else
            case op is
              when 0 =>
                status := (others=>'0');
                status(0):=armed; status(1):=frozen; status(2):=halted;
                status(3):=halt_on_event; status(7 downto 4):=mask;
                status(11 downto 8):=changed;
                status(20 downto 12):=std_logic_vector(to_unsigned(count,9));
                status(21):=stepping; status(22):=step_p0;
                response <= status;
              when 1 => -- ARM: explicit discard of preceding capture/history.
                if stepping='1' then response<=x"E0000002";
                elsif index<32 then
                  armed<='1'; frozen<='0'; mask<=command_i(3 downto 0);
                  halt_on_event<=command_i(4); changed<=(others=>'0');
                  head<=(others=>'0'); count<=0; previous<=state_i;
                  history_write_valid<='0'; -- discard the old arm's pending entry
                  breakpoint_hits<=(others=>'0'); event_reason<=(others=>'0');
                  event_hits<=(others=>'0'); event_breakpoint_config<=(others=>'0');
                  event_breakpoint_target<=(others=>'0'); event_step_start_ros<=(others=>'0');
                else response<=x"E0000001"; end if;
              when 2 => -- Freeze snapshot/history without halting execution.
                if frozen='0' and stepping='0' then
                  before_event<=previous; at_event<=state_i; event_time<=ticks;
                  changed<=(others=>'0'); generation<=generation+1;
                  event_reason<="00100"; event_step_start_ros<=step_start_ros; event_hits<=next_hits;
                  event_breakpoint_config<=breakpoint_config;
                  event_breakpoint_target<=breakpoint_target;
                  frozen<='1'; armed<='0';
                else response<=x"E0000002"; end if;
              when 3 => halted<='1'; stepping<='0'; -- Explicit cancellation.
              when 4 => -- Explicit release only; do not interrupt an active step.
                if stepping='1' then response<=x"E0000002"; else halted<='0'; end if;
              when 5 => -- Before words0..31, at-event words32..63.
                if frozen='0' then response<=x"E0000002";
                elsif index<32 then response<=before_event(index*32+31 downto index*32);
                elsif index<64 then response<=at_event((index-32)*32+31 downto (index-32)*32);
                else response<=x"E0000001"; end if;
              when 6 => -- Oldest first, 4 DWORD stride: flags, ticks low/high, reserved.
                if frozen='0' then response<=x"E0000002";
                elsif index<count*4 then
                  if count=256 then slot:=to_integer(head+unsigned(command_i(9 downto 2)));
                  else slot:=to_integer(unsigned(command_i(9 downto 2))); end if;
                  -- Read-during-write behavior must not depend on BRAM mode.
                  if history_write_valid='1' and history_write_address=to_unsigned(slot,8) then
                    history_read<=history_write_data;
                  else history_read<=history(slot); end if;
                  history_lane<=to_integer(unsigned(command_i(1 downto 0)));
                  history_read_pending<='1'; response_valid<='0';
                else response<=x"E0000001"; end if;
              when 7 =>
                case index is
                  when 0 => response<=std_logic_vector(generation);
                  when 1 => response<=std_logic_vector(event_time(31 downto 0));
                  when 2 => response<=std_logic_vector(event_time(63 downto 32));
                  when 3 => response<=std_logic_vector(to_unsigned(count,32));
                  when 4 => response<=std_logic_vector(resize(head,32));
                  when 5 => response<=x"00010100"; -- schema1, 256 entries
                  when 6 => response<=std_logic_vector(resize(unsigned(event_reason),32));
                  when 7 => response<=std_logic_vector(resize(event_hits,32));
                  when 8 => response<=std_logic_vector(resize(unsigned(event_breakpoint_config),32));
                  when 9 => response<=std_logic_vector(resize(event_breakpoint_target,32));
                  when 10 => response<=std_logic_vector(resize(unsigned(event_step_start_ros),32));
                  when others => response<=x"E0000001";
                end case;
              when 8 => -- Preserve an active step's eventual stop/capture.
                if stepping='1' then response<=x"E0000002"; else armed<='0'; end if;
              when 9 => -- Address[11:0], enable12, P0-sample mode13.
                if armed='1' then response<=x"E0000002";
                elsif index<16384 then breakpoint_config<=command_i(13 downto 0);
                else response<=x"E0000001"; end if;
              when 10 => -- One-based hit target, 1..16777215.
                if armed='1' then response<=x"E0000002";
                elsif index>0 then breakpoint_target<=unsigned(command_i(23 downto 0));
                else response<=x"E0000001"; end if;
              when 11 => -- Live configuration/progress; never changes capture.
                case index is
                  when 0 => response<=std_logic_vector(resize(unsigned(breakpoint_config),32));
                  when 1 => response<=std_logic_vector(resize(breakpoint_target,32));
                  when 2 => response<=std_logic_vector(resize(breakpoint_hits,32));
                  when others => response<=x"E0000001";
                end case;
              when 12 => -- Run from halt to ROSAR change(0) or next P0 rise(1).
                if index>1 then response<=x"E0000001";
                elsif halted='0' or armed='0' or frozen='1' or stepping='1' then response<=x"E0000002";
                else
                  stepping<='1'; step_p0<=command_i(0);
                  step_start_ros<=state_i(11 downto 0); halted<='0';
                end if;
              when others => response<=x"E0000001";
            end case;
          end if;
        end if;
      end if;
    end if;
  end process;
end rtl;
