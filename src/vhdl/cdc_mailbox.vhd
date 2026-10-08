library ieee;
use ieee.std_logic_1164.all;

-- One outstanding coherent bus transfer. Both ends share asynchronous reset;
-- each releases reset on its own clock. No destination samples a live source bus.
entity CDC_MAILBOX is
  generic (WIDTH : positive := 32);
  port (source_clk_i, destination_clk_i, reset_i : in std_logic;
        source_data_i : in std_logic_vector(WIDTH-1 downto 0);
        source_valid_i : in std_logic;
        source_ready_o : out std_logic;
        destination_data_o : out std_logic_vector(WIDTH-1 downto 0);
        destination_valid_o : out std_logic);
end CDC_MAILBOX;

architecture RTL of CDC_MAILBOX is
  signal source_hold, destination_data : std_logic_vector(WIDTH-1 downto 0) := (others => '0');
  signal request_toggle, acknowledge_toggle : std_logic := '0';
  signal request_meta, request_sync, ack_meta, ack_sync : std_logic := '0';
  signal source_reset, destination_reset : std_logic_vector(1 downto 0) := "11";
  signal capture_pending, destination_valid : std_logic := '0';
  attribute ASYNC_REG : string;
  attribute SHREG_EXTRACT : string;
  attribute KEEP : string;
  attribute ASYNC_REG of request_meta, request_sync, ack_meta, ack_sync,
    source_reset, destination_reset : signal is "TRUE";
  attribute SHREG_EXTRACT of request_meta, request_sync, ack_meta, ack_sync,
    source_reset, destination_reset : signal is "NO";
  attribute KEEP of source_hold, destination_data : signal is "TRUE";
begin
  process(source_clk_i, reset_i)
  begin
    if reset_i = '1' then source_reset <= "11";
    elsif rising_edge(source_clk_i) then source_reset <= source_reset(0) & '0';
    end if;
  end process;
  process(destination_clk_i, reset_i)
  begin
    if reset_i = '1' then destination_reset <= "11";
    elsif rising_edge(destination_clk_i) then destination_reset <= destination_reset(0) & '0';
    end if;
  end process;

  source_ready_o <= '1' when reset_i = '0' and source_reset(1) = '0'
    and request_toggle = ack_sync else '0';
  destination_data_o <= destination_data;
  destination_valid_o <= destination_valid;

  process(source_clk_i, reset_i)
  begin
    if reset_i = '1' then
      source_hold <= (others => '0'); request_toggle <= '0';
      ack_meta <= '0'; ack_sync <= '0';
    elsif rising_edge(source_clk_i) then
      if source_reset(1) = '1' then
        source_hold <= (others => '0'); request_toggle <= '0';
        ack_meta <= '0'; ack_sync <= '0';
      else
        ack_meta <= acknowledge_toggle; ack_sync <= ack_meta;
        if source_valid_i = '1' and request_toggle = ack_sync then
          source_hold <= source_data_i;
          request_toggle <= not request_toggle;
        end if;
      end if;
    end if;
  end process;

  process(destination_clk_i, reset_i)
  begin
    if reset_i = '1' then
      request_meta <= '0'; request_sync <= '0'; acknowledge_toggle <= '0';
      capture_pending <= '0'; destination_valid <= '0'; destination_data <= (others => '0');
    elsif rising_edge(destination_clk_i) then
      destination_valid <= '0';
      if destination_reset(1) = '1' then
        request_meta <= '0'; request_sync <= '0'; acknowledge_toggle <= '0';
        capture_pending <= '0'; destination_data <= (others => '0');
      else
        request_meta <= request_toggle; request_sync <= request_meta;
        -- Extra destination edge after the two-stage request synchronizer.
        -- UCF bounds source_hold -> destination_data to 8 ns DATAPATHONLY.
        if capture_pending = '1' then
          destination_data <= source_hold;
          destination_valid <= '1'; acknowledge_toggle <= request_sync;
          capture_pending <= '0';
        elsif request_sync /= acknowledge_toggle then
          capture_pending <= '1';
        end if;
      end if;
    end if;
  end process;
end RTL;
