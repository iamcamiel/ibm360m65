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
use ieee.std_logic_arith.all;
use ieee.numeric_std.all;

entity PIO_EP is

port (

  clk                    : in std_logic;
  rst_n                  : in std_logic;

  -- LocalLink Tx
    
  trn_td                 : out std_logic_vector(( 64 - 1) downto 0);
  trn_trem_n             : out std_logic_vector(7 downto 0);

  trn_tsof_n             : out std_logic;
  trn_teof_n             : out std_logic;
  trn_tsrc_dsc_n         : out std_logic;
  trn_tsrc_rdy_n         : out std_logic;
  trn_tdst_dsc_n         : in std_logic;
  trn_tdst_rdy_n         : in std_logic;
    
  -- LocalLink Rx

  trn_rd                 : in std_logic_vector(( 64 - 1) downto 0);
  trn_rrem_n             : in std_logic_vector(7 downto 0);
  trn_rsof_n             : in std_logic;
  trn_reof_n             : in std_logic;
  trn_rsrc_rdy_n         : in std_logic;
  trn_rsrc_dsc_n         : in std_logic;
  trn_rbar_hit_n         : in std_logic_vector(6 downto 0);
  trn_rdst_rdy_n         : out std_logic;
  
  req_compl_o            : out std_logic;
  compl_done_o           : out std_logic;

  cfg_completer_id       : in std_logic_vector(15 downto 0);
  cfg_bus_mstr_enable    : in std_logic;

    core_clk_i, cdc_reset_i : in std_logic;
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
end PIO_EP;
    
architecture rtl of PIO_EP is
 
-- Local signals
    
  signal rd_addr       : std_logic_vector(10 downto 0); 
  signal rd_be         : std_logic_vector(3 downto 0); 
  signal rd_data       : std_logic_vector(31 downto 0); 

  signal wr_addr       : std_logic_vector(10 downto 0); 
  signal wr_be         : std_logic_vector(7 downto 0); 
  signal wr_data       : std_logic_vector(31 downto 0); 
  signal wr_en         : std_logic;
  signal wr_busy       : std_logic;

  signal req_compl     : std_logic;
  signal req_compl_with_data  : std_logic;
  signal compl_done    : std_logic;

  signal req_tc        : std_logic_vector(2 downto 0);
  signal req_td        : std_logic; 
  signal req_ep        : std_logic; 
  signal req_attr      : std_logic_vector(1 downto 0);
  signal req_len       : std_logic_vector(9 downto 0);
  signal req_rid       : std_logic_vector(15 downto 0);
  signal req_tag       : std_logic_vector(7 downto 0);
  signal req_be        : std_logic_vector(7 downto 0);
  signal req_addr      : std_logic_vector(12 downto 0);







signal trn_td_int        : std_logic_vector(( 64 - 1) downto 0);
signal trn_trem_n_int    : std_logic_vector(7 downto 0);


begin

trn_trem_n    <= trn_trem_n_int;
trn_td        <= trn_td_int;

-- ENDPOINT MEMORY : 8KB memory aperture implemented in FPGA BlockRAM(*)  

EP_MEM : entity work.PIO_EP_MEM_ACCESS port map (

  clk => clk,                           -- I
  rst_n => rst_n,                       -- I

  -- Read Port

  rd_addr_i => rd_addr,                 -- I [10:0]
  rd_be_i => rd_be,                     -- I [3:0]
  rd_data_o => rd_data,                 -- O [31:0]

  -- Write Port

  wr_addr_i => wr_addr,                 -- I [10:0]
  wr_be_i => wr_be,                     -- I [7:0]
  wr_data_i => wr_data,                 -- I [31:0]
  wr_en_i => wr_en,                     -- I
  wr_busy_o => wr_busy                  -- O
,
		
    core_clk_i => core_clk_i, cdc_reset_i => cdc_reset_i,
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

EP_RX_64 : entity work.PIO_64_RX_ENGINE port map (

  clk => clk,                           -- I
  rst_n => rst_n,                       -- I

  -- LocalLink Rx
  trn_rd => trn_rd,                     -- I [63/31:0]
  trn_rrem_n => trn_rrem_n,             -- I [7:0]
  trn_rsof_n => trn_rsof_n,             -- I
  trn_reof_n => trn_reof_n,             -- I
  trn_rsrc_rdy_n => trn_rsrc_rdy_n,     -- I
  trn_rsrc_dsc_n => trn_rsrc_dsc_n,     -- I
  trn_rbar_hit_n => trn_rbar_hit_n,     -- I [6:0]
  trn_rdst_rdy_n => trn_rdst_rdy_n,     -- O

  -- Handshake with Tx engine 

  req_compl_o => req_compl,             -- O
  req_compl_with_data_o => req_compl_with_data,  -- O
  compl_done_i => compl_done,           -- I

  req_tc_o => req_tc,                   -- O [2:0]
  req_td_o => req_td,                   -- O
  req_ep_o => req_ep,                   -- O
  req_attr_o => req_attr,               -- O [1:0]
  req_len_o => req_len,                 -- O [9:0]
  req_rid_o => req_rid,                 -- O [15:0]
  req_tag_o => req_tag,                 -- O [7:0]
  req_be_o => req_be,                   -- O [7:0]
  req_addr_o => req_addr,               -- O [12:0]

  -- Memory Write Port

  wr_addr_o => wr_addr,                 -- O [10:0]
  wr_be_o => wr_be,                     -- O [7:0]
  wr_data_o => wr_data,                 -- O [31:0]
  wr_en_o => wr_en,                     -- O
  wr_busy_i => wr_busy                  -- I
                   
);

-- Local-Link Transmit Controller

EP_TX_64 : entity work.PIO_64_TX_ENGINE  port map (

  clk => clk,                         -- I
  rst_n => rst_n,                     -- I

  -- LocalLink Tx
  trn_td => trn_td_int,               -- O [63/31:0]
  trn_trem_n => trn_trem_n_int,       -- O [7:0]
  trn_tsof_n => trn_tsof_n,           -- O
  trn_teof_n => trn_teof_n,           -- O
  trn_tsrc_dsc_n => trn_tsrc_dsc_n,   -- O
  trn_tsrc_rdy_n => trn_tsrc_rdy_n,   -- O
  trn_tdst_dsc_n => trn_tdst_dsc_n,   -- I
  trn_tdst_rdy_n => trn_tdst_rdy_n,   -- I

  -- Handshake with Rx engine 
  req_compl_i => req_compl,           -- I
  req_compl_with_data_i => req_compl_with_data, -- I
  compl_done_o => compl_done,         -- 0

  req_tc_i => req_tc,                 -- I [2:0]
  req_td_i => req_td,                 -- I
  req_ep_i => req_ep,                 -- I
  req_attr_i => req_attr,             -- I [1:0]
  req_len_i => req_len,               -- I [9:0]
  req_rid_i => req_rid,               -- I [15:0]
  req_tag_i => req_tag,               -- I [7:0]
  req_be_i => req_be,                 -- I [7:0]
  req_addr_i => req_addr,             -- I [12:0]
                    
  -- Read Port

  rd_addr_o => rd_addr,              -- O [10:0]
  rd_be_o => rd_be,                  -- O [3:0]
  rd_data_i => rd_data,              -- I [31:0]

  completer_id_i => cfg_completer_id,          -- I [15:0]
  cfg_bus_mstr_enable_i => cfg_bus_mstr_enable -- I

);

  req_compl_o     <= req_compl;
  compl_done_o    <= compl_done;

end; -- PIO_EP

