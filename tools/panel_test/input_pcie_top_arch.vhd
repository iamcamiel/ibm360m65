architecture diagnostic of IBM360 is
  signal clk200, clk, core_ready, sys_reset_n_c : std_logic;
  signal payload : std_logic_vector(735 downto 0);
  signal valid, power_off, phase : std_logic;
  signal pcie_reset_status,pcie_link_status : std_logic;
  signal pcie_status_meta,pcie_status_sync : std_logic_vector(1 downto 0):="00";
  attribute ASYNC_REG : string;
  attribute SHREG_EXTRACT : string;
  attribute ASYNC_REG of pcie_status_meta,pcie_status_sync : signal is "TRUE";
  attribute SHREG_EXTRACT of pcie_status_meta,pcie_status_sync : signal is "NO";
begin
  fpgaclk_ibuf : IBUFDS port map(I=>clk_fpga_p,IB=>clk_fpga_n,O=>clk200);
  core_clock : entity work.CORE_CLOCK100 port map(clk200_i=>clk200,
    reset_i=>not sys_reset_n_c,clk100_o=>clk,ready_o=>core_ready);
  panel : entity work.INPUT_PCIE_CORE
    port map(clk=>clk,reset=>not core_ready,shift_i=>disp_shift_i,
      sck=>disp_clk_o,latch_n=>disp_latch_n_o,shift_o=>disp_shift_o,
      power_off=>power_off,phase_o=>phase,snapshot_o=>payload,snapshot_valid_o=>valid);
  pcie : entity work.XILINX_PCI_EXP_EP port map(
    core_clk_i=>clk,core_ready_i=>core_ready,panel_snapshot_i=>payload,panel_valid_i=>valid,
    pci_exp_txp(0)=>pci_exp_txp,pci_exp_txn(0)=>pci_exp_txn,
    pci_exp_rxp(0)=>pci_exp_rxp,pci_exp_rxn(0)=>pci_exp_rxn,
    sys_clk_p=>sys_clk_p,sys_clk_n=>sys_clk_n,sys_reset_n=>sys_reset_n,
    sys_reset_n_buf_o=>sys_reset_n_c,trn_reset_n=>pcie_reset_status,trn_lnk_up_n=>pcie_link_status,
    P_reg_io_int=>open,P_reg_io_resp=>open,P_reg_se_rdata_hi=>open,
    P_reg_se_rdata_lo=>open,P_reg_se_resp=>open,P_reg_se_size=>open,
    P_reg_ext=>x"00000000",P_reg_io_cmd=>x"00000000",P_reg_se_addr=>x"00000000",
    P_reg_se_cmd=>x"00000000",P_reg_se_wdata_hi=>x"00000000",P_reg_se_wdata_lo=>x"00000000");
  process(clk,core_ready)
  begin
    if core_ready='0' then pcie_status_meta<="00";pcie_status_sync<="00";
    elsif rising_edge(clk) then
      pcie_status_meta<=pcie_link_status & pcie_reset_status;
      pcie_status_sync<=pcie_status_meta;
    end if;
  end process;
  led_4<=core_ready & not power_off & not pcie_status_sync(1) & phase;
  led_10<=not pcie_status_sync(0) & not pcie_status_sync(1) & "00000000";
end;
