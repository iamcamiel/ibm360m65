library ieee;
use ieee.std_logic_1164.all;
use ieee.numeric_std.all;
use work.fpga_build.all;

entity test_pcie_cdc is
  generic (CPU_HALF_PS : positive := 5000; PCI_HALF_PS : positive := 8000;
           PCI_PHASE_PS : natural := 3000);
end;
architecture test of test_pcie_cdc is
  signal cpu_clk, pci_clk : std_logic := '0';
  signal reset : std_logic := '1';
  signal reset_n : std_logic;
  signal done, cpu_running : boolean := false;
  signal rd_addr, wr_addr : std_logic_vector(10 downto 0) := (others => '0');
  signal rd_be : std_logic_vector(3 downto 0) := "1111";
  signal wr_be : std_logic_vector(7 downto 0) := x"0F";
  signal wr_data, rd_data : std_logic_vector(31 downto 0) := (others => '0');
  signal wr_en, busy : std_logic := '0';
  signal io_int, io_resp, rhi, rlo, resp, size : std_logic_vector(31 downto 0);
  signal ext, io_cmd, address, command, whi, wlo : std_logic_vector(31 downto 0) := (others => '0');
  function swap(v : std_logic_vector(31 downto 0)) return std_logic_vector is
  begin return v(7 downto 0) & v(15 downto 8) & v(23 downto 16) & v(31 downto 24); end;
begin
  reset_n <= not reset;
  dut : entity work.PIO_EP_MEM_ACCESS port map (
    clk => pci_clk, rst_n => reset_n, core_clk_i => cpu_clk, cdc_reset_i => reset,
    rd_addr_i => rd_addr, rd_be_i => rd_be, rd_data_o => rd_data,
    wr_addr_i => wr_addr, wr_be_i => wr_be, wr_data_i => wr_data,
    wr_en_i => wr_en, wr_busy_o => busy,
    P_reg_io_int => io_int, P_reg_io_resp => io_resp,
    P_reg_se_rdata_hi => rhi, P_reg_se_rdata_lo => rlo,
    P_reg_se_resp => resp, P_reg_se_size => size,
    P_reg_ext => ext, P_reg_io_cmd => io_cmd, P_reg_se_addr => address,
    P_reg_se_cmd => command, P_reg_se_wdata_hi => whi, P_reg_se_wdata_lo => wlo);
  process
  begin
    while not done loop
      wait for CPU_HALF_PS * 1 ps;
      if cpu_running then cpu_clk <= not cpu_clk; else cpu_clk <= '0'; end if;
    end loop;
    wait;
  end process;
  process
  begin
    wait for PCI_PHASE_PS * 1 ps;
    while not done loop pci_clk <= not pci_clk; wait for PCI_HALF_PS * 1 ps; end loop;
    wait;
  end process;

  process
    procedure pci_edges(n : positive) is
    begin for i in 1 to n loop wait until rising_edge(pci_clk); end loop; wait for 1 ns; end;
    procedure await_idle is
      variable count : natural := 0;
    begin
      while busy /= '0' loop
        pci_edges(1); count := count + 1;
        assert count < 100 report "CDC write failed to finish" severity failure;
      end loop;
    end;
    procedure begin_write(word : natural; value : std_logic_vector(31 downto 0);
                          enables : std_logic_vector(7 downto 0) := x"0F") is
    begin
      await_idle;
      wr_addr <= std_logic_vector(to_unsigned(word, 11)); wr_be <= enables;
      wr_data <= swap(value);
      pci_edges(3); -- RX presents address before its one-cycle write pulse.
      wr_en <= '1'; pci_edges(1); wr_en <= '0';
    end;
    procedure write_word(word : natural; value : std_logic_vector(31 downto 0);
                         enables : std_logic_vector(7 downto 0) := x"0F") is
    begin begin_write(word, value, enables); await_idle; end;
    procedure read_word(word : natural; value : std_logic_vector(31 downto 0)) is
    begin
      rd_addr <= std_logic_vector(to_unsigned(word, 11)); pci_edges(5);
      assert rd_data = swap(value)
        report "BAR read mismatch at word " & integer'image(word) severity failure;
    end;
    variable seq : std_logic_vector(31 downto 0);
  begin
    cpu_running <= true; wait for 47 ns; reset <= '0'; pci_edges(8);
    assert size = x"00000000" and resp = x"00000000" report "reset outputs" severity failure;
    write_word(1, x"007FFFFF"); assert size = x"007FFFFF" severity failure;
    read_word(1, x"007FFFFF");
    -- Read-modify-write must merge against the PCIe-owned register, not an old CPU copy.
    write_word(1, x"ABCDEF12", x"05");
    assert size = x"00CDFF12" report "byte-enable merge" severity failure;
    read_word(1, x"00CDFF12");
    read_word(0, x"03602065");
    read_word(16#1fb#, M65_BUILD_MAGIC); read_word(16#1fc#, M65_BUILD_TIME);
    read_word(16#1fd#, M65_BUILD_DATE); read_word(16#1fe#, M65_FPGA_VERSION);
    write_word(16#1fe#, x"FFFFFFFF"); read_word(16#1fe#, M65_FPGA_VERSION);
    read_word(16#1ff#, M65_INTERFACE_VERSION); read_word(13, M65_INTERFACE_VERSION);

    -- CPU fields settle on adjacent core edges. Command is published only with
    -- coherent address/data; software observes the sequence before reading them.
    for i in 1 to 20 loop
      wait until falling_edge(cpu_clk);
      seq := std_logic_vector(to_unsigned(i mod 4, 2)) & "000000000000000000000000010000";
      command <= seq;
      wait until falling_edge(cpu_clk); address <= std_logic_vector(to_unsigned(i*8, 32));
      wait until falling_edge(cpu_clk); whi <= std_logic_vector(to_unsigned(i*101, 32));
      wlo <= not std_logic_vector(to_unsigned(i*101, 32));
      ext <= x"12345678"; io_cmd <= x"80000001";
      pci_edges(25);
      read_word(6, seq); read_word(7, std_logic_vector(to_unsigned(i*8, 32)));
      read_word(11, std_logic_vector(to_unsigned(i*101, 32)));
      read_word(12, not std_logic_vector(to_unsigned(i*101, 32)));
      write_word(9, std_logic_vector(to_unsigned(i*103, 32)));
      write_word(10, not std_logic_vector(to_unsigned(i*103, 32)));
      -- Response counter is the commit marker. Earlier data writes must arrive first.
      write_word(8, seq);
      assert resp = seq and rhi = std_logic_vector(to_unsigned(i*103, 32)) and
        rlo = not std_logic_vector(to_unsigned(i*103, 32))
        report "response overtook data or mixed words" severity failure;
    end loop;
    write_word(4, x"C0000002"); write_word(5, x"00000055");
    assert io_resp = x"C0000002" and io_int = x"00000055" severity failure;

    -- A stopped receiving clock must retain backpressure, not lose the write.
    cpu_running <= false; wait for 20 ns;
    begin_write(9, x"76543210"); pci_edges(20);
    assert busy = '1' and rhi /= x"76543210" report "stopped CPU lost backpressure" severity failure;
    cpu_running <= true; await_idle;
    assert rhi = x"76543210" report "stalled transfer lost payload" severity failure;

    -- Cancel an in-flight transfer with common reset. No stale request may replay.
    cpu_running <= false; wait for 20 ns;
    begin_write(8, x"FFFFFFFF"); pci_edges(6); reset <= '1'; pci_edges(3);
    cpu_running <= true; reset <= '0'; pci_edges(12); await_idle;
    assert resp = x"00000000" and rhi = x"00000000" and size = x"00000000"
      report "reset replayed stale response" severity failure;
    write_word(1, x"001FFFFF"); assert size = x"001FFFFF" severity failure;
    report "PCIE_CDC_TEST_PASS" severity note; done <= true; wait;
  end process;
end test;
