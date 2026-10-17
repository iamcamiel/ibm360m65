library ieee;
use ieee.std_logic_1164.all;
use ieee.numeric_std.all;

entity test_cpu_diagnostics is
  generic(CPU_HALF_PS : positive := 5000; PCI_HALF_PS : positive := 8000;
          PCI_PHASE_PS : natural := 3000);
end;
architecture test of test_cpu_diagnostics is
  signal core, pci : std_logic := '0';
  signal reset, busy, we, halted : std_logic := '1';
  signal done : boolean := false;
  signal state : std_logic_vector(1023 downto 0) := (others=>'0');
  signal address, waddress : std_logic_vector(8 downto 0) := (others=>'0');
  signal rd, wrd, wd : std_logic_vector(31 downto 0) := (others=>'0');
  signal io_int, io_resp, hi, lo, resp, size : std_logic_vector(31 downto 0);
begin
  process begin while not done loop core<=not core; wait for CPU_HALF_PS*1 ps; end loop; wait; end process;
  process begin wait for PCI_PHASE_PS*1 ps; while not done loop pci<=not pci; wait for PCI_HALF_PS*1 ps; end loop; wait; end process;
  ep : entity work.EP_MEM port map (
    clk_i=>pci, core_clk_i=>core, cdc_reset_i=>reset, cdc_busy_o=>busy,
    a_rd_a_i_0=>address,a_rd_d_o_0=>rd,b_wr_a_i_0=>waddress,
    b_wr_d_i_0=>wd,b_wr_en_i_0=>we,b_rd_d_o_0=>wrd,
    diagnostic_state_i=>state,diagnostic_halt_o=>halted,
    P_reg_io_int=>io_int,P_reg_io_resp=>io_resp,P_reg_se_rdata_hi=>hi,
    P_reg_se_rdata_lo=>lo,P_reg_se_resp=>resp,P_reg_se_size=>size,
    P_reg_ext=>x"12345678",P_reg_io_cmd=>x"00000000",P_reg_se_addr=>x"00000090",
    P_reg_se_cmd=>x"00000000",P_reg_se_wdata_hi=>x"00000000",P_reg_se_wdata_lo=>x"00000000");
  process
    variable value, sequence_number, snapshot, first_time, last_time : std_logic_vector(31 downto 0);
    procedure read_word(n : natural; variable v : out std_logic_vector(31 downto 0)) is
    begin
      address<=std_logic_vector(to_unsigned(n,9));
      for k in 1 to 3 loop wait until rising_edge(pci); end loop;
      wait for 1 ns; v:=rd;
    end;
    procedure write_word(n : natural; v : std_logic_vector(31 downto 0)) is
    begin
      for k in 1 to 300 loop
        wait until falling_edge(pci); exit when busy='0';
        assert k<300 report "write port stuck" severity failure;
      end loop;
      waddress<=std_logic_vector(to_unsigned(n,9));wd<=v;we<='1';
      wait until falling_edge(pci);we<='0';wait for 1 ns;
    end;
    procedure command(op, arg : natural; variable v : out std_logic_vector(31 downto 0)) is
      variable seq, complete : std_logic_vector(31 downto 0);
    begin
      read_word(385,seq);
      write_word(384,std_logic_vector(to_unsigned(16#A0#+op,8)) & std_logic_vector(to_unsigned(arg,24)));
      for k in 1 to 300 loop
        read_word(386,complete); exit when unsigned(complete)=unsigned(seq)+1;
        assert k<300 report "diagnostic response timed out" severity failure;
      end loop;
      read_word(387,v);
    end;
    procedure tick is begin wait until falling_edge(core); end;
  begin
    we<='0';wait for 40 ns; reset<='0';wait for 200 ns;
    state(17)<='1';state(11 downto 0)<=x"010";state(63 downto 32)<=x"00000090";
    wait for 100 ns;
    read_word(384,value);assert value=x"44424731" report "DBG1 signature" severity failure;
    read_word(511,value);assert value=x"00010007" report "interface revision" severity failure;
    -- Disarmed check changes have no side effects.
    state(13)<='1';wait for 40 ns;
    command(0,0,value);assert value(2 downto 0)="000" severity failure;
    state(13)<='0';wait for 40 ns;
    command(1,31,value); -- both passes, both indicators, automatic halt
    tick;state(100 downto 96)<="10101";wait for 30 ns;
    state(13)<='1';wait for 60 ns;
    assert halted='1' report "halt on storage indicator transition" severity failure;
    command(0,0,value);
    assert value(2 downto 0)="110" and value(11 downto 8)="0010" severity failure;
    command(5,0,value);assert value(13)='0' report "before check snapshot" severity failure;
    command(5,32,value);assert value(13)='1' report "event check snapshot" severity failure;
    command(5,33,value);assert value=x"00000090" report "IC at event" severity failure;
    command(5,35,value);assert value(4 downto 0)="10101" report "state payload" severity failure;
    -- A second event or power-panel reset cannot overwrite the first one.
    state(12)<='1';state(16)<='1';state(63 downto 32)<=x"00009000";wait for 100 ns;
    command(5,33,value);assert value=x"00000090" report "sticky snapshot overwritten" severity failure;
    command(7,0,value);assert value=x"00000001" report "event count" severity failure;
    -- Rearming cannot implicitly resume a halted CPU.
    command(1,31,value);assert halted='1' severity failure;
    command(4,0,value);assert halted='0' report "explicit release" severity failure;
    state(12)<='0';state(13)<='0';state(16)<='0';wait for 100 ns;
    command(1,20,value); -- first-pass PROC only, halt enabled
    state(14)<='1';wait for 60 ns;
    command(0,0,value);assert value(2)='1' and value(11 downto 8)="0100"
      report "first-pass-only event was missed" severity failure;
    command(1,4,value); -- falling edges are diagnostic events too
    command(4,0,value);
    state(14)<='0';wait for 40 ns;
    command(0,0,value);assert value(1)='1' and value(11 downto 8)="0100"
      report "falling check transition was missed" severity failure;
    command(1,0,value); -- ring-only mode, retain repeated-address P0 cycles
    for n in 0 to 5 loop
      tick;state(20)<='1';tick;state(20)<='0';tick;
    end loop;
    command(2,0,value);command(7,3,value);
    assert unsigned(value)=6 report "same-address microcycles not recorded" severity failure;
    command(1,15,value); -- capture without halt; populate/wrap ring
    for n in 0 to 319 loop
      tick;state(11 downto 0)<=std_logic_vector(to_unsigned(n,12));
      state(20)<='0';tick;tick;
    end loop;
    command(2,0,value);assert halted='0' report "manual snapshot halted CPU" severity failure;
    command(7,3,value);assert unsigned(value)=256 report "ring did not wrap" severity failure;
    command(6,0,value);assert unsigned(value(11 downto 0))=64 report "oldest ring entry" severity failure;
    command(6,1,first_time);
    command(6,1020,value);assert unsigned(value(11 downto 0))=319 report "newest ring entry" severity failure;
    command(6,1023,value);assert value=x"00000000" report "reserved history word" severity failure;
    command(6,1021,last_time);assert unsigned(last_time)>unsigned(first_time) report "history time order" severity failure;
    -- Continued execution changes live state but not the frozen ring/snapshot.
    state(11 downto 0)<=x"BAD";wait for 40 ns;
    command(6,1020,value);assert unsigned(value(11 downto 0))=319 severity failure;
    command(6,1024,value);assert value=x"E0000001" report "history bounds" severity failure;
    command(5,64,value);assert value=x"E0000001" report "snapshot bounds" severity failure;
    -- Manual halt and continued ordinary PCIe writes while halted.
    command(3,0,value);assert halted='1' severity failure;
    write_word(8,x"C0000000");wait for 500 ns;
    assert resp=x"C0000000" report "memory response transport stopped on halt" severity failure;
    command(0,0,value);assert value(2)='1' report "PCIe lost while halted" severity failure;
    command(4,0,value);assert halted='0' severity failure;
    -- New breakpoint commands cross the same PCIe/core mailbox.
    command(8,0,value);
    command(10,0,value);assert value=x"E0000001" report "zero target accepted" severity failure;
    command(9,16384,value);assert value=x"E0000001" report "config reserved bits accepted" severity failure;
    command(10,3,value);assert value=x"00000000" severity failure;
    command(9,16#3A48#,value);assert value=x"00000000" severity failure;
    state(20)<='0';state(11 downto 0)<=x"A48";wait for 50 ns;
    command(1,16,value); -- breakpoint enabled, check mask zero, halt on event
    command(9,0,value);assert value=x"E0000002" report "armed config changed" severity failure;
    command(10,1,value);assert value=x"E0000002" report "armed target changed" severity failure;
    -- Address match held for many core edges is not a P0 hit.
    wait for 500 ns;command(11,2,value);assert unsigned(value)=0 severity failure;
    -- Reset, unconfigured storage, wrong address and halt cannot consume hits.
    state(16)<='1';wait for 50 ns;tick;state(20)<='1';tick;tick;state(20)<='0';tick;
    state(16)<='0';state(17)<='0';wait for 50 ns;tick;state(20)<='1';tick;tick;state(20)<='0';tick;
    state(17)<='1';state(11 downto 0)<=x"A49";wait for 50 ns;
    tick;state(20)<='1';tick;tick;state(20)<='0';tick;
    command(3,0,value);state(11 downto 0)<=x"A48";wait for 50 ns;
    tick;state(20)<='1';tick;tick;state(20)<='0';tick;
    command(11,2,value);assert unsigned(value)=0 severity failure;
    command(4,0,value);
    for n in 1 to 2 loop
      tick;state(20)<='1';tick;tick;state(20)<='0';tick;
      command(11,2,value);assert unsigned(value)=n report "P0 hit count" severity failure;
      assert halted='0' report "breakpoint fired early" severity failure;
    end loop;
    tick;state(20)<='1';tick;tick;state(20)<='0';tick;
    command(0,0,value);assert value(2 downto 0)="110" and value(11 downto 8)="0000"
      report "Nth P0 breakpoint did not freeze and halt" severity failure;
    command(7,6,value);assert value=x"00000002" report "breakpoint reason" severity failure;
    command(7,7,value);assert unsigned(value)=3 severity failure;
    command(5,32,value);assert value(11 downto 0)=x"A48" and value(20)='1' severity failure;
    command(9,16#1A49#,value);command(10,1,value); -- disarmed config may change, frozen metadata must not
    command(7,8,value);assert value=x"00003A48" report "frozen config overwritten" severity failure;
    command(7,9,value);assert unsigned(value)=3 severity failure;
    command(1,0,value);assert halted='1' report "breakpoint ARM released halt" severity failure;
    command(11,2,value);assert unsigned(value)=0 report "ARM did not reset hits" severity failure;
    command(4,0,value);
    state(11 downto 0)<=x"A49";wait for 50 ns; -- entry mode needs no P0
    command(0,0,value);assert value(1)='1' and value(2)='0' report "entry breakpoint" severity failure;
    command(7,6,value);assert unsigned(value)=2 severity failure;
    command(9,0,value);command(1,0,value); -- disabled address cannot trigger
    state(11 downto 0)<=x"000";wait for 50 ns;state(11 downto 0)<=x"A49";wait for 50 ns;
    tick;state(20)<='1';tick;tick;state(20)<='0';tick;
    command(0,0,value);assert value(1 downto 0)="01" report "disabled breakpoint triggered" severity failure;
    command(2,0,value);command(7,6,value);assert unsigned(value)=4 report "manual capture reason" severity failure;
    -- Single stepping never implicitly discards a capture or starts a running CPU.
    command(12,0,value);assert value=x"E0000002" report "step accepted from running" severity failure;
    command(3,0,value);command(12,1,value);assert value=x"E0000002" report "step discarded frozen capture" severity failure;
    command(1,0,value);assert halted='1' severity failure;
    command(12,2,value);assert value=x"E0000001" severity failure;
    command(12,0,value);assert value=x"00000000" and halted='0' severity failure;
    wait for 100 ns;
    command(0,0,value);assert value(21)='1' and value(1)='0' report "held ROS ended step" severity failure;
    for op in 1 to 2 loop command(op,0,value);assert value=x"E0000002" severity failure;end loop;
    command(4,0,value);assert value=x"E0000002" severity failure;
    command(8,0,value);assert value=x"E0000002" severity failure;
    command(12,0,value);assert value=x"E0000002" report "duplicate step accepted" severity failure;
    tick;state(20)<='1';tick;tick;state(20)<='0';tick;assert halted='0' severity failure;
    state(11 downto 0)<=x"A4A";wait for 50 ns;
    assert halted='1' report "ROS step failed to halt" severity failure;
    command(7,6,value);assert unsigned(value)=8 report "ROS step reason" severity failure;
    command(7,10,value);assert value=x"00000A49" report "step start ROS" severity failure;
    command(5,32,value);assert value(11 downto 0)=x"A4A" severity failure;
    -- Starting with P0 high must wait for its fall and the next rise.
    state(20)<='1';wait for 50 ns;command(1,0,value);command(12,1,value);
    wait for 100 ns;assert halted='0' report "P0 high level ended step" severity failure;
    state(11 downto 0)<=x"A4B";wait for 50 ns;assert halted='0' report "ROS change ended P0 step" severity failure;
    tick;state(20)<='0';tick;tick;state(20)<='1';tick;tick;
    assert halted='1' report "P0 step failed to halt" severity failure;
    command(7,6,value);assert unsigned(value)=16 report "P0 step reason" severity failure;
    command(0,0,value);assert value(21)='0' and value(2 downto 0)="110" severity failure;
    -- Earlier check events preempt stepping and force halt even without halt-on-event.
    state(20)<='0';state(12)<='0';wait for 50 ns;
    command(1,1,value);command(12,0,value);state(12)<='1';wait for 50 ns;
    assert halted='1' report "check did not preempt step" severity failure;
    command(7,6,value);assert unsigned(value)=1 severity failure;
    -- Explicit HALT can cancel an unfinished step; it does not manufacture a capture.
    command(1,0,value);command(12,0,value);command(3,0,value);
    command(0,0,value);assert value(21)='0' and value(2 downto 0)="101" severity failure;
    command(8,0,value);command(12,0,value);assert value=x"E0000002" report "disarmed step accepted" severity failure;
    command(4,0,value);
    -- Bad command prefix is ignored, so accidental non-control writes cannot halt.
    read_word(385,sequence_number);write_word(384,x"03000000");wait for 500 ns;
    read_word(385,value);assert value=sequence_number and halted='0' severity failure;
    report "CPU_DIAGNOSTICS_TEST_PASS" severity note;done<=true;wait;
  end process;
end test;
