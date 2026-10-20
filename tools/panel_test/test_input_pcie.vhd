library ieee;
use ieee.std_logic_1164.all;
use ieee.numeric_std.all;
entity test_input_pcie is end;
architecture test of test_input_pcie is
  type banks is array(0 to 7) of std_logic_vector(0 to 23);
  signal values : banks := (others=>(others=>'1'));
  signal core,pci,reset,sck,latch_n,valid,busy,off,phase : std_logic:='0';
  signal done : boolean:=false;
  signal di : std_logic_vector(0 to 7):=(others=>'1');
  signal so : std_logic_vector(0 to 5);
  signal payload : std_logic_vector(735 downto 0);
  signal a,wa : std_logic_vector(8 downto 0):=(others=>'0');
  signal rd,wr,wd : std_logic_vector(31 downto 0):=(others=>'0');
  signal we : std_logic:='0';
  signal io_int,io_resp,hi,lo,resp,size : std_logic_vector(31 downto 0);
begin
  process begin while not done loop core<=not core;wait for 5 ns;end loop;wait;end process;
  process begin wait for 3 ns;while not done loop pci<=not pci;wait for 8 ns;end loop;wait;end process;
  panel : entity work.INPUT_PCIE_CORE generic map(SCAN_ENABLE_CYCLES=>4,PATTERN_CYCLES=>10000)
    port map(clk=>core,reset=>reset,shift_i=>di,sck=>sck,latch_n=>latch_n,shift_o=>so,
      power_off=>off,phase_o=>phase,snapshot_o=>payload,snapshot_valid_o=>valid);
  mem : entity work.EP_MEM port map(clk_i=>pci,core_clk_i=>core,cdc_reset_i=>reset,
    cdc_busy_o=>busy,a_rd_a_i_0=>a,a_rd_d_o_0=>rd,b_wr_a_i_0=>wa,b_rd_d_o_0=>wr,
    b_wr_d_i_0=>wd,b_wr_en_i_0=>we,panel_snapshot_i=>payload,panel_valid_i=>valid,
    P_reg_io_int=>io_int,P_reg_io_resp=>io_resp,P_reg_se_rdata_hi=>hi,P_reg_se_rdata_lo=>lo,
    P_reg_se_resp=>resp,P_reg_se_size=>size,P_reg_ext=>x"FFFFFFFF",P_reg_io_cmd=>x"FFFFFFFF",
    P_reg_se_addr=>x"FFFFFFFF",P_reg_se_cmd=>x"FFFFFFFF",P_reg_se_wdata_hi=>x"FFFFFFFF",P_reg_se_wdata_lo=>x"FFFFFFFF");
  -- Model all eight input chains. Input bit 23 is present before the first SCK edge.
  process(sck,latch_n,reset,values)
    variable n : natural range 0 to 24:=0;
  begin
    if reset='1' or latch_n='0' then n:=0;
    elsif rising_edge(sck) then if n<24 then n:=n+1;end if;end if;
    for b in 0 to 7 loop
      if n<24 then di(b)<=values(b)(23-n);else di(b)<='1';end if;
    end loop;
  end process;
  process
    variable expected : banks;
    variable saved : std_logic_vector(735 downto 0);
    variable old_generation : unsigned(31 downto 0);
    procedure read_word(n:natural) is
    begin a<=std_logic_vector(to_unsigned(n,9));
      for j in 1 to 3 loop wait until rising_edge(pci);end loop;wait for 1 ns;end;
    procedure capture is
    begin read_word(256);assert rd=x"504E4C31" severity failure;end;
    procedure check_inputs(v:banks) is
    begin
      wait for 12 us;capture;
      read_word(257);assert unsigned(rd)>0 report "no complete frames" severity failure;
      read_word(259);assert rd=x"00000004" report "divider metadata" severity failure;
      for b in 0 to 7 loop
        read_word(260+b);
        for n in 0 to 23 loop
          assert rd(n)=v(b)(n) report "PCIe bank/bit order mismatch" severity failure;
        end loop;
        assert rd(31 downto 24)=x"00" severity failure;
      end loop;
    end;
  begin
    reset<='1';wait for 37 ns;reset<='0';
    read_word(255);assert rd=x"50444931" severity failure;
    read_word(510);assert rd=x"00000000" report "diagnostic must not identify as CPU" severity failure;
    expected:=(others=>(others=>'0'));values<=expected;check_inputs(expected);
    -- Every one of the 192 inputs is exercised in both polarities.
    for polarity in 0 to 1 loop
      for bank in 0 to 7 loop
        for bitno in 0 to 23 loop
          if polarity=0 then expected:=(others=>(others=>'0'));expected(bank)(bitno):='1';
          else expected:=(others=>(others=>'1'));expected(bank)(bitno):='0';end if;
          values<=expected;check_inputs(expected);
        end loop;
      end loop;
    end loop;
    capture;
    for n in 1 to 23 loop read_word(256+n);saved((n-1)*32+31 downto (n-1)*32):=rd;end loop;
    old_generation:=unsigned(saved(31 downto 0));
    values<=(others=>(others=>'0'));wait for 12 us;
    for n in 0 to 511 loop
      wa<=std_logic_vector(to_unsigned(n,9));wd<=(others=>'1');we<='1';read_word(1);
      assert rd=x"00000000" report "writes changed inert CPU configuration" severity failure;
    end loop;
    we<='0';
    for n in 1 to 23 loop
      read_word(256+n);assert rd=saved((n-1)*32+31 downto (n-1)*32)
        report "held capture changed without magic read" severity failure;
    end loop;
    capture;read_word(257);assert unsigned(rd)>old_generation severity failure;
    for b in 0 to 7 loop read_word(260+b);assert rd=x"00000000" severity failure;end loop;
    assert io_int=x"00000000" and io_resp=x"00000000" and hi=x"00000000" and
      lo=x"00000000" and resp=x"00000000" and size=x"00000000" severity failure;
    reset<='1';wait for 37 ns;reset<='0';capture;read_word(257);
    assert rd=x"00000000" report "stale reset snapshot" severity failure;
    report "INPUT_PCIE_PASS 192 inputs both polarities, coherent held capture, all 512 writes ignored, identity and reset" severity note;
    done<=true;wait;
  end process;
end;
