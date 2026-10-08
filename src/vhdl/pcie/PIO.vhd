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


entity PIO is

port (
	
  trn_clk                : in std_logic;         
  trn_reset_n            : in std_logic;
  trn_lnk_up_n           : in std_logic;

   
  trn_td                 : out std_logic_vector((64 - 1) downto 0);
  trn_trem_n             : out std_logic_vector(7 downto 0);

  trn_tsof_n             : out std_logic;
  trn_teof_n             : out std_logic;
  trn_tsrc_rdy_n         : out std_logic;
  trn_tsrc_dsc_n         : out std_logic;
  trn_tdst_rdy_n         : in std_logic;
  trn_tdst_dsc_n         : in std_logic;

    
  trn_rd                 : in std_logic_vector(( 64 - 1) downto 0);
  trn_rrem_n             : in std_logic_vector(7 downto 0);

  trn_rsof_n             : in std_logic;
  trn_reof_n             : in std_logic;
  trn_rsrc_rdy_n         : in std_logic;
  trn_rsrc_dsc_n         : in std_logic;
  trn_rbar_hit_n         : in std_logic_vector(6 downto 0);
  trn_rdst_rdy_n         : out std_logic;
  cfg_to_turnoff_n       : in std_logic;
  cfg_turnoff_ok_n       : out std_logic;

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

end PIO;

architecture rtl of PIO is	 

-- Local wires

signal req_compl      : std_logic;
signal compl_done     : std_logic;
signal pio_reset_n    : std_logic;
signal pio_reset_sync : std_logic_vector(1 downto 0) := "00";
attribute ASYNC_REG : string;
attribute SHREG_EXTRACT : string;
attribute ASYNC_REG of pio_reset_sync : signal is "TRUE";
attribute SHREG_EXTRACT of pio_reset_sync : signal is "NO";








begin

-- Hold RX/TX and register writes in reset on either clock/link reset;
-- release only on stable PCIe transaction-clock edges.
process(trn_clk, cdc_reset_i)
begin
  if cdc_reset_i = '1' then pio_reset_sync <= "00";
  elsif rising_edge(trn_clk) then pio_reset_sync <= pio_reset_sync(0) & '1';
  end if;
end process;
pio_reset_n <= pio_reset_sync(1);

-- PIO instance

PIO_EP_ins : entity work.PIO_EP

port map (

  clk => trn_clk,                            -- I
  rst_n => pio_reset_n,                      -- I

  trn_td => trn_td,                          -- O [63/31:0]
  
  trn_trem_n => trn_trem_n,                  -- O
  trn_tsof_n => trn_tsof_n,                  -- O
  trn_teof_n => trn_teof_n,                  -- O
  trn_tsrc_rdy_n => trn_tsrc_rdy_n,          -- O
  trn_tsrc_dsc_n => trn_tsrc_dsc_n,          -- O
  trn_tdst_rdy_n => trn_tdst_rdy_n,          -- I
  trn_tdst_dsc_n => trn_tdst_dsc_n,          -- I

  trn_rd => trn_rd,                          -- I [63/31:0]

  
  trn_rrem_n => trn_rrem_n,                  -- I
  trn_rsof_n => trn_rsof_n,                  -- I
  trn_reof_n => trn_reof_n,                  -- I
  trn_rsrc_rdy_n => trn_rsrc_rdy_n,          -- I
  trn_rsrc_dsc_n => trn_rsrc_dsc_n,          -- I
  trn_rbar_hit_n => trn_rbar_hit_n,     -- I
  trn_rdst_rdy_n => trn_rdst_rdy_n,          -- O

  req_compl_o => req_compl,                  -- O
  compl_done_o => compl_done,                -- O

  cfg_completer_id => cfg_completer_id,      -- I [15:0]
  cfg_bus_mstr_enable => cfg_bus_mstr_enable -- I
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


    --
    -- Turn-Off controller
    --

PIO_TO : entity work.PIO_TO_CTRL port map   (

   clk => trn_clk,                             -- I
   rst_n => trn_reset_n,                       -- I

   req_compl_i => req_compl,                   -- I
   compl_done_i => compl_done,                 -- I

   cfg_to_turnoff_n => cfg_to_turnoff_n,       -- I
   cfg_turnoff_ok_n => cfg_turnoff_ok_n        -- O

);

end;  -- PIO
