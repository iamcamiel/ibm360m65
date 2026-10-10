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

library work;


entity pci_exp_64b_app is

port  (

  -- Common

  trn_clk                   : in std_logic;
  trn_reset_n               : in std_logic;
  trn_lnk_up_n              : in std_logic;

  -- Tx

  trn_td                    : out std_logic_vector(63 downto 0);
  trn_trem_n                : out std_logic_vector(7 downto 0);
  trn_tsof_n                : out std_logic;
  trn_teof_n                : out std_logic;
  trn_tsrc_rdy_n            : out std_logic;
  trn_tdst_rdy_n            : in std_logic;
  trn_tsrc_dsc_n            : out std_logic;
  trn_terrfwd_n             : out std_logic;
  trn_tdst_dsc_n            : in std_logic;
  trn_tbuf_av               : in std_logic_vector((4 - 1) downto 0);

  -- Rx

  trn_rd                    : in std_logic_vector(63 downto 0);
  trn_rrem_n                : in std_logic_vector(7 downto 0);
  trn_rsof_n                : in std_logic;
  trn_reof_n                : in std_logic;
  trn_rsrc_rdy_n            : in std_logic;
  trn_rsrc_dsc_n            : in std_logic;
  trn_rdst_rdy_n            : out std_logic;
  trn_rerrfwd_n             : in std_logic;
  trn_rnp_ok_n              : out std_logic;
  trn_rbar_hit_n            : in std_logic_vector(6 downto 0);
  trn_rfc_nph_av            : in std_logic_vector(7 downto 0);
  trn_rfc_npd_av            : in std_logic_vector(11 downto 0);
  trn_rfc_ph_av             : in std_logic_vector(7 downto 0);
  trn_rfc_pd_av             : in std_logic_vector(11 downto 0);

  trn_rcpl_streaming_n      : out std_logic; 

  -- Host (CFG) Interface

  cfg_do                    : in std_logic_vector(31 downto 0);
  cfg_di                    : out std_logic_vector(31 downto 0);
  cfg_byte_en_n             : out std_logic_vector(3 downto 0);
  cfg_dwaddr                : out std_logic_vector(9 downto 0);
  cfg_rd_wr_done_n          : in std_logic;
  cfg_wr_en_n               : out std_logic;
  cfg_rd_en_n               : out std_logic;
  cfg_err_cor_n             : out std_logic;
  cfg_err_ur_n              : out std_logic;
  cfg_err_cpl_rdy_n         : in std_logic;
  cfg_err_ecrc_n            : out std_logic;
  cfg_err_cpl_timeout_n     : out std_logic;
  cfg_err_cpl_abort_n       : out std_logic;
  cfg_err_cpl_unexpect_n    : out std_logic;
  cfg_err_posted_n          : out std_logic;
  cfg_interrupt_n           : out std_logic;
  cfg_interrupt_rdy_n       : in std_logic;

  cfg_interrupt_assert_n    : out std_logic;
  cfg_interrupt_di          : out std_logic_vector(7 downto 0);
  cfg_interrupt_do          : in  std_logic_vector(7 downto 0);
  cfg_interrupt_mmenable    : in  std_logic_vector(2 downto 0);
  cfg_interrupt_msienable   : in  std_logic;

  cfg_turnoff_ok_n          : out std_logic;
  cfg_to_turnoff_n          : in std_logic;
  cfg_pm_wake_n             : out std_logic;
  cfg_pcie_link_state_n     : in std_logic_vector(2 downto 0);
  cfg_trn_pending_n         : out std_logic;
  cfg_err_tlp_cpl_header    : out std_logic_vector(47 downto 0);
  cfg_bus_number            : in std_logic_vector(7 downto 0);
  cfg_device_number         : in std_logic_vector(4 downto 0);
  cfg_function_number       : in std_logic_vector(2 downto 0);
  cfg_status                : in std_logic_vector(15 downto 0);
  cfg_command               : in std_logic_vector(15 downto 0);
  cfg_dstatus               : in std_logic_vector(15 downto 0);
  cfg_dcommand              : in std_logic_vector(15 downto 0);
  cfg_lstatus               : in std_logic_vector(15 downto 0);
  cfg_lcommand              : in std_logic_vector(15 downto 0);
  
    core_clk_i, cdc_reset_i : in std_logic;
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
end pci_exp_64b_app;

architecture endpoint_blk_plus_v1_15 of pci_exp_64b_app is



-- Local wires 

signal cfg_completer_id       : std_logic_vector(15 downto 0);
signal cfg_bus_mstr_enable    : std_logic;


begin 

  -- Core input tie-offs

  trn_rnp_ok_n              <= '0';
  trn_rcpl_streaming_n      <= '1'; 
  trn_terrfwd_n             <= '1';

  cfg_err_cor_n             <= '1';
  cfg_err_ur_n              <= '1';
  cfg_err_ecrc_n            <= '1';
  cfg_err_cpl_timeout_n     <= '1';
  cfg_err_cpl_abort_n       <= '1';
  cfg_err_cpl_unexpect_n    <= '1';
  cfg_err_posted_n          <= '0';
  cfg_interrupt_n           <= '1';

  cfg_interrupt_assert_n <= '0';
  cfg_interrupt_di <= X"00";

  cfg_pm_wake_n             <= '1';
  cfg_trn_pending_n         <= '1';
  cfg_dwaddr                <= (others => '0');
  cfg_err_tlp_cpl_header    <= (others => '0');
  cfg_di                    <= (others => '0');
  cfg_byte_en_n             <= X"F"; -- 4-bit bus
  cfg_wr_en_n               <= '1';
  cfg_rd_en_n               <= '1';
  cfg_completer_id          <= (cfg_bus_number &
                                cfg_device_number &
                                cfg_function_number);
  cfg_bus_mstr_enable       <= cfg_command(2);

-- Programmable I/O Module

PIO_interface : entity work.PIO

port map (

  trn_clk  =>  trn_clk,                       -- I
  trn_reset_n  =>  trn_reset_n,               -- I
  trn_lnk_up_n  =>  trn_lnk_up_n,             -- I

  trn_td  => trn_td,                          -- O (63:0)
  trn_tsof_n  => trn_tsof_n,
  trn_trem_n  => trn_trem_n,
  trn_teof_n  => trn_teof_n,                  -- O
  trn_tsrc_rdy_n  => trn_tsrc_rdy_n,          -- O
  trn_tsrc_dsc_n  => trn_tsrc_dsc_n,          -- O
  trn_tdst_rdy_n  => trn_tdst_rdy_n,          -- I
  trn_tdst_dsc_n  => trn_tdst_dsc_n,          -- I

  trn_rd  => trn_rd ,                         -- I (63:0)
  trn_rrem_n  => trn_rrem_n,
  trn_rsof_n  => trn_rsof_n,                  -- I
  trn_reof_n  => trn_reof_n,                  -- I
  trn_rsrc_rdy_n  => trn_rsrc_rdy_n,          -- I
  trn_rsrc_dsc_n  => trn_rsrc_dsc_n,          -- I
  trn_rbar_hit_n => trn_rbar_hit_n,           -- I (6:0)
  trn_rdst_rdy_n  => trn_rdst_rdy_n,          -- O

  cfg_to_turnoff_n  => cfg_to_turnoff_n,      -- I
  cfg_turnoff_ok_n => cfg_turnoff_ok_n,    -- O
  cfg_completer_id  => cfg_completer_id,      -- I (15:0)
  cfg_bus_mstr_enable => cfg_bus_mstr_enable  -- I
,
		
    core_clk_i => core_clk_i, cdc_reset_i => cdc_reset_i,
    panel_snapshot_i => panel_snapshot_i, panel_valid_i => panel_valid_i,
    P_reg_io_int => P_reg_io_int,
	 P_reg_io_resp => P_reg_io_resp,
    P_reg_se_rdata_hi => P_reg_se_rdata_hi,
	 P_reg_se_rdata_lo => P_reg_se_rdata_lo,
	 P_reg_se_resp => P_reg_se_resp,
	 P_reg_se_size => P_reg_se_size,

	 P_reg_ext => P_reg_ext,
	 P_reg_io_cmd => P_reg_io_cmd,
	 P_reg_se_addr => P_reg_se_addr,
	 P_reg_se_cmd => P_reg_se_cmd,
	 P_reg_se_wdata_hi => P_reg_se_wdata_hi,
	 P_reg_se_wdata_lo => P_reg_se_wdata_lo
);

end; -- pci_exp_64b_app
