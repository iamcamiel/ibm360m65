Standalone panel checkerboard diagnostic for the XUPV5/ML505 board.

The image contains a clock PLL and serial panel driver, with no CPU or PCIe
endpoint. It runs independently of Hercules. The existing CPU image remains
available for restoration; programming this test into FPGA RAM does not replace
the configuration flash.

The current top generates a 100 kHz SCK from the 100 MHz core: 5 us high and low.
A frame retains the production driver's 40 serial clocks, idle slot, and low latch
pulse. The 0.42 ms frame period gives 2380.95 refreshes/second. The latch low pulse
is 5 us, close to the production driver's 5.12 us at 97.65625 kHz. The immutable 1 kHz and
10 kHz trials remain in gen/ise-panel-checker-slow-20261009 and
gen/ise-panel-checker-10khz-20261009; the 50 kHz trial remains in
gen/ise-panel-checker-50khz-20261009. The user observed stable patterns at all three rates.

All six 40-bit banks show alternating bits, with adjacent banks inverted. A
one-second timer reverses the pattern; each reversal is applied at the next
frame boundary, up to 0.42 ms later. Data is fixed during each frame. Each LED is
commanded on half of the time. The four board LEDs show clock ready, active
checkerboard phase, SCK and latch respectively.

This tests the panel output link independently of the CPU. It does not sample switches,
prove electrical signal integrity, or isolate clock rate from latch pulse width.
The bank arrangement is a serial checkerboard; the physical lamp layout may differ.

Simulation verifies coherent checkerboards, 40 clocks per frame, clock/latch
timing, pattern reversal and recovery from reset during a frame.
The production-divider test separately verifies 100 kHz serial clocks, 5 us
high/latch pulses and the 0.42 ms refresh period.
