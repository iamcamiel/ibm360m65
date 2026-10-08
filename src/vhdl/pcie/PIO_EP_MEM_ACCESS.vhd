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

entity PIO_EP_MEM_ACCESS is port (

  clk          : in std_logic;
  rst_n        : in std_logic;

  --  Read Port

  rd_addr_i    : in std_logic_vector(10 downto 0);
  rd_be_i      : in std_logic_vector(3 downto 0);
  rd_data_o    : out std_logic_vector(31 downto 0);

  --  Write Port
		
  wr_addr_i    : in std_logic_vector(10 downto 0);
  wr_be_i      : in std_logic_vector(7 downto 0);    
  wr_data_i    : in std_logic_vector(31 downto 0);
  wr_en_i      : in std_logic;
  wr_busy_o    : out std_logic;
  
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

end PIO_EP_MEM_ACCESS;

architecture rtl of PIO_EP_MEM_ACCESS is

type state_type is (PIO_MEM_ACCESS_WR_RST,
                    PIO_MEM_ACCESS_WR_READ,
                    PIO_MEM_ACCESS_WR_WRITE
                    );



signal  rd_data0_q        : std_logic_vector(31 downto 0);
signal  rd_data0_o        : std_logic_vector(31 downto 0);

signal  write_en          : std_logic;
signal  post_wr_data      : std_logic_vector(31 downto 0);
signal  w_pre_wr_data     : std_logic_vector(31 downto 0);
signal  wr_mem_state      : state_type;
signal  pre_wr_data       : std_logic_vector(31 downto 0);

signal  w_pre_wr_data0    : std_logic_vector(31 downto 0);

signal  pre_wr_data0_q : std_logic_vector(31 downto 0);

signal 	rd_data_raw_o     : std_logic_vector(31 downto 0);

-- Memory Write Process

signal  w_pre_wr_data_b3 : std_logic_vector(7 downto 0);
signal  w_pre_wr_data_b2 : std_logic_vector(7 downto 0);
signal  w_pre_wr_data_b1 : std_logic_vector(7 downto 0);
signal  w_pre_wr_data_b0 : std_logic_vector(7 downto 0);


signal  w_wr_data_b3 : std_logic_vector(7 downto 0);
signal  w_wr_data_b2 : std_logic_vector(7 downto 0);
signal  w_wr_data_b1 : std_logic_vector(7 downto 0);
signal  w_wr_data_b0 : std_logic_vector(7 downto 0);

signal w_wr_data0_int : std_logic_vector(7 downto 0);
signal w_wr_data1_int : std_logic_vector(7 downto 0);
signal w_wr_data2_int : std_logic_vector(7 downto 0);
signal w_wr_data3_int : std_logic_vector(7 downto 0);

signal interim, cdc_busy : std_logic;

signal rd_data_raw_int0 : std_logic_vector(7 downto 0);
signal rd_data_raw_int1 : std_logic_vector(7 downto 0);
signal rd_data_raw_int2 : std_logic_vector(7 downto 0);
signal rd_data_raw_int3 : std_logic_vector(7 downto 0);

begin

  w_wr_data_b3 <= wr_data_i(7 downto 0);
  w_wr_data_b2 <= wr_data_i(15 downto 8);
  w_wr_data_b1 <= wr_data_i(23 downto 16);
  w_wr_data_b0 <= wr_data_i(31 downto 24);

  w_pre_wr_data_b3 <= pre_wr_data(31 downto 24);
  w_pre_wr_data_b2 <= pre_wr_data(23 downto 16);
  w_pre_wr_data_b1 <= pre_wr_data(15 downto 08);
  w_pre_wr_data_b0 <= pre_wr_data(07 downto 00);

  w_wr_data3_int <= w_wr_data_b3 when (wr_be_i(3) = '1') else w_pre_wr_data_b3;
  w_wr_data2_int <= w_wr_data_b2 when (wr_be_i(2) = '1') else w_pre_wr_data_b2;
  w_wr_data1_int <= w_wr_data_b1 when (wr_be_i(1) = '1') else w_pre_wr_data_b1;
  w_wr_data0_int <= w_wr_data_b0 when (wr_be_i(0) = '1') else w_pre_wr_data_b0;
	
process(clk, rst_n)
begin

  if ( rst_n = '0' ) then
    
    write_en   <= '0';
    pre_wr_data <= (others => '0');
    post_wr_data <= (others => '0');
    pre_wr_data <= (others => '0');
    pre_wr_data0_q <= (others => '0');
    wr_mem_state <= PIO_MEM_ACCESS_WR_RST;

  else 

    if (clk'event and clk = '1') then

      case ( wr_mem_state ) is

        when PIO_MEM_ACCESS_WR_RST =>

          if (wr_en_i = '1') then -- read state

            -- Pipeline B port data before processing

            pre_wr_data0_q <= w_pre_wr_data0;
            write_en <= '0';
            wr_mem_state <= PIO_MEM_ACCESS_WR_READ ;
            
          else

            write_en <= '0';
            wr_mem_state <= PIO_MEM_ACCESS_WR_RST;

          end if;
				

        when PIO_MEM_ACCESS_WR_READ =>

          -- Now save the selected BRAM B port data out

          pre_wr_data <= w_pre_wr_data;
          write_en <= '0';
          wr_mem_state <= PIO_MEM_ACCESS_WR_WRITE;
           
        when PIO_MEM_ACCESS_WR_WRITE =>

          -- Merge new enabled data and write target BlockRAM location

          post_wr_data <= w_wr_data3_int &
                          w_wr_data2_int &
                          w_wr_data1_int &
                          w_wr_data0_int;
          write_en     <= '1';
          wr_mem_state <= PIO_MEM_ACCESS_WR_RST;

       when others => null;

     end case;

   end if;

end if;

end process;

-- Write controller busy

wr_busy_o <= wr_en_i or interim or write_en or cdc_busy;

interim <= '1' when (wr_mem_state /= PIO_MEM_ACCESS_WR_RST) else '0';

-- Select BlockRAM output based on higher 2 address bits
process(pre_wr_data0_q)

begin

    w_pre_wr_data <= pre_wr_data0_q;

end process;



-- pipeline stage BRAM read data before processing --

process(rst_n, clk)
begin

  if ( rst_n = '0' ) then

    rd_data0_q <= (others => '0');

  else

    if (clk'event and clk='1') then

      rd_data0_q <= rd_data0_o(31 downto 0);

    end if;

  end if;

end process;

process(rd_data0_q(31 downto 0))
begin

  rd_data_raw_o  <= rd_data0_q(31 downto 0);

end process;

-- Handle Read byte enables  --

rd_data_o        <= rd_data_raw_int0 &
                    rd_data_raw_int1 &
                    rd_data_raw_int2 &
                    rd_data_raw_int3 ;

rd_data_raw_int0 <= rd_data_raw_o(7 downto 0) when (rd_be_i(0) = '1') else (others => '0');
rd_data_raw_int1 <= rd_data_raw_o(15 downto 8) when (rd_be_i(1) = '1') else (others => '0');
rd_data_raw_int2 <= rd_data_raw_o(23 downto 16) when (rd_be_i(2) = '1') else (others => '0');
rd_data_raw_int3 <= rd_data_raw_o (31 downto 24) when (rd_be_i(3) = '1') else (others => '0');


EP_MEM_inst : entity work.EP_MEM port map (

  clk_i => clk,
  cdc_busy_o => cdc_busy,

  a_rd_a_i_0 => rd_addr_i(8 downto 0),       -- I [8:0]
  a_rd_d_o_0 => rd_data0_o,                  -- O [31:0]

  b_wr_a_i_0 => wr_addr_i(8 downto 0),       -- I [8:0]
  b_wr_d_i_0 => post_wr_data,                -- I [31:0]
  b_wr_en_i_0 => write_en, --{write_en & (wr_addr_i[10:9] == 2'b00)}), -- I
  b_rd_d_o_0 => w_pre_wr_data0(31 downto 0),        -- O [31:0]
                  
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

end; -- PIO_EP_MEM_ACCESS

