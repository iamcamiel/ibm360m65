# FPGA diagnostic recorder

FPGA1.38 / PCIe interface1.7 extends the recorder with ROSAR breakpoints and
single stepping in the 100 MHz core domain.
It starts **disarmed, running, with automatic halt disabled**. Installing this
revision requires its matching host and a separately verified routed build.
Nothing in the reader performs IPL, reset, FPGA programming or guest commands.

## Captures and halt

An explicit ARM starts a new history and discards the preceding frozen capture.
It takes the current check levels as its baseline. The mask selects current
PROC, current STOR, first-pass PROC and first-pass STOR, in bits0..3. Either a
rising or falling selected check transition freezes two 32-word state bundles:
the preceding core-edge observation and the observation at detection. Signals
are sampled **before the rising core edge**, after the preceding finite Boolean
evaluation has settled. Both pass summaries are exported independently; a
first-pass-only indication is eligible. This is not a separate sample between
the two evaluations of one edge, nor a complete CPU-state dump.

Trigger qualification requires configured storage and CPU reset inactive on both
observations. Reset/power transitions are recorded as state but do not trigger
an automatic capture. Panel Power Off does not erase an existing capture. The
recorder resets only when core/PCIe readiness resets, or rearms explicitly.

The trigger follows indicator summaries, including lamp-test effects. Raw check
register summaries, lamp-test/reset controls and delayed error request are also
captured. An indicator transition does **not** establish a fatal fault or an
accepted SOROS/forced019 transfer.

Optional halt-on-event asserts the existing ALD `hlt` clock-enable control.
The detection edge can still execute; subsequent ALD CLOCK, sampled Boolean
state, SPECIAL, local-store and ROS-memory updates stop. The pre/event bundles
remain the evidence for the detection edge. P60/POR generation also pauses.
This is a synchronous enable stop, not physical gating of the 100 MHz clock.
PCIe, its command/response mailboxes, the recorder and panel scanner continue.
Host memory/I/O replies can arrive while the CPU is halted; the CPU samples
them after explicit release. Halt is intrusive and can affect external timing.

Manual HALT and RELEASE are separate explicit commands. ARM, DISARM, reads and
manual CAPTURE never release a halt. Manual CAPTURE freezes without halting.
The first capture stays frozen until ARM. A command coinciding with an automatic
event is rejected so it cannot overwrite or release that event.

## ROSAR breakpoints and stepping (R137)

The breakpoint observes the internal 12-bit ROSAR from the CPU snapshot, before
KT panel display overrides. It is disabled after reset. Configure an exact
address and a one-based hit target (1..16777215), then ARM explicitly. The hit
counter resets on ARM. A hit uses one of two selectable boundaries:

- `p0` (default): a positive P0 rising observation while ROSAR matches. A held
  matching address is counted once per observed P0 edge, not every100MHz edge.
- `entry`: a matching ROSAR observation immediately after a different observed
  address. This includes observable intermediate addresses such as000; it is
  not a count of decoded/executed microinstructions.

Both require configured storage, reset inactive on consecutive observations and
no diagnostic halt. ARM takes the current state as its baseline, so it does not
invent an entry or P0 edge at the moment of arming. The Nth matching hit freezes
the before/event bundles and history, optionally halting under halt-on-event.
Selected check transitions remain eligible and may capture earlier. Simultaneous
check/breakpoint/step reasons are retained as independent event-reason bits.
Configuration writes require a disarmed recorder; they do not clear evidence,
arm, or release halt. Frozen capture metadata retains the configuration and hit
count at detection even if the live configuration is subsequently changed.

A48 is visited normally in preserved native LOAD fixtures. A later P0 hit can
inspect its serial-adder byte and branch controls; a ring around A49 can inspect
countdown exit. Neither a breakpoint nor a panel display alone proves that the
physical countdown completed. The32-word snapshots omit some internal inputs;
only ROS/flags/timestamps are retained throughout the history ring.

Both step modes require **halted, armed, not frozen**. Preserve/dump a prior
capture, explicitly ARM while still halted, then issue exactly one STEP:

- `ros-change`: run until the first observed ROSAR differs from its starting
  value, including intermediate000 if present. P0 changes alone do not stop it.
- `p0`: run until the next observed positive P0 rising edge, even with the same
  ROSAR. Starting with P0 high waits for a fall followed by the next rise. From
  an arbitrary phase this is a boundary step, potentially less than a full cycle.

Completion always freezes and halts, independently of halt-on-event. A check or
breakpoint encountered earlier also freezes and halts an active step. The
recorder samples before the core edge, so the detection edge can still execute;
this is not an exact stop before executing the target microinstruction. Step
commands do not alter the100MHz clock or CPU equations. PCIe and panel scanning
continue, as with manual HALT. If the chosen boundary never occurs, the step
remains active; explicit HALT cancels it. There is no automatic retry/recovery.
While stepping, ARM/CAPTURE/RELEASE/DISARM and another STEP are rejected. A
cancelled step does not manufacture a frozen capture; CAPTURE remains explicit.

## ROS history

The cyclic RAM retains the last256 entries. It records on ROSAR change **or** a
positive P0 edge, so a self-loop at the same ROS address remains visible. A
trigger also forces an entry. Each entry contains a 64-bit 100 MHz core-edge
timestamp and the state flags/12-bit ROSAR word. Timestamps continue during halt.
History stops with the capture and reads oldest first. The readout uses a
power-of-two four-DWORD stride (last word reserved), avoiding unsupported
non-power-of-two division in ISE synthesis. Initial histories can
contain fewer than256 entries; the count is explicit. This is core-edge history,
not a log of every transient combinational ROS address.

## BAR0 protocol

All offsets below are **bytes**; existing service, panel and version registers
retain their addresses. Each diagnostic command uses an independent coherent
CDC round trip, with one outstanding request. Automatic capture does not use
the service mailbox or stall CPU memory/I/O. Both new mailbox bundles retain
held-payload acknowledgement and have explicit8ns DATAPATHONLY constraints;
their placement and routed timing must be verified in the new build.

| Offset | Read | Write |
|---|---|---|
| `0x600` | `0x44424731` (`DBG1`) | Full DWORD guarded command |
| `0x604` | Issued sequence number | Ignored |
| `0x608` | Completed sequence number | Ignored |
| `0x60C` | Result for completed sequence | Ignored |
| `0x610` | Bit0: command pending | Ignored |

Command is `0xA0000000 | (opcode << 24) | argument`. Use complete DWORD writes;
the normal PIO path merges partial writes, and the diagnostic write-port
readback is zero. Writes with a different high nibble are ignored. Wait until
pending is clear, read issued sequence, write once, and wait for completed to
equal issued+1 modulo2^32 before reading the result. Never retry a timed-out
control command automatically.

| Opcode | Operation | Argument/result |
|---|---|---|
| 0 | Status | Armed/frozen/halted/halt-on-event bits0..3; mask4..7; changed8..11; history count12..20; stepping21; last step mode22 (1=P0) |
| 1 | ARM | Mask bits0..3, halt-on-event bit4; other bits must be zero |
| 2 | CAPTURE | Freeze before/current state and history without halt |
| 3 | HALT | Assert diagnostic halt |
| 4 | RELEASE | Release diagnostic halt, retain evidence |
| 5 | Snapshot word | 0..31: before;32..63: at event; frozen required |
| 6 | History word | 4 DWORD stride/entry: ROS+flags, timestamp low, timestamp high, reserved zero; frozen required |
| 7 | Metadata | 0:generation;1/2:event timestamp;3:count;4:write head;5:schema1/depth256;6:event reason bits0=check,1=breakpoint,2=manual,3=ROS step,4=P0 step;7:event hits;8:event breakpoint config;9:event hit target;10:step starting ROSAR |
| 8 | DISARM | Stop recording, retain capture/halt; rejected during step |
| 9 | Configure breakpoint | Address0..11; enable12; P0 mode13 (0=entry); other bits zero; disarmed required |
| 10 | Configure hit target | One-based1..16777215; disarmed required |
| 11 | Live breakpoint read | Index0:config;1:hit target;2:hits; no frozen requirement |
| 12 | STEP | Argument0=ROSAR change,1=next P0 rising edge; halted+armed+not frozen required; captures and halts at boundary |

Results `E0000001/2/3` mean invalid argument, unavailable/frozen-state conflict,
or event-edge priority rejection. State/history DWORDs can legitimately equal
these patterns, so do not treat arbitrary payload words as error messages.
Frozen capture generation must remain unchanged through a dump.

## Snapshot schema1

All words are32bits. IBM bit0 remains the most significant bit when a whole
IBM vector is packed; flag lists below use ordinary PCIe bit numbering.

| Word | Contents |
|---|---|
| 0 | ROSAR bits0..11; PROC/STOR current12/13, first14/15; reset16, configured17, WAIT18, halt19, KM positive P0/P2 bits20/21; history event marker24 |
| 1 | IC, low24bits |
| 2 | KW current bits0..11 and first-pass bits16..27: PROC summary, STOR summary, raw check1/2, delayed error request, storage error, error-log required, disabled checking, hard-stop071/081, check reset, lamp test |
| 3 | MC flags0..15: accept, sample SDBI, select SE1, store, cancel, set/insert key, TS, protection check, CPU data check, invalid address, timeout, stop-on-storage-check, stop-clock trigger, BCU hold, error reset |
| 4 | AP current full-sum checks0..8 and half-sum checks9..17; groups04–07 through64–67 |
| 5 | SAB, low24bits |
| 6–7 | SDBI high/low32bits |
| 8 | Byte marks, low8bits |
| 9 | Local-store bus, normalized positive data |
| 10 | BF LS parity, MB SDBI parity, SDBO parity, MB SDBO check, bits0..3 |
| 11 | AS parity/full/half checks/P0/P2/P4 bits0..5; serial sum bits8..15 |
| 12 | D register, low24bits |
| 13–14 | ST high/low32bits |
| 15 | KU SOROS/sync, SOROS qualification, CE/logout interrupt, logout, force ROSAR bit7, scan block, scan bypass/logout, oscillator inhibit, ROS-error inhibit, bits0..8; address sequence bits16..20 |
| 16–20 | Core-domain mailbox inputs: SE response, read data high/low, I/O response, I/O interrupt; these may change while ALD is halted |
| 21 | AR ROS69–73 parity, AP ROS69–99 check, qualified half-sum check bits0..2 |
| 22 | AP first-pass full/half checks in the same layout as word4 |
| 23 | KW stop, hard-stop oscillator, CE-clock inhibit, manual, timer disabled; KM timer-step, step/stop/interrupt, PSA hard-stop, bits0..7 |
| 24–30 | SE command/address/write high/write low, I/O command, external register, storage size |
| 31 | Wrapper CPU reset bit0, core readiness bit1 |

Negative ALD check/control pins are inverted where needed to make the named
condition true in this diagnostic schema. Full source mappings are emitted by
`tools/ald_compiler/src/gen_vhd.cpp`; CPU equations and native C++ are unchanged.

## Reader

`tools/m65_diagnostic_dump.py` checks FPGA/interface identity before any command.
Its default action dumps an existing frozen capture; it does not arm or halt.
Explicit options are `--arm`, `--halt-on-event` (only with arm), `--capture`,
`--halt`, `--release`, and `--disarm`. Supply the Linux M65 PCI `resource0` path.
Diagnostic read commands write only the dedicated diagnostic command register.
The reader requires the exact R137/interface1.7 image and rejects the current
R136 image before issuing any command. The live R136 run uses its preserved
matching reader. `--breakpoint 0xA48 --breakpoint-hit 128 --breakpoint-mode p0`
configures only; add `--arm --halt-on-event --mask 3` explicitly to arm. A
configuration request while armed fails without automatically disarming.
`--disable-breakpoint` explicitly disables it while disarmed. For a preserved
halted capture, dump it, use `--arm --mask 0` while halted, then `--step p0` or
`--step ros-change`. Disable/reconfigure the breakpoint before ARM if it should
not preempt the step. `--step` never implicitly arms, discards or releases a
capture. Its reply may show an active step; read again for completion.

Validation uses `tools/test_cpu_diagnostics.py` and the preserved R136 generated
baseline (all generated files must remain byte-identical). Functional simulation, Boolean audits and source equality do not
establish routed timing, hardware behavior or full CPU/SPECIAL equivalence.


## Roller bar5 P0 counter (FPGA1.38)

Position6 displays a 36-bit binary counter, most significant bit at the left.
One count is one sampled positive transition of gated KM P0 (diagnostic state
bit20), irrespective of ROSAR changes. It counts continuously while other roller
positions are selected, clears on panel Power Off/CPU reset, and holds during
diagnostic halt. Held-high P0 and halt/release do not manufacture edges. The
36-bit value wraps modulo 2^36. At 5 MHz it wraps after about 3.8 hours.

Only outgoing physical roller5 bits0..35 are replaced when its five active-low
position contacts B4.17..21 are all high. Status lamps36..39 retain their ALD
values, and the existing frame latch presents a coherent count for each serial
frame. Internal KT/PK scan data and every CPU equation remain unchanged. This
is visual clock activity evidence, not an instruction counter or proof that
console microcode advances. The PCIe1.7 protocol and snapshot schema are unchanged.
