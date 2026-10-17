architecture diagnostic of EP_MEM is
  signal latest, held : std_logic_vector(735 downto 0) := (others=>'0');
  signal user_reset : std_logic_vector(1 downto 0) := "11";
  attribute ASYNC_REG : string;
  attribute SHREG_EXTRACT : string;
  attribute ASYNC_REG of user_reset : signal is "TRUE";
  attribute SHREG_EXTRACT of user_reset : signal is "NO";
  function read_bank(a : std_logic_vector(8 downto 0);
                     bank : std_logic_vector(735 downto 0)) return std_logic_vector is
    variable n : natural := to_integer(unsigned(a));
  begin
    case n is
      when 0 => return x"03602065";
      when 255 => return x"50444931"; -- BAR0 0x3FC: PDI1 diagnostic identity
      when 256 => return x"504E4C31"; -- captures a complete held PNL1 frame
      when 257 to 279 => return bank((n-256)*32-1 downto (n-257)*32);
      when 506 => return x"50444931"; -- BAR0 0x7E8 diagnostic identity alias
      when 507 => return M65_BUILD_MAGIC;
      when 508 => return M65_BUILD_TIME;
      when 509 => return M65_BUILD_DATE;
      when 510 => return M65_FPGA_VERSION; -- zero: deliberately not a CPU image
      when 511 => return M65_INTERFACE_VERSION;
      when others => return x"00000000";
    end case;
  end;
begin
  P_reg_io_int<=(others=>'0'); P_reg_io_resp<=(others=>'0');
  P_reg_se_rdata_hi<=(others=>'0'); P_reg_se_rdata_lo<=(others=>'0');
  P_reg_se_resp<=(others=>'0'); P_reg_se_size<=(others=>'0');
  cdc_busy_o<=user_reset(1);
  process(clk_i,cdc_reset_i)
  begin
    if cdc_reset_i='1' then user_reset<="11";
    elsif rising_edge(clk_i) then user_reset<=user_reset(0)&'0'; end if;
  end process;
  panel_to_pcie : entity work.CDC_MAILBOX generic map(WIDTH=>736)
    port map(source_clk_i=>core_clk_i,destination_clk_i=>clk_i,
      reset_i=>cdc_reset_i,source_data_i=>panel_snapshot_i,
      source_valid_i=>panel_valid_i,source_ready_o=>open,
      destination_data_o=>latest,destination_valid_o=>open);
  process(clk_i,cdc_reset_i)
  begin
    if cdc_reset_i='1' then
      held<=(others=>'0'); a_rd_d_o_0<=(others=>'0'); b_rd_d_o_0<=(others=>'0');
    elsif rising_edge(clk_i) then
      if user_reset(1)='1' then
        held<=(others=>'0'); a_rd_d_o_0<=(others=>'0'); b_rd_d_o_0<=(others=>'0');
      else
        if a_rd_a_i_0="100000000" then held<=latest; end if;
        a_rd_d_o_0<=read_bank(a_rd_a_i_0,held);
        b_rd_d_o_0<=read_bank(b_wr_a_i_0,held);
        -- Every BAR write is ignored. No CPU, configuration or lamp control.
      end if;
    end if;
  end process;
end;
