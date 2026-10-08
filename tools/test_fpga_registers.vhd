library ieee;
use ieee.std_logic_1164.all;
use ieee.numeric_std.all;
use work.fpga_build.all;

entity test_fpga_registers is end;
architecture test of test_fpga_registers is
  signal clk : std_logic := '0';
  signal done : boolean := false;
  signal a, b : std_logic_vector(8 downto 0) := (others => '0');
  signal qa, qb, data : std_logic_vector(31 downto 0);
  signal we : std_logic := '0';
  signal io_int, io_resp, rhi, rlo, resp, size : std_logic_vector(31 downto 0);
begin
  dut : entity work.EP_MEM port map (
    clk, a, qa, b, data, we, qb, io_int, io_resp, rhi, rlo, resp, size,
    x"12345678", x"23456789", x"34567890", x"45678901", x"56789012", x"67890123");
  process
  begin
    while not done loop
      clk <= '0'; wait for 5 ns;
      clk <= '1'; wait for 5 ns;
    end loop;
    wait;
  end process;
  process
    procedure read_both(word : natural; value : std_logic_vector(31 downto 0)) is
    begin
      a <= std_logic_vector(to_unsigned(word, 9)); b <= std_logic_vector(to_unsigned(word, 9));
      wait until rising_edge(clk); wait for 1 ns;
      assert qa = value and qb = value report "register read failed at word " & integer'image(word) severity failure;
    end;
  begin
    read_both(0, x"03602065");
    read_both(2, x"12345678");
    read_both(16#1fb#, M65_BUILD_MAGIC);
    read_both(16#1fc#, M65_BUILD_TIME);
    read_both(16#1fd#, M65_BUILD_DATE);
    read_both(16#1fe#, M65_FPGA_VERSION);
    read_both(16#1ff#, M65_INTERFACE_VERSION);
    -- Metadata stays read-only, including the write-port readback.
    we <= '1'; data <= x"FFFFFFFF";
    read_both(16#1fb#, M65_BUILD_MAGIC);
    read_both(16#1fc#, M65_BUILD_TIME);
    read_both(16#1fd#, M65_BUILD_DATE);
    read_both(16#1fe#, M65_FPGA_VERSION);
    read_both(16#1ff#, M65_INTERFACE_VERSION);
    -- Existing writable CPU registers and version aliases retain their behavior.
    a <= std_logic_vector(to_unsigned(1, 9)); b <= std_logic_vector(to_unsigned(1, 9));
    data <= x"007FFFFF";
    wait until rising_edge(clk); wait for 1 ns;
    we <= '0';
    read_both(1, x"007FFFFF");
    read_both(13, M65_INTERFACE_VERSION);
    done <= true;
    report "FPGA register reads and write protection passed" severity note;
    wait;
  end process;
end;
