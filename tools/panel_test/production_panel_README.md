Standalone production panel-driver experiment.

BLINKEN is copied byte-for-byte from src/vhdl/blinken.vhd into an immutable
trial, with the production 100 MHz core, 512-cycle enable divider, reset release,
97.65625 kHz serial clock and 5.12 us latch pulse. All eight switch chains remain
connected and observable through registered switch parity on a board LED.

The wrapper sets configured=1, supplies six checkerboard lamp banks, and changes
their phase once per second between serial frames. The driver's normal power
control remains in effect: press Power On to show the checkerboard; Power Off
returns to the steady red Off indication. The configured=0 waiting-blink mode
and PCIe snapshot mailbox are not exercised in this image. No CPU or PCIe is
included, and no configuration flash is written.

Board LEDs show clock-ready, panel-power-on, parity of all 192 scanned switch
bits, and checkerboard phase. The parity observation prevents removal of otherwise
unused switch-bank registers; it is not a switch-state display.

GHDL and ISim fixtures model eight 597 input chains and six 595 output chains,
verify every scanned switch bit, exact serial/latch/frame periods, power-on/off,
coherent alternating checkerboards, and reset recovery. The production driver's
existing display clock fixture also runs unchanged. Routed timing and pin checks
remain necessary. External cable, level shifting, supplies and optical stability
are not established by simulation. Keep every earlier checkerboard and CPU image.

The wrapper supports SCAN_ENABLE_CYCLES, default 512. A separate half-rate trial
uses 1024 (48.828125 kHz SCK, 10.24 us pulse widths, 1162.5744 scans/s), with the
same 100 MHz core and one-second checkerboard alternation. BLINKEN is unchanged.
The original production-driver hardware test started without a button press,
ignored Power Off, briefly blanked on Power On, and left P24-31 dark on all
roller bars. The older CPU image's always-lit roller row 22 is a separate
observation; P24-31 lies two columns to its right. These remain physical
anomalies, despite successful build/timing and ideal-input simulation checks.

Halving the clock left these hardware symptoms unchanged, according to the user.
The next isolated diagnostic keeps that half-rate scan and all input registers,
but adds DIAGNOSTIC_FORCE_LEDS to its private BLINKEN copy. It bypasses only the
power-off LED mux; the power latch still runs and drives the board power LED.
The canonical production BLINKEN source and all earlier trial inputs remain
unchanged. The forced test checks all 240 output bits during off-button readings
and bank-0 stuck-low serial input, after two complete frames following reset.
