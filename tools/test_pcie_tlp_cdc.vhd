library ieee;
use ieee.std_logic_1164.all;
use ieee.numeric_std.all;

-- Exercise the unchanged generated RX/TX engines with the production PIO stack.
entity test_pcie_tlp_cdc is end;
architecture test of test_pcie_tlp_cdc is
  signal cpu_clk, pci_clk : std_logic := '0';
  signal reset : std_logic := '1';
  signal reset_n : std_logic;
  signal done : boolean := false;
  signal running : boolean := true;
  signal rd : std_logic_vector(63 downto 0) := (others => '0');
  signal sof_n, eof_n, src_n : std_logic := '1';
  signal ready_n : std_logic;
  signal io_int, io_resp, rhi, rlo, resp, size : std_logic_vector(31 downto 0);
  function swap(v : std_logic_vector(31 downto 0)) return std_logic_vector is
  begin return v(7 downto 0) & v(15 downto 8) & v(23 downto 16) & v(31 downto 24); end;
begin
  reset_n <= not reset;
  dut : entity work.PIO port map (
    trn_clk => pci_clk, trn_reset_n => reset_n, trn_lnk_up_n => '0',
    core_clk_i => cpu_clk, cdc_reset_i => reset,
    trn_td => open, trn_trem_n => open, trn_tsof_n => open, trn_teof_n => open,
    trn_tsrc_rdy_n => open, trn_tsrc_dsc_n => open, trn_tdst_rdy_n => '0', trn_tdst_dsc_n => '1',
    trn_rd => rd, trn_rrem_n => x"00", trn_rsof_n => sof_n, trn_reof_n => eof_n,
    trn_rsrc_rdy_n => src_n, trn_rsrc_dsc_n => '1', trn_rbar_hit_n => "1111110",
    trn_rdst_rdy_n => ready_n, cfg_to_turnoff_n => '1', cfg_turnoff_ok_n => open,
    cfg_completer_id => x"0001", cfg_bus_mstr_enable => '1',
    P_reg_io_int => io_int, P_reg_io_resp => io_resp,
    P_reg_se_rdata_hi => rhi, P_reg_se_rdata_lo => rlo,
    P_reg_se_resp => resp, P_reg_se_size => size,
    P_reg_ext => x"00000000", P_reg_io_cmd => x"00000000", P_reg_se_addr => x"00000000",
    P_reg_se_cmd => x"00000000", P_reg_se_wdata_hi => x"00000000", P_reg_se_wdata_lo => x"00000000");
  process
  begin while not done loop wait for 5 ns; if running then cpu_clk <= not cpu_clk; else cpu_clk <= '0'; end if; end loop; wait; end process;
  process
  begin wait for 3 ns; while not done loop pci_clk <= not pci_clk; wait for 8 ns; end loop; wait; end process;
  process
    procedure edge is
    begin wait until rising_edge(pci_clk); wait for 1 ns; end;
    procedure await_ready is
      variable n : natural := 0;
    begin
      while ready_n /= '0' loop edge; n := n+1;
        assert n < 100 report "RX did not release CDC backpressure" severity failure;
      end loop;
    end;
    procedure start_write(word : natural; value : std_logic_vector(31 downto 0)) is
    begin
      await_ready; wait until falling_edge(pci_clk);
      rd <= x"400000010001000F"; sof_n <= '0'; eof_n <= '1'; src_n <= '0'; edge;
      wait until falling_edge(pci_clk);
      rd <= std_logic_vector(to_unsigned(word*4, 32)) & swap(value);
      sof_n <= '1'; eof_n <= '0'; edge;
      src_n <= '1'; eof_n <= '1';
      assert ready_n = '1' report "RX did not stop after a write" severity failure;
    end;
    procedure write_word(word : natural; value : std_logic_vector(31 downto 0)) is
    begin start_write(word, value); await_ready; end;
    variable value : std_logic_vector(31 downto 0);
  begin
    wait for 49 ns; reset <= '0'; for i in 1 to 8 loop edge; end loop;
    write_word(1, x"007FFFFF"); assert size = x"007FFFFF" report "configuration TLP" severity failure;
    for i in 1 to 32 loop
      value := std_logic_vector(to_unsigned(i*101,32));
      write_word(9, value); assert rhi = value report "TLP data high" severity failure;
      write_word(10, not value); assert rlo = not value report "TLP data low" severity failure;
      write_word(8, value);
      assert resp = value and rhi = value and rlo = not value report "TLP response overtook data" severity failure;
    end loop;
    running <= false; wait for 20 ns; start_write(5, x"00000077");
    for i in 1 to 20 loop edge; assert ready_n = '1' report "RX reopened before CPU capture" severity failure; end loop;
    running <= true; await_ready; assert io_int = x"00000077" severity failure;
    running <= false; wait for 20 ns; start_write(8, x"FFFFFFFF");
    for i in 1 to 5 loop edge; end loop; reset <= '1'; for i in 1 to 3 loop edge; end loop;
    running <= true; reset <= '0'; for i in 1 to 12 loop edge; end loop;
    await_ready;
    assert resp = x"00000000" and size = x"00000000" report "TLP reset replay" severity failure;
    write_word(1, x"003FFFFF"); assert size = x"003FFFFF" severity failure;
    report "PCIE_TLP_CDC_TEST_PASS" severity note; done <= true; wait;
  end process;
end test;
