-- * IBM 360 Model 65 Emulator
-- * Copyright (C) 2024 Camiel Vanderhoeven
-- *
-- * This program is free software: you can redistribute it and/or modify
-- * it under the terms of the GNU General Public License as published by
-- * the Free Software Foundation, either version 3 of the License, or
-- * (at your option) any later version.
-- *
-- * This program is distributed in the hope that it will be useful,
-- * but WITHOUT ANY WARRANTY; without even the implied warranty of
-- * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
-- * GNU General Public License for more details.
-- *
-- * You should have received a copy of the GNU General Public License
-- * along with this program.  If not, see <http://www.gnu.org/licenses/>.

library ieee;
use ieee.std_logic_1164.all;
use ieee.numeric_std.all;
use work.fpga_build.all;

entity EP_MEM is port (

  clk_i : in std_logic;
  core_clk_i, cdc_reset_i : in std_logic;
  cdc_busy_o : out std_logic;

  a_rd_a_i_0 : in std_logic_vector(8 downto 0);
  a_rd_d_o_0 : out std_logic_vector(31 downto 0);

  b_wr_a_i_0 : in std_logic_vector(8 downto 0);
  b_wr_d_i_0 : in std_logic_vector(31 downto 0);
  b_wr_en_i_0 : in std_logic ;
  b_rd_d_o_0 : out std_logic_vector(31 downto 0);
  
    panel_snapshot_i : in std_logic_vector(735 downto 0) := (others => '0');
    panel_valid_i : in std_logic := '0';
    P_reg_io_int : buffer STD_LOGIC_VECTOR (31 downto 0);
    P_reg_io_resp : buffer STD_LOGIC_VECTOR (31 downto 0);
    P_reg_se_rdata_hi : buffer STD_LOGIC_VECTOR (31 downto 0);
    P_reg_se_rdata_lo : buffer STD_LOGIC_VECTOR (31 downto 0);
    P_reg_se_resp : buffer STD_LOGIC_VECTOR (31 downto 0);
    P_reg_se_size : buffer STD_LOGIC_VECTOR (31 downto 0);
    P_reg_ext : in STD_LOGIC_VECTOR (31 downto 0);
    P_reg_io_cmd : in STD_LOGIC_VECTOR (31 downto 0);
    P_reg_se_addr : in STD_LOGIC_VECTOR (31 downto 0);
    P_reg_se_cmd : in STD_LOGIC_VECTOR (31 downto 0);
    P_reg_se_wdata_hi : in STD_LOGIC_VECTOR (31 downto 0);
    P_reg_se_wdata_lo : in STD_LOGIC_VECTOR (31 downto 0)


);
	
end EP_MEM;

architecture rtl of EP_MEM is

  -- BAR writes live in the PCIe domain; CPU inputs are coherent snapshots.
  signal cpu_payload, cpu_previous, command_snapshot : std_logic_vector(191 downto 0) := (others => '0');
  signal response_payload, response_snapshot : std_logic_vector(191 downto 0);
  signal command_valid, response_pending : std_logic := '0';
  signal response_ready : std_logic;
  signal stable_edges : integer range 0 to 2 := 0;
  signal user_reset : std_logic_vector(1 downto 0) := "11";
  signal cpu_reset : std_logic_vector(1 downto 0) := "11";
  attribute ASYNC_REG : string;
  attribute SHREG_EXTRACT : string;
  attribute ASYNC_REG of user_reset, cpu_reset : signal is "TRUE";
  attribute SHREG_EXTRACT of user_reset, cpu_reset : signal is "NO";
  signal pci_io_int : std_logic_vector(31 downto 0) := (others => '0');
  signal pci_io_resp : std_logic_vector(31 downto 0) := (others => '0');
  signal pci_se_rdata_hi : std_logic_vector(31 downto 0) := (others => '0');
  signal pci_se_rdata_lo : std_logic_vector(31 downto 0) := (others => '0');
  signal pci_se_resp : std_logic_vector(31 downto 0) := (others => '0');
  signal pci_se_size : std_logic_vector(31 downto 0) := (others => '0');
  signal panel_latest, panel_view : std_logic_vector(735 downto 0) := (others => '0');
  signal snapshot_ext : std_logic_vector(31 downto 0);
  signal snapshot_io_cmd : std_logic_vector(31 downto 0);
  signal snapshot_se_addr : std_logic_vector(31 downto 0);
  signal snapshot_se_cmd : std_logic_vector(31 downto 0);
  signal snapshot_se_wdata_hi : std_logic_vector(31 downto 0);
  signal snapshot_se_wdata_lo : std_logic_vector(31 downto 0);

begin

  process(clk_i, cdc_reset_i)
  begin
    if cdc_reset_i = '1' then user_reset <= "11";
    elsif rising_edge(clk_i) then user_reset <= user_reset(0) & '0';
    end if;
  end process;

  process(core_clk_i, cdc_reset_i)
  begin
    if cdc_reset_i = '1' then cpu_reset <= "11";
    elsif rising_edge(core_clk_i) then cpu_reset <= cpu_reset(0) & '0';
    end if;
  end process;

  -- WA is registered but related outputs can settle on adjacent core edges.
  -- Publish only after the whole bundle has been unchanged for two edges.
  cpu_payload <= P_reg_ext & P_reg_io_cmd & P_reg_se_addr & P_reg_se_cmd &
                 P_reg_se_wdata_hi & P_reg_se_wdata_lo;
  process(core_clk_i, cdc_reset_i)
  begin
    if cdc_reset_i = '1' then
      cpu_previous <= (others => '0'); stable_edges <= 0;
    elsif rising_edge(core_clk_i) then
      if cpu_reset(1) = '1' then
        cpu_previous <= (others => '0'); stable_edges <= 0;
      else
        cpu_previous <= cpu_payload;
        if cpu_payload /= cpu_previous then stable_edges <= 0;
        elsif stable_edges < 2 then stable_edges <= stable_edges + 1;
        end if;
      end if;
    end if;
  end process;
  command_valid <= '1' when stable_edges = 2 and cpu_payload = cpu_previous else '0';
  cpu_to_pcie : entity work.CDC_MAILBOX generic map (WIDTH => 192)
    port map (source_clk_i => core_clk_i, destination_clk_i => clk_i,
      reset_i => cdc_reset_i, source_data_i => cpu_payload,
      source_valid_i => command_valid, source_ready_o => open,
      destination_data_o => command_snapshot, destination_valid_o => open);

  -- Independent observation mailbox: it never stalls the CPU service path.
  panel_to_pcie : entity work.CDC_MAILBOX generic map (WIDTH => 736)
    port map (source_clk_i => core_clk_i, destination_clk_i => clk_i,
      reset_i => cdc_reset_i, source_data_i => panel_snapshot_i,
      source_valid_i => panel_valid_i, source_ready_o => open,
      destination_data_o => panel_latest, destination_valid_o => open);

  response_payload <= pci_io_int & pci_io_resp & pci_se_rdata_hi &
                      pci_se_rdata_lo & pci_se_resp & pci_se_size;
  pcie_to_cpu : entity work.CDC_MAILBOX generic map (WIDTH => 192)
    port map (source_clk_i => clk_i, destination_clk_i => core_clk_i,
      reset_i => cdc_reset_i, source_data_i => response_payload,
      source_valid_i => response_pending, source_ready_o => response_ready,
      destination_data_o => response_snapshot, destination_valid_o => open);
  -- RX must not accept another write until the preceding write reached CPU.
  -- PIO_EP_MEM_ACCESS also includes its write_en edge in wr_busy_o.
  -- user_reset and mailbox readiness already assert asynchronously and
  -- release locally. Raw cdc_reset_i must not feed PCIe synchronous control.
  cdc_busy_o <= response_pending or not response_ready or user_reset(1);
  snapshot_ext <= command_snapshot(191 downto 160);
  snapshot_io_cmd <= command_snapshot(159 downto 128);
  snapshot_se_addr <= command_snapshot(127 downto 96);
  snapshot_se_cmd <= command_snapshot(95 downto 64);
  snapshot_se_wdata_hi <= command_snapshot(63 downto 32);
  snapshot_se_wdata_lo <= command_snapshot(31 downto 0);
  P_reg_io_int <= response_snapshot(191 downto 160);
  P_reg_io_resp <= response_snapshot(159 downto 128);
  P_reg_se_rdata_hi <= response_snapshot(127 downto 96);
  P_reg_se_rdata_lo <= response_snapshot(95 downto 64);
  P_reg_se_resp <= response_snapshot(63 downto 32);
  P_reg_se_size <= response_snapshot(31 downto 0);

--#define M65_REG_CFG 0
--#define M65_REG_EXT 1
--#define M65_REG_IO_CMD 2
--#define M65_REG_IO_RESP 3
--#define M65_REG_IO_INT 4
--#define M65_REG_SE_CMD 5
--#define M65_REG_SE_ADDR 6
--#define M65_REG_SE_RESP 7
--#define M65_REG_SE_RDATA_HI 8
--#define M65_REG_SE_RDATA_LO 9
--#define M65_REG_SE_WDATA_HI 10
--#define M65_REG_SE_WDATA_LO 11

	process (clk_i, cdc_reset_i)
	begin
        if cdc_reset_i = '1' then
            response_pending <= '0'; panel_view <= (others => '0');
            a_rd_d_o_0 <= (others => '0'); b_rd_d_o_0 <= (others => '0');
            pci_io_int <= (others => '0');
            pci_io_resp <= (others => '0');
            pci_se_rdata_hi <= (others => '0');
            pci_se_rdata_lo <= (others => '0');
            pci_se_resp <= (others => '0');
            pci_se_size <= (others => '0');
        elsif rising_edge(clk_i) then
          if user_reset(1) = '1' then
            response_pending <= '0'; panel_view <= (others => '0');
            pci_io_int <= (others => '0');
            pci_io_resp <= (others => '0');
            pci_se_rdata_hi <= (others => '0');
            pci_se_rdata_lo <= (others => '0');
            pci_se_resp <= (others => '0');
            pci_se_size <= (others => '0');
          else
            if response_pending = '1' and response_ready = '1' then
              response_pending <= '0';
            end if;
            -- Reading magic (BAR0 0x400) captures the latest complete frame.
            -- Subsequent reads use this stable bank, even as new scans arrive.
            -- Only this read address captures; BAR writes are ignored here.
            if a_rd_a_i_0 = "100000000" then panel_view <= panel_latest; end if;
			case (a_rd_a_i_0(8 downto 0)) is 
				when "000000000" => a_rd_d_o_0 <= "00000011011000000010000001100101"; -- 03602065
				when "000000001" => a_rd_d_o_0 <= pci_se_size;
				when "000000010" => a_rd_d_o_0 <= snapshot_ext;
				when "000000011" => a_rd_d_o_0 <= snapshot_io_cmd;
				when "000000100" => a_rd_d_o_0 <= pci_io_resp;
				when "000000101" => a_rd_d_o_0 <= pci_io_int;
				when "000000110" => a_rd_d_o_0 <= snapshot_se_cmd;
				when "000000111" => a_rd_d_o_0 <= snapshot_se_addr;
				when "000001000" => a_rd_d_o_0 <= pci_se_resp;
				when "000001001" => a_rd_d_o_0 <= pci_se_rdata_hi;
				when "000001010" => a_rd_d_o_0 <= pci_se_rdata_lo;
				when "000001011" => a_rd_d_o_0 <= snapshot_se_wdata_hi;
				when "000001100" => a_rd_d_o_0 <= snapshot_se_wdata_lo;
				when "100000000" => a_rd_d_o_0 <= x"504E4C31"; -- PNL1
				when "100000001" | "100000010" | "100000011" | "100000100" | "100000101" | "100000110" | "100000111" | "100001000" | "100001001" | "100001010" | "100001011" | "100001100" | "100001101" | "100001110" | "100001111" | "100010000" | "100010001" | "100010010" | "100010011" | "100010100" | "100010101" | "100010110" | "100010111" => a_rd_d_o_0 <= panel_view((to_integer(unsigned(a_rd_a_i_0))-256)*32-1 downto (to_integer(unsigned(a_rd_a_i_0))-257)*32);
				when "111111011" => a_rd_d_o_0 <= M65_BUILD_MAGIC; -- BAR0 0x7EC
				when "111111100" => a_rd_d_o_0 <= M65_BUILD_TIME; -- BAR0 0x7F0
				when "111111101" => a_rd_d_o_0 <= M65_BUILD_DATE; -- BAR0 0x7F4
				when "111111110" => a_rd_d_o_0 <= M65_FPGA_VERSION; -- BAR0 0x7F8
				when others => a_rd_d_o_0 <= M65_INTERFACE_VERSION; -- BAR0 0x7FC, legacy aliases
			end case;
			case (b_wr_a_i_0(8 downto 0)) is 
				when "000000000" => b_rd_d_o_0 <= "00000011011000000010000001100101";
				when "000000001" => b_rd_d_o_0 <= pci_se_size;
				when "000000010" => b_rd_d_o_0 <= snapshot_ext;
				when "000000011" => b_rd_d_o_0 <= snapshot_io_cmd;
				when "000000100" => b_rd_d_o_0 <= pci_io_resp;
				when "000000101" => b_rd_d_o_0 <= pci_io_int;
				when "000000110" => b_rd_d_o_0 <= snapshot_se_cmd;
				when "000000111" => b_rd_d_o_0 <= snapshot_se_addr;
				when "000001000" => b_rd_d_o_0 <= pci_se_resp;
				when "000001001" => b_rd_d_o_0 <= pci_se_rdata_hi;
				when "000001010" => b_rd_d_o_0 <= pci_se_rdata_lo;
				when "000001011" => b_rd_d_o_0 <= snapshot_se_wdata_hi;
				when "000001100" => b_rd_d_o_0 <= snapshot_se_wdata_lo;
				when "100000000" => b_rd_d_o_0 <= x"504E4C31"; -- PNL1
				when "100000001" | "100000010" | "100000011" | "100000100" | "100000101" | "100000110" | "100000111" | "100001000" | "100001001" | "100001010" | "100001011" | "100001100" | "100001101" | "100001110" | "100001111" | "100010000" | "100010001" | "100010010" | "100010011" | "100010100" | "100010101" | "100010110" | "100010111" => b_rd_d_o_0 <= panel_view((to_integer(unsigned(b_wr_a_i_0))-256)*32-1 downto (to_integer(unsigned(b_wr_a_i_0))-257)*32);
				when "111111011" => b_rd_d_o_0 <= M65_BUILD_MAGIC;
				when "111111100" => b_rd_d_o_0 <= M65_BUILD_TIME;
				when "111111101" => b_rd_d_o_0 <= M65_BUILD_DATE;
				when "111111110" => b_rd_d_o_0 <= M65_FPGA_VERSION;
				when others => b_rd_d_o_0 <= M65_INTERFACE_VERSION;
			end case;
            if (b_wr_en_i_0 = '1') then
                -- synthesis translate_off
                assert response_pending = '0' and response_ready = '1'
                  report "BAR write accepted while CDC response busy" severity failure;
                -- synthesis translate_on
				case (b_wr_a_i_0(8 downto 0)) is 
					when "000000001" => pci_se_size <= b_wr_d_i_0; response_pending <= '1';
					when "000000100" => pci_io_resp <= b_wr_d_i_0; response_pending <= '1';
					when "000000101" => pci_io_int <= b_wr_d_i_0; response_pending <= '1';
					when "000001000" => pci_se_resp <= b_wr_d_i_0; response_pending <= '1';
					when "000001001" => pci_se_rdata_hi <= b_wr_d_i_0; response_pending <= '1';
					when "000001010" => pci_se_rdata_lo <= b_wr_d_i_0; response_pending <= '1';
					when others => null;
				end case;
			end if;
		  end if; -- synchronized reset release
		end if;
	end process;

end; -- EP_MEM

