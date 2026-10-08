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

library IEEE;
use IEEE.STD_LOGIC_1164.ALL;
use IEEE.NUMERIC_STD.ALL;
USE ieee.std_logic_arith.all;

library UNISIM;
use UNISIM.VComponents.all;

entity IBM360 is
    Port ( pci_exp_txp    : out STD_LOGIC;
           pci_exp_txn    : out STD_LOGIC;
           pci_exp_rxp    : in  STD_LOGIC;
           pci_exp_rxn    : in  STD_LOGIC;
           sys_clk_p      : in  STD_LOGIC;
           sys_clk_n      : in  STD_LOGIC;
           sys_reset_n    : in  STD_LOGIC;
			  
			  clk_fpga_n     : in  STD_LOGIC;
			  clk_fpga_p     : in  STD_LOGIC;
			  
			  disp_clk_o     : out STD_LOGIC;
			  disp_latch_n_o : out STD_LOGIC;
			  disp_shift_o   : out STD_LOGIC_VECTOR(0 to 5);
			  disp_shift_i   : in  STD_LOGIC_VECTOR(0 to 7);
			  
			  led_4 : out STD_LOGIC_VECTOR(0 to 3);
			  led_10 : out STD_LOGIC_VECTOR(0 to 9)

           );
end IBM360;

architecture Behavioral of IBM360 is
	signal clk : STD_LOGIC;
	signal clk200 : STD_LOGIC;
	signal core_ready : STD_LOGIC;
	signal panel_rst : STD_LOGIC;
	signal sys_reset_n_c : STD_LOGIC;
	signal dclk : STD_LOGIC;
	signal rst : STD_LOGIC;
	signal hlt : STD_LOGIC;
	
	signal sw0 : STD_LOGIC_VECTOR(0 to 23);
	signal sw1 : STD_LOGIC_VECTOR(0 to 23);
	signal sw2 : STD_LOGIC_VECTOR(0 to 23);
	signal sw3 : STD_LOGIC_VECTOR(0 to 23);
	signal sw4 : STD_LOGIC_VECTOR(0 to 23);
	signal sw5 : STD_LOGIC_VECTOR(0 to 23);
	signal sw6 : STD_LOGIC_VECTOR(0 to 23);
	signal sw7 : STD_LOGIC_VECTOR(0 to 23);
	signal li0 : STD_LOGIC_VECTOR(0 to 39);
	signal li1 : STD_LOGIC_VECTOR(0 to 39);
	signal li2 : STD_LOGIC_VECTOR(0 to 39);
	signal li3 : STD_LOGIC_VECTOR(0 to 39);
	signal li4 : STD_LOGIC_VECTOR(0 to 39);
	signal li5 : STD_LOGIC_VECTOR(0 to 39);
	signal clock_ctr : integer range 0 to 833333;
	signal d_ctr : integer range 0 to 255;
	signal l_ctr : integer range 0 to 10000000;
	signal p60 : STD_LOGIC := '0';
	signal por : STD_LOGIC := '1';

	signal P_reg_io_int : STD_LOGIC_VECTOR (31 downto 0);
	signal P_reg_io_resp : STD_LOGIC_VECTOR (31 downto 0);
	signal P_reg_se_rdata_hi : STD_LOGIC_VECTOR (31 downto 0);
	signal P_reg_se_rdata_lo : STD_LOGIC_VECTOR (31 downto 0);
	signal P_reg_se_resp : STD_LOGIC_VECTOR (31 downto 0);
	signal P_reg_se_size : STD_LOGIC_VECTOR (31 downto 0);
   signal P_reg_ext : STD_LOGIC_VECTOR (31 downto 0);
	signal P_reg_io_cmd : STD_LOGIC_VECTOR (31 downto 0);
	signal P_reg_se_cmd : STD_LOGIC_VECTOR (31 downto 0);
	signal P_reg_se_addr : STD_LOGIC_VECTOR (31 downto 0);
	signal P_reg_se_wdata_hi : STD_LOGIC_VECTOR (31 downto 0);
	signal P_reg_se_wdata_lo : STD_LOGIC_VECTOR (31 downto 0);
	signal l_4 : STD_LOGIC_VECTOR(0 to 3) := "1000";
	signal l_10: STD_LOGIC_VECTOR(0 to 9) := "0000000000";
	signal l_10_d : STD_LOGIC := '0';
begin
  process (clk)
  begin
    if (clk'event and clk = '1') then
		  led_4 <= l_4;
		  led_10 <= (not l_10(0)) & l_10(1 to 9);

	   if (d_ctr = 255) then
		  dclk <= not dclk;
		  d_ctr <= 0;
		else
		  d_ctr <= d_ctr + 1;
		end if;
		
		if (l_ctr = 10000000) then
		  l_4 <= l_4(3) & l_4(0 to 2);
	     l_ctr <= 0;
		else
		  l_ctr <= l_ctr + 1;
		end if;
		
	   if (rst = '1') then
		  clock_ctr <= 0;
		  p60 <= '0';
		  por <= '1';
		else
            -- One ALD update per 10 ns core edge; P60 retains its 60 Hz rate.
            if (clock_ctr = 833333) then
                clock_ctr <= 1;
                p60 <= not p60;
            else
                clock_ctr <= clock_ctr + 1;
            end if;
            if (p60 = '1') then
                por <= '0';
            end if;
		end if;
	 end if;
  end process;
  
  
  ald : entity ALD port map (
    clk => clk,
	 rst => rst,
	 hlt => hlt,
	 P_P60_cycles_from_transformer => p60,
	 P_power_on_reset => por,
    P_reg_io_int => P_reg_io_int,
	 P_reg_io_resp => P_reg_io_resp,
    P_reg_se_rdata_hi => P_reg_se_rdata_hi,
	 P_reg_se_rdata_lo => P_reg_se_rdata_lo,
	 P_reg_se_resp => P_reg_se_resp,
	 P_reg_se_size => P_reg_se_size,
	 P_switches_0 => sw0,
	 P_switches_1 => sw1,
	 P_switches_2 => sw2,
	 P_switches_3 => sw3,
	 P_switches_4 => sw4,
	 P_switches_5 => sw5,
	 P_switches_6 => sw6,
	 P_switches_7 => sw7,
    P_lamps_0 => li0,
    P_lamps_1 => li1,
    P_lamps_2 => li2,
    P_lamps_3 => li3,
    P_lamps_4 => li4,
    P_lamps_5 => li5,

	 P_reg_ext => P_reg_ext,
	 P_reg_io_cmd => P_reg_io_cmd,
	 P_reg_se_addr => P_reg_se_addr,
	 P_reg_se_cmd => P_reg_se_cmd,
	 P_reg_se_wdata_hi => P_reg_se_wdata_hi,
	 P_reg_se_wdata_lo => P_reg_se_wdata_lo
  );
  
  blinken : entity BLINKEN port map (
    clk => dclk,
	 
	 disp_clk_o => disp_clk_o,
	 disp_latch_n_o => disp_latch_n_o,
	 disp_shift_o => disp_shift_o,
	 disp_shift_i => disp_shift_i,
	 
	 sw0 => sw0,
	 sw1 => sw1,
	 sw2 => sw2,
	 sw3 => sw3,
	 sw4 => sw4,
	 sw5 => sw5,
	 sw6 => sw6,
	 sw7 => sw7,
	 
	 li0 => li0,
	 li1 => li1,
	 li2 => li2,
	 li3 => li3,
	 li4 => li4,
	 li5 => li5,
	 
	 configured => P_reg_se_size(0),
	 
	 power_off => panel_rst
  );
  
  pcie : entity XILINX_PCI_EXP_EP port map (
		pci_exp_txp(0) => pci_exp_txp,
		pci_exp_txn(0) => pci_exp_txn,
		pci_exp_rxp(0) => pci_exp_rxp,
		pci_exp_rxn(0) => pci_exp_rxn,
		sys_clk_p => sys_clk_p,
		sys_clk_n => sys_clk_n,
		sys_reset_n => sys_reset_n,
		sys_reset_n_buf_o => sys_reset_n_c,
		
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
	 P_reg_se_wdata_lo => P_reg_se_wdata_lo,

	 trn_reset_n => l_10(0),
	 trn_lnk_up_n => l_10(1)
  );

	fpgaclk_ibuf: ibufds port map (
		 o => clk200,
		 i => clk_fpga_p,
		 ib => clk_fpga_n
	);

  core_clock : entity work.CORE_CLOCK100 port map (
    clk200_i => clk200, reset_i => not sys_reset_n_c,
    clk100_o => clk, ready_o => core_ready
  );
  rst <= panel_rst or not core_ready;

  l_10(2) <= P_reg_se_size(0);	-- host configures
  l_10(3) <= not rst;				-- power on
  l_10(4) <= not rst and not li4(38);			-- not wait state
  l_10(5) <= li4(36);				-- system

  l_10(6) <= (P_reg_se_cmd(30) xor P_reg_se_resp(30)) or (P_reg_se_cmd(31) xor P_reg_se_resp(31));  
  l_10(7) <= (P_reg_io_cmd(30) xor P_reg_io_resp(30)) or (P_reg_io_cmd(31) xor P_reg_io_resp(31));  
end Behavioral;

