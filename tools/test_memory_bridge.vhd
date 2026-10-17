library ieee;
use ieee.std_logic_1164.all;
use ieee.numeric_std.all;
use std.env.all;

entity test_memory_bridge is
  generic (FLAGS : natural := 0; LATENCY : natural := 50;
           HELD_SELECT : boolean := false; HALT_REPLY : boolean := false;
           RESET_PENDING : boolean := false);
end entity;

architecture test of test_memory_bridge is
  signal clk : std_logic := '0';
  signal rst : std_logic := '1';
  signal hlt : std_logic := '0';
  signal select_n : std_logic := '1';
  signal mode : std_logic_vector(4 downto 0) := (others => '0');
  signal sab : std_logic_vector(0 to 23) := (others => '0');
  signal marks : std_logic_vector(0 to 7) := (others => '0');
  signal in_key : std_logic_vector(0 to 4) := (others => '0');
  signal wdata, rdata, sdbo : std_logic_vector(0 to 63) := (others => '0');
  signal response, command, address, write_hi, write_lo : std_logic_vector(31 downto 0) := (others => '0');
  signal accept_n, advance, key_advance, key_parity : std_logic;
  signal out_key : std_logic_vector(0 to 4);
  signal parity : std_logic_vector(0 to 7);

  function odd_parity(v : std_logic_vector) return std_logic is
    variable p : std_logic := '1';
  begin
    for i in v'range loop p := p xor v(i); end loop;
    return p;
  end;
begin
  clk <= not clk after 5 ns;
  -- PORT_MAP is filled from the actual generated entity by the runner.
  PORT_MAP

  stimulus : process
    variable seq : unsigned(1 downto 0) := "00";
    variable expected_address, expected_command : std_logic_vector(31 downto 0);
    variable expected_write, expected_read : std_logic_vector(0 to 63);
    variable old_accept, old_advance : std_logic := '0';
    variable accepts, advances, accept_ticks, advance_ticks, keys : natural := 0;
    variable published, replied : boolean := false;

    procedure tick is
    begin
      wait until rising_edge(clk); wait for 1 ns;
      if rst = '0' and hlt = '0' then
        if accept_n = '0' then
          accept_ticks := accept_ticks + 1;
          if old_accept = '0' then accepts := accepts + 1; end if;
          old_accept := '1';
        else old_accept := '0'; end if;
        if advance = '1' then
          advance_ticks := advance_ticks + 1;
          if old_advance = '0' then advances := advances + 1; end if;
        end if;
        old_advance := advance;
        if key_advance = '1' then
          assert replied and mode(2) = '1' report "premature/unrequested key advance" severity failure;
          assert out_key = "11010" and key_parity = odd_parity(out_key)
            report "wrong storage key or key parity" severity failure;
          keys := keys + 1;
        end if;
        if published then
          assert command = expected_command and address = expected_address
            report "request/payload changed after publication" severity failure;
          if FLAGS mod 2 = 1 then
            assert write_hi & write_lo = expected_write report "store data not held" severity failure;
          end if;
        end if;
      end if;
    end;
    procedure ticks(n : natural) is
    begin for i in 1 to n loop tick; end loop; end;
  begin
    ticks(8); rst <= '0'; ticks(100);
    assert accept_ticks = 0 and advance_ticks = 0 and command(31 downto 30) = "00"
      report "phantom transaction after reset" severity failure;
    for transaction in 0 to 11 loop
      published := false; replied := false;
      accepts := 0; advances := 0; accept_ticks := 0; advance_ticks := 0; keys := 0;
      old_accept := '0'; old_advance := '0';
      mode <= std_logic_vector(to_unsigned(FLAGS, 5));
      sab <= std_logic_vector(to_unsigned(16#123458# + transaction * 8, 24));
      marks <= std_logic_vector(to_unsigned((16#A5# + transaction) mod 256, 8));
      in_key <= "10101";
      expected_write := std_logic_vector(unsigned'(x"0123456789ABCDEF") + transaction);
      expected_read := std_logic_vector(unsigned'(x"FEDCBA9876543210") + transaction);
      wdata <= expected_write;
      ticks(3); select_n <= '0';
      expected_address := std_logic_vector(to_unsigned((16#A5# + transaction) mod 256, 8)) & std_logic_vector(to_unsigned(16#123458# + transaction * 8, 24));
      expected_command := std_logic_vector(seq + 1) & "10101" & std_logic_vector(to_unsigned(FLAGS, 25));
      for watchdog in 1 to 100 loop
        tick;
        exit when command(31 downto 30) /= std_logic_vector(seq);
      end loop;
      assert command(31 downto 30) = std_logic_vector(seq + 1)
        report "missing or repeated sequence increment" severity failure;
      seq := seq + 1;
      assert command = expected_command and address = expected_address report "incorrect request flags/address/key/marks" severity failure;
      published := true;
      if not HELD_SELECT then select_n <= '1'; end if;
      -- Mutating source pins must not change a published mailbox transaction.
      sab <= (others => '1'); marks <= (others => '0'); in_key <= (others => '0'); wdata <= (others => '1');
      if RESET_PENDING and transaction = 3 then
        published := false; rst <= '1'; select_n <= '1'; response <= (others => '0');
        ticks(5); rst <= '0'; seq := "00"; ticks(100);
        assert command(31 downto 30) = "00" and accept_n = '1' and advance = '0'
          report "reset did not clear pending transaction" severity failure;
      else
        response <= std_logic_vector(seq - 1) & "11010" & '1' & x"000000";
        ticks(LATENCY);
        if FLAGS mod 2 = 0 then
          assert accepts = 0 and advances = 0 report "read accepted before matching response" severity failure;
        end if;
        if HALT_REPLY then
          hlt <= '1'; ticks(3);
          response <= std_logic_vector(seq) & "11010" & '1' & x"000000";
          rdata <= expected_read; ticks(7); hlt <= '0';
        else
          response <= std_logic_vector(seq) & "11010" & '1' & x"000000";
          rdata <= expected_read;
        end if;
        replied := true;
        ticks(150);
        assert accepts = 1 and advances = 1 report "completion absent or repeated" severity failure;
        assert accept_ticks = 20 and advance_ticks = 20 report "completion pulse width changed" severity failure;
        if FLAGS mod 2 = 0 then
          assert sdbo = expected_read report "wrong returned read data" severity failure;
          for byte in 0 to 7 loop
            assert parity(byte) = odd_parity(expected_read(byte*8 to byte*8+7))
              report "wrong returned data parity" severity failure;
          end loop;
        end if;
        if (FLAGS / 4) mod 2 = 1 then assert keys > 0 report "missing matched key advance" severity failure; end if;
        if HELD_SELECT then ticks(150); end if;
      end if;
      published := false; select_n <= '1'; ticks(20);
    end loop;
    report "MEMORY_BRIDGE_PASS transactions=12";
    stop; wait;
  end process;
end architecture;
