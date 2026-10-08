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
use work.fpga_build.all;

library unisim;
use unisim.vcomponents.all;

entity EP_MEM is port (

  clk_i : in std_logic ;

  a_rd_a_i_0 : in std_logic_vector(8 downto 0);
  a_rd_d_o_0 : out std_logic_vector(31 downto 0);

  b_wr_a_i_0 : in std_logic_vector(8 downto 0);
  b_wr_d_i_0 : in std_logic_vector(31 downto 0);
  b_wr_en_i_0 : in std_logic ;
  b_rd_d_o_0 : out std_logic_vector(31 downto 0);
  
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

begin

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

	process (clk_i)
	begin
		if (rising_edge(clk_i)) then
			case (a_rd_a_i_0(8 downto 0)) is 
				when "000000000" => a_rd_d_o_0 <= "00000011011000000010000001100101"; -- 03602065
				when "000000001" => a_rd_d_o_0 <= P_reg_se_size;
				when "000000010" => a_rd_d_o_0 <= P_reg_ext;
				when "000000011" => a_rd_d_o_0 <= P_reg_io_cmd;
				when "000000100" => a_rd_d_o_0 <= P_reg_io_resp;
				when "000000101" => a_rd_d_o_0 <= P_reg_io_int;
				when "000000110" => a_rd_d_o_0 <= P_reg_se_cmd;
				when "000000111" => a_rd_d_o_0 <= P_reg_se_addr;
				when "000001000" => a_rd_d_o_0 <= P_reg_se_resp;
				when "000001001" => a_rd_d_o_0 <= P_reg_se_rdata_hi;
				when "000001010" => a_rd_d_o_0 <= P_reg_se_rdata_lo;
				when "000001011" => a_rd_d_o_0 <= P_reg_se_wdata_hi;
				when "000001100" => a_rd_d_o_0 <= P_reg_se_wdata_lo;
				when "111111011" => a_rd_d_o_0 <= M65_BUILD_MAGIC; -- BAR0 0x7EC
				when "111111100" => a_rd_d_o_0 <= M65_BUILD_TIME; -- BAR0 0x7F0
				when "111111101" => a_rd_d_o_0 <= M65_BUILD_DATE; -- BAR0 0x7F4
				when "111111110" => a_rd_d_o_0 <= M65_FPGA_VERSION; -- BAR0 0x7F8
				when others => a_rd_d_o_0 <= M65_INTERFACE_VERSION; -- BAR0 0x7FC, legacy aliases
			end case;
			case (b_wr_a_i_0(8 downto 0)) is 
				when "000000000" => b_rd_d_o_0 <= "00000011011000000010000001100101";
				when "000000001" => b_rd_d_o_0 <= P_reg_se_size;
				when "000000010" => b_rd_d_o_0 <= P_reg_ext;
				when "000000011" => b_rd_d_o_0 <= P_reg_io_cmd;
				when "000000100" => b_rd_d_o_0 <= P_reg_io_resp;
				when "000000101" => b_rd_d_o_0 <= P_reg_io_int;
				when "000000110" => b_rd_d_o_0 <= P_reg_se_cmd;
				when "000000111" => b_rd_d_o_0 <= P_reg_se_addr;
				when "000001000" => b_rd_d_o_0 <= P_reg_se_resp;
				when "000001001" => b_rd_d_o_0 <= P_reg_se_rdata_hi;
				when "000001010" => b_rd_d_o_0 <= P_reg_se_rdata_lo;
				when "000001011" => b_rd_d_o_0 <= P_reg_se_wdata_hi;
				when "000001100" => b_rd_d_o_0 <= P_reg_se_wdata_lo;
				when "111111011" => b_rd_d_o_0 <= M65_BUILD_MAGIC;
				when "111111100" => b_rd_d_o_0 <= M65_BUILD_TIME;
				when "111111101" => b_rd_d_o_0 <= M65_BUILD_DATE;
				when "111111110" => b_rd_d_o_0 <= M65_FPGA_VERSION;
				when others => b_rd_d_o_0 <= M65_INTERFACE_VERSION;
			end case;
			if (b_wr_en_i_0 = '1') then
				case (b_wr_a_i_0(8 downto 0)) is 
					when "000000001" => P_reg_se_size <= b_wr_d_i_0;
					when "000000100" => P_reg_io_resp <= b_wr_d_i_0;
					when "000000101" => P_reg_io_int <= b_wr_d_i_0;
					when "000001000" => P_reg_se_resp <= b_wr_d_i_0;
					when "000001001" => P_reg_se_rdata_hi <= b_wr_d_i_0;
					when "000001010" => P_reg_se_rdata_lo <= b_wr_d_i_0;
					when others => null;
				end case;
			end if;
		end if;
	end process;

end; -- EP_MEM

