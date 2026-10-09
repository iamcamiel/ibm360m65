# ibm360m65
IBM360 Model 65 CPU Emulation

The FPGA core uses a 100 MHz PLL clock derived from the board's 200 MHz input.
There is no `hclk` enable. Each 10 ns rising edge stores the first NOCLOCK pass
and updates CLOCK state, delay primitives and CPU memories. A second NOCLOCK
pass evaluates combinationally from that first-pass snapshot, with separate
first-pass connections between sections. CLOCK logic reads the prior settled
state. External inputs for the second pass are sampled on the same edge, so
halt and synchronous reset retain their behavior. Both Boolean passes must
fit within the 10 ns period; there is no falling-edge update or 5 ns half-cycle
constraint. The oscillator retains its 200 ns cycle and 10 ns phase spacing.
Regenerate VHDL with the updated ALD compiler using `-O1`.
The C++ emitter and its two-pass schedule are unchanged. `tools/test_nohclk.vhd` checks oscillator and
delay timing, consecutive local-store updates, halt and reset. Whole-CPU
validation remains a separate check.

`tools/test_ald_settle.py` compares generated VHDL against generated C++ using
`process_ald()`, `process_ald_clock()`, a state copy and the second
`process_ald()`. Its 1,024-cycle fixture covers cross-section feedback, vectors,
aliasing, CLOCK sampling, reset, halt and changes to live external inputs.
`tools/audit_ald_schedule.py gen/ald` checks all generated second-pass Boolean
equations and snapshot connections. These checks do not establish full CPU
equivalence or SPECIAL primitive equivalence to the software model.

The display state machine uses the core clock with a local enable every 512
edges (5.12 us), preserving its serial scan rate without an internal `dclk`.
External panel inputs pass through two synchronizer stages before sampling.
PCIe still uses its independent 62.5 MHz transaction clock. `CDC_MAILBOX`
transfers complete 192-bit register snapshots with request/acknowledge toggles,
two-stage control synchronizers, and an extra capture edge. CPU command bundles
must settle for two core edges before publication. Each BAR write remains busy
until its response snapshot reaches the CPU, preserving data-before-response
ordering and byte-enable read-modify-write behavior. Common bridge reset clears
both ends on PCIe reset/link loss or core clock lock loss; release is synchronized
in each domain. Panel power-off resets the ALD CPU without clearing host configuration.
Mailbox readiness and PCIe busy control use those local reset-release stages;
the raw shared reset does not directly gate synchronous control logic.

The UCF bounds each mailbox data path to 8 ns `DATAPATHONLY`. Exceptions apply
only to request/acknowledge first-stage inputs and panel/indicator first-stage
synchronizers; there is no blanket exception between CPU and PCIe clocks.
Routing must confirm that these groups exist and every data bound passes.
External panel/LED timing requirements and whole-CPU hardware validation remain
unverified; a simulation pass alone does not establish board timing closure.

`tools/test_pcie_cdc.vhd` exercises the production register and write controller
with unrelated clocks, byte enables, sequence-counter ordering, a stopped CPU
clock, and reset during a transfer. `tools/test_display_clock.vhd` checks scan
timing, switch/lamp bit order, power control and reset. With ISE 14.7 loaded, run
these commands from the repository root. ISim resolves source paths inside a
`.prj` relative to that project file's directory, so `../src/...` entries in
`tools/*.prj` refer to this repository's `src/`. Executable and Tcl batch paths
in these commands are relative to the working directory. Compile
each using `fuse -prj tools/test_pcie_cdc.prj -o test_pcie_cdc.exe test_pcie_cdc`
and the corresponding `test_display_clock` project, then run its matching Tcl
batch file. Require `PCIE_CDC_TEST_PASS` / `DISPLAY_CLOCK_TEST_PASS` and no failures.
`tools/test_pcie_tlp_cdc.prj` additionally uses the generated PCIe RX/TX engines
and the entire production PIO stack to check packet-level backpressure and
ordered BAR writes; require `PCIE_TLP_CDC_TEST_PASS`. Generate the PCIe IP first
if its example-design HDL is absent from `xise/ipcore_dir`.

With the ISE environment loaded, run the timing/memory regression from the
repository root with `fuse -prj tools/test_nohclk.prj -o test_nohclk.exe test_nohclk`,
then `./test_nohclk.exe -tclbatch tools/test_nohclk.tcl`. Require
`NOHCLK_TEST_PASS` and no assertion failures.

The ALD compiler's C++ CPU is used for comparison with Hercules before running
the generated VHDL on the FPGA. Hercules supplies memory and peripheral devices.

The software helpers in `tools` also support a Hercules-only reference run and
OS/360 MVT system generation. `install_mvt.py` uses the supplied MFT starter and
distribution libraries from the old `os360mvt` working directory. It creates
private disk copies, runs the preparation jobs, generates Stage 2 from Stage 1,
checks job completion, and saves console milestones and printed listings.

For example, from this checkout with Python available:

```powershell
python tools/install_mvt.py --run-dir gen/mvt-new-install `
  --system-dir D:/Github/IBM360/Backup_ibm360m65/os360mvt `
  --exe gen/hercules-clock-off/Hercules.exe `
  --cpu-model 65 --console-port 3280 --web-port 8150
```

The executable must be the native Hercules reference build, with
`COMPARE_M65` and `FEATURE_INTERVAL_TIMER` disabled. The default target is
Model 65 with selector channels and SER1 recovery. `--cpu-model 158` keeps the
supplied 370/158 definition, which requires instructions absent from this
project's reduced Hercules build.

The Model 65 preparation also applies `tools/mvt-jcl/fix65ios.jcl`: it directs
program interrupts to the MVT handler instead of the absent ITEL instruction
simulator, and repairs the addressing of the HASP error exit. The unresolved
`IECHASPE` link reference is expected until HASP is installed.

After generation, close Hercules cleanly before copying its disks for first
IPL. The local validated generation is in `gen/mvt-install-model65`; the first
boot and its evidence are in `gen/mvt-model65-boot`. `validation.json` records
the successful IPL and the two-step `MVTCHECK` job, which writes records to
disk, reads them back, and prints them. Both steps returned 0000.

`tn3270_console.py` provides the primary MVT screen console in a browser. The
operator console is at <http://127.0.0.1:8152/>. The alternate line console
is at <http://127.0.0.1:8153/>. The validated session used Hercules alone with the
interval timer disabled and is now stopped.

For a new Model 65 IPL, select device 350, send Enter at the system-parameter
prompt, and reply `R 00,'NO'` to the dump-tape request. At READY, the first IPL
needs `T DATE=75.279,Q=(,F)` and `R 00,U` to initialize the job queue. Start
`S INIT`, `S WTR,00E`, answer the printer's numbered UCS request with `PN`,
and start `S RDR,00C` after attaching a card deck. The guest date is deliberately
in the OS/360 era.

With the interval timer disabled, timed console scrolling is unavailable.
`K E,1,19`, entered twice, clears the displayed message area. The helper's
Enter-only input can confirm a host-prefilled command. Use instruction counts
for comparisons; guest CPU-time accounting is distorted in this configuration.

HASP 4.009762 and interactive TSO/TCAM are now installed and tested on the
native Model 65 reference. The browser TSO terminal used
<http://127.0.0.1:8154/>; CAMIEL can log on without a password. It uses
`tools/tso_console.py` and the official portable ws3270 client from
<https://x3270.bgp.nu/download/04.05/>. The primary operator console continues
using `tn3270_console.py`.

The stopped checkpoint `gen/mvt-tso-final` contains the installed disks,
CAMIEL account, initialized broadcast dataset, and two test datasets.
Its `validation.json` records successful logon, allocation/free/catalog,
TSO editor SAVE, TSO SUBMIT to HASP (`TSOTEST`, condition code 0000), and the
HASP disk write/read job (`MVTCHECK`, both steps 0000). TSO, TCAM and HASP
shut down cleanly. `gen/mvt-tso-live` is a separate copy; logon and
both datasets were verified again after its restart. This copy was also
shut down cleanly at the user's request. The build/install
listings are in `gen/mvt-hasp-tso/job-output`.

For a restart, copy the stopped checkpoint through `software_ipl.py` into a
new run directory. Attach the primary console before IPL, and attach the TSO
terminal before starting TCAM. For example, use three separate terminals:

```powershell
python tools/software_ipl.py --run-dir gen/mvt-next --system-dir gen/mvt-tso-final `
  --exe gen/hercules-clock-off/Hercules.exe --reference --config gen.cnf `
  --console-port 3288 --web-port 8153 --ipl-device 350 --console-device 001F --no-ipl
python tools/tn3270_console.py --console-port 3288 --web-port 8152 `
  --run-dir gen/mvt-next-console --control-url http://127.0.0.1:8153
python tools/tso_console.py --exe gen/wc3270/ws3270.exe --console-port 3288 `
  --web-port 8154 --run-dir gen/mvt-next-tso --control-url http://127.0.0.1:8153
```

Use unused ports if another session is running. Select IPL device 350 through
the Hercules control file/console. Send Enter and `R 00,'NO'` at the NIP prompts.
At READY, `T DATE=75.279` retains the clean job queue. Start `S HASP`, answer
its numbered options request with `NOREQ`, then start `S TCAM` and `S TSO`.
`FORMAT,NOREQ` is only for deliberately reinitializing an empty HASP spool.
HASP supplies the readers, writers and initiators.

At the TSO terminal, Clear requests the logon prompt. `LOGON CAMIEL` enters
the account. Keep each browser input line within 72 characters; longer lines
are rejected locally. Input spanning multiple 80-column rows stalled both
terminal implementations, with the clock disabled and enabled; the precise
TCAM cause remains to be diagnosed. Attention sends PA1 to interrupt a TSO
command, PA2 continues paged output, and Reset releases a keyboard error.
The browser waits for a stable screen/cursor before sending commands because
TCAM can unlock before its final page update.

Useful MVT-era commands (allocation syntax differs from modern TSO):

```
LISTCAT
ALLOC DA(DEMO.DATA) NEW SPACE(10,10) BLOCK(80) VOLUME(HERC01)
FREE DA(DEMO.DATA)
EDIT CHECK.CNTL
SUBMIT CHECK.CNTL
LOGOFF
```

If TSO reports `REENTER -`, replace the rejected operand alone, or use
Attention to return to command mode. For shutdown, log off first, enter
`P TSO`, answer its outstanding stop reply with `U` if necessary, then
`Z TP` to close TCAM. Once HASP has no pending jobs, `$PHASP` stops its tasks.
Clear waiting operator messages as needed; `apply_mvt_jobs.py` handles the
actual displayed message range. Enter `Z EOD`, quit Hercules cleanly, and
only then copy its disks.

Execution of this installed MVT system on the ALD CPU and FPGA remains a
separate validation step. The original backup directories are preserved.

The first clock-free ALD comparison stopped at instruction 98,284 while MVT
probed address `0x800000`, the first byte beyond the configured 8 MiB. The
adapted MC351 decoder omitted SAB bit 0, so the model selected storage and
wrapped the transfer into low memory instead of taking an addressing exception.
The paused run and first-failure evidence remain in `gen/mvt-ald-clock-off`.

MC351 now checks all 24 address bits against the configured storage mask.
Regeneration uses the original `-O1` signal-collapsing mode. The generated C++
and VHDL agree on 11,636 Boolean assignments, and `tools/test_storage_decode.cpp`
checks 49 boundary cases using the actual generated MC decoder, with zero
failures. These checks do not establish that the complete CPU interruption
sequence or MVT startup succeeds.

The bit-0 comparison in `gen/mvt-ald-bit0` passed the storage boundary and
then paused at 337,230 instructions with two memory-write mismatch reports.
Both were MVZ (Move Zones): the Hercules instruction wrote memory directly
without recording those writes for the comparator. Its write recording now
retains the original destination pointer and records all `len + 1` bytes,
including zero values. `python tools/test_mvz_compare.py` exercises the actual
instruction body with five cases: both reported zero writes, nonzero zones,
the maximum operand length, and overlapping operands. The paused run and logs
remain preserved; the comparator's strict write checks are unchanged.

The MVZ-corrected run in `gen/mvt-ald-mvz-clean` passed both MVZ failure points,
then paused at 337,752 instructions with four missing Hercules STD write records.
The first was at instruction 337,379. Its disks, executable and logs remain
preserved. `gen/mvt-ald-mvz` is an abandoned startup attempt whose control file
repeated IPL before clearing; its emulator was stopped.

The broader write audit covers this repository's stripped S/370 CPU, rather
than other Hercules architectures or hypothetical optional features. It fixes:

| Write path | Comparison recording change |
| --- | --- |
| Eight-byte storage helper, including STD | Record all eight bytes in aligned and unaligned paths. |
| Split-page eight-byte helper | Record each physical fragment under its correct wrapped operand address. |
| TS (Test and Set) | Record the byte set to FF for both condition codes. |
| CS (Compare and Swap) | Record four bytes only when the exchange succeeds. |
| STCTL (Store Control) | Retain both destination pointers and record the correct register/page spans. |
| Byte and halfword helpers | Record after successful access and storage, preventing phantom writes on exceptions. |
| External interruptions and CPU status storage | Record directly stored PSA fields and saved register arrays. |
| Both machine-check interruption paths | Record direct logout, interruption-code and failure-address stores. |

`python tools/audit_hercules_writes.py` writes `gen/hercules-write-audit.json`:
53 write-related paths with no unclassified direct writers in the reviewed
scope. This source inventory is not a runtime proof. Decimal and ordinary
register-store instructions delegate to the recorded helpers. Shared channel
DMA/CSW stores, IPL/reset initialization and operator memory alterations are
not independent CPU instruction writes. Storage reference/change bits are
separate from the explicit SSK key-write comparison.

`python tools/test_hercules_write_recording.py` executes the actual source
function bodies in a storage fixture: 43 cases pass with comparison enabled
and disabled. Cases cover zero/nonzero values, lengths, preserved write pointers,
page splits/address wrap, conditional CS, TS condition codes, register wrap,
and access faults without write records. The fixture supplies decoding and
translation; it does not execute full interruption sequences or ALD microcode.
The five MVZ tests and nine validation-controller tests also pass. The strict
write-map comparator is unchanged; no discrepancy is suppressed.

The new comparison uses fresh private disks in `gen/mvt-ald-writeaudit`.
Exactly one IPL and both disabled timers were verified. Its primary console
uses port 8192, its comparison status/alternate console uses port 8193, and the
TSO terminal uses port 8194. The optimized Win32 executable
is `gen/hercules-compare-writeaudit/Hercules.exe`; it retains `COMPARE_M65` and
compares registers, condition code, system mask, memory/key writes and I/O.
`validation-progress.json` records the actual stage and whether the complete
startup/TSO/job test has passed. A running or paused test is not a success.

The launcher sets `M65_INTERVAL_TIMER=0` for comparison runs. This operates
the active-low ALD console DISABLE TIMER switch (switch bank 7, bit 13),
in addition to Hercules's disabled `FEATURE_INTERVAL_TIMER`. Startup records
`M65TIMER disable_key=1 clock_enable=0`; the validation controller requires
this confirmation before answering any guest prompts. Previously, disabling
Hercules's timer alone left the model's timer enabled. The preliminary run
in `gen/mvt-ald-compare` was stopped before NIP to correct that configuration;
its 13,782 compared instructions are not a clock-free MVT validation.

For another comparison, choose fresh run directories and unused ports:

```powershell
python tools/software_ipl.py --run-dir gen/mvt-ald-next --system-dir gen/mvt-tso-final `
  --exe gen/hercules-compare-writeaudit/Hercules.exe --console-port 3295 `
  --web-port 8193 --ipl-device 350 --console-device 001F --no-ipl --control-hours 168
python tools/tn3270_console.py --console-port 3295 --web-port 8192 `
  --run-dir gen/mvt-ald-next-console --control-url http://127.0.0.1:8193
```

After attaching the primary console, select IPL device 350 through Hercules.
Then run `python tools/validate_mvt_compare.py --run-dir gen/mvt-ald-next
--checkpoint gen/mvt-tso-final --backend http://127.0.0.1:8193
--primary http://127.0.0.1:8192 --tso http://127.0.0.1:8194
--console-port 3295 --tso-web-port 8194`. The controller answers NIP, retains the clean
job queue, starts HASP with NOREQ, attaches the official TSO client, starts
TCAM/TSO, logs on as CAMIEL, runs LISTCAT, and submits CHECK.CNTL. Each reply
waits for a fresh settled model wait; full-console deletion needs separate
waits too. The first mismatch pauses execution and retains the evidence.
The initial storage clear is slow because every microcycle runs the generated
ALD logic. Do not send competing commands while the controller is active.
Progress-file replacement retries temporary Windows file locks. Failure handling
pauses the CPU before saving its report, so a locked status file cannot bypass
the pause.

### FPGA identity over PCIe

BAR0 keeps its existing CPU register layout and exposes read-only build metadata:

| Byte offset | Value |
| --- | --- |
| `0x7EC` | Metadata signature `0x4D363542` (`M65B`) |
| `0x7F0` | UTC build time, packed BCD `00HHMMSS` |
| `0x7F4` | UTC build date, packed BCD `YYYYMMDD` |
| `0x7F8` | FPGA revision, 16-bit major and minor (now 1.3, including the ISK storage-key return path, AR401 M17 correction and 100 MHz two-pass CPU scheduling) |
| `0x7FC` | PCIe interface revision, 16-bit major and minor (now 1.3) |

SE response bits 31..30 acknowledge the request sequence. An ISK response also
sets bit 24 (key valid) and places the five storage-key bits in 29..25, in IBM
bit order. WA accepts key advance only for a valid response matching its active
ISK request. Other responses clear key valid. Register offsets are unchanged.

Use the ISE 14.7 environment and run `xtclsh tools/build_fpga.tcl /path/to/ibm360m65`.
This regenerates the PCIe core, adds its required HDL, fixes the UCF selection,
and stamps `src/vhdl/pcie/fpga_build.vhd` immediately before synthesis. It also
removes the previous blanket ROS synthesis fanout attribute after ALD generation.
ROS fanout uses default synthesis and placement handling, with register
duplication enabled in MAP. This
does not add a pipeline stage or relax the 10 ns core clock constraint. For a GUI
build, first run `xtclsh tools/stamp_fpga_build.tcl /path/to/fpga_build.vhd` and
`xtclsh tools/distribute_ros_address.tcl /path/to/gen/ald/360_rx.vhd`, then enable
register duplication in XST and MAP.
`SOURCE_DATE_EPOCH` can supply a reproducible UTC timestamp. The checked-in
package has a zero date so an unstamped build is rejected.

The hardware emulator reports the FPGA revision and timestamp before issuing
CPU register commands. It requires interface major 1, interface minor at least
3, FPGA revision exactly 1.3, the metadata signature and a valid timestamp.
Legacy bitstreams that returned 1.1 from unused addresses are rejected. The
build date is diagnostic; matching dates alone never establish compatibility.
Increment the FPGA revision in both `fpga_build.vhd` and
`hercules/m65_fpga_version.h` when changing CPU logic or behavior that must
match the emulator. Bump the interface major for incompatible register or
protocol changes. These fields identify a declared revision and build time,
not a cryptographic identity of the complete source snapshot.

`tools/m65_pcie_probe.c` uses the same compatibility policy. It lists PCIe
metadata by default; `--read` performs only BAR0 reads. `--fixture FILE` checks
an ordinary 2048-byte test image. Hardware mode still requires its remaining
Pi integration work and an actual FPGA test; these checks do not prove that
the CPU or PCIe link works on the board.
