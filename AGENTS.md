# Local repository instructions

This file is local guidance requested by the user. Do not stage or commit it.
Work in `D:/Github/IBM360/ibm360m65`. Follow the user's latest instructions when
they change an earlier authorization or constraint. Leave commits to the user.

## FPGA compatibility and ALD scheduling

- When CPU logic or scheduling changes, increment both `M65_FPGA_VERSION` in
  `src/vhdl/pcie/fpga_build.vhd` and `M65_FPGA_REQUIRED_VERSION` in
  `hercules/m65_fpga_version.h`, as required by README.md. Update documentation
  and compatibility tests. A plausible build timestamp is not compatibility proof.
- FPGA/interface revision 1.3 adds ISK key responses to the 100 MHz two-pass CPU.
  SE response bits 29..25 carry the key and bit 24 marks validity. Require a
  matching active ISK sequence before key advance. The user requested the separate
  immutable `ise-100mhz-settle-r13-20261008` ISE trial after the 1.2 build completed.
  Preserve the completed FPGA/interface 1.2 snapshot and its reports unchanged.
- Preserve two finite Boolean NOCLOCK passes per 10 ns core edge. First-pass
  registers and CLOCK state update on the rising edge; the combinational second
  pass reads a global first snapshot and sampled external inputs. CLOCK logic
  reads the previous settled state. Do not remove the reference model's second
  `process_ald()` to conceal a hardware scheduling difference.
- There are no hclk or internal dclk domains, and no falling-edge settling pass.
  Both Boolean passes must meet the 10 ns core period. SPECIAL primitives and
  memories retain their existing core-edge behavior; full SPECIAL/software and
  whole-CPU hardware equivalence are not yet established.
- The user wants to test the current FPGA 1.2 implementation as it stands.
  Defer SPECIAL scheduling changes until functional testing shows a problem;
  preserve the known sampling-gap evidence and report its limited test scope.
  Continue the current build and routed timing checks without changing its snapshot.
  The separate 1.3 build includes the ISK repair while retaining this SPECIAL limitation.
- Change the compiler source, not just generated files. Regenerate with `-O1`.
  `gen/ald` is ignored by Git; verify installed generated VHDL explicitly.
  Preserve C++ emitter/output unless the user's task specifically requires changes.

## Validation and timing evidence

- Run `tools/audit_ald_logic.py` and `tools/audit_ald_schedule.py` against generated
  output. The equation audit maps `_first` assignment targets back to logical
  targets only inside `ALD_NOCLOCK_FIRST`; the schedule audit checks snapshot wiring.
- Use the generated C++/VHDL fixture in `tools/test_ald_settle.py` for scheduling
  changes, plus appropriate GHDL/ISE ISim production regressions and elaboration.
  Tests and Boolean audits do not prove routed timing or complete CPU equivalence.
- Keep CDC mailbox payloads held through acknowledgement. Request/acknowledge,
  panel and status synchronizers have two stages. Preserve physical registers
  and ASYNC_REG/SHREG_EXTRACT attributes, and time local stage-to-stage paths.
- Use locally synchronized reset release for readiness/busy logic. Raw shared
  asynchronous reset must not bypass release stages into synchronous control.
- Verify routed mailbox endpoints and their 8 ns DATAPATHONLY bounds. Exceptions
  must target intended first-stage inputs; do not hide crossings with blanket
  clock-group exclusions. The user authorized removing the unmatched ROS address
  bit-4/7 MAX_FANOUT placement constraints after the two-pass build failed Translate.
  Use default ROS fanout handling and verify routed timing in the replacement trial.
- Build success requires Generate Programming File completion, no active-build
  errors, a fresh nonempty bitstream, verified revision/stamp and matching fetched
  hash. PAR score zero alone is insufficient. Inspect constraint coverage and
  unconstrained paths; external panel/LED/reset I/O requirements remain unspecified.
- At most one bounded expanded timing audit per trial:
  `trce -intstyle silent -v 1 -n 40000 -u 20 -tsi IBM360-clock-audit.tsi -o IBM360-clock-audit.twr IBM360.ncd IBM360.pcf`.
  Check for an existing/running audit first. Do not repeat expensive `-v 5000`
  audits or routed XDL conversions. Save reports, logs and explicit limitations.

## ISE trials and preservation

- Read current trial metadata and the ISE heartbeat prompt for active paths and
  start times. Never infer the active process from an old PID or use archived
  baseline reports as current success. Discover processes and verify remote cwd.
- Use immutable source snapshots with manifests. Do not edit an active trial or
  launch duplicate jobs. The user can authorize cancellation/restart; preserve
  the stopped attempt and start a separately identified trial.
- Preserve all previous trials and logs under `gen/ise-*` and remote
  `/home/ise/ibm360-*`. Keep baseline reports separate from new outputs.
- ISE runs in `ISE_14.7_VIRTUAL_MACHINE`, SSH `ise@192.168.56.102`, with environment
  `/opt/Xilinx/14.7/ISE_DS/settings64.sh`. Use the authorized per-trial SSH helper
  and trusted host key; never print credentials or credential-bearing source.
  Helper exec mode does not change cwd. Linux launch scripts need LF endings;
  ISE forced builds use `-force rerun_all`.
- Do not restart the VM, change resources or program hardware without explicit
  user authorization. Pi remains off. Preserve local validation/result/trial JSON.

## Independent MVT comparison and monitors

- CE indicators alone are not host diagnostic faults. Keep CE checks enabled in
  the ALD and let microcode determine when to branch on checks. Do not introduce
  IC parity power-up presets to satisfy an immediate host CE guard. Actual
  comparison mismatches still require a stop and investigation.
- A verified error-caused ROS transfer into the logout microprogram is also a
  diagnostic stop. Require accepted error-request attribution and the actual
  KU511/DS/RX force-address transfer to ROS019; lamps or ROS019 alone do not qualify.
- MVT and ISE are independent. FPGA work must not modify the active MVT sources,
  executable, private disks, controller or heartbeat, or send guest commands.
- The user authorized a fresh ISK-repaired boot in `gen/mvt-ald-isk-clean`, using
  `gen/hercules-compare-isk/Hercules.exe`, private copies of `gen/mvt-tso-final`,
  both consoles attached before exactly one IPL, and disabled interval timers.
  Use the normal `tools/validate_mvt_compare.py` controller. The new executable
  includes the corrected SSK recorder, so no ignored-error offset/filter applies.
  Require complete=true, zero raw comparison errors and nonempty all-0000 test
  condition codes for completion. Discover process identities rather than reuse PIDs.
- The preserved failed comparison is `gen/mvt-ald-m17-clean`. Read validation-progress.json
  and session.json, retrying incomplete concurrent JSON writes. Use actual session
  instruction count and raw cumulative comparison_errors. The user authorized
  continuing this same emulator after the SSK recorder encoding failure at
  10,136,061 through 10,136,130 (24 reports). The restored controller is
  `tools/resume_mvt_ssk_comparison.py`; it filters only complete SSK records with
  equal addresses and exact five-bit-to-high-five-bit byte agreement. Report
  raw counts, accepted SSK records and remaining errors separately using its
  validation-progress.json. Never suppress other errors or clear raw history.
- Never resume preserved failed/paused runs or launch a competing controller or
  emulator. Do not resume an automatically paused CPU. Both interval timers are
  intentionally disabled. Preserve all logs and private disks.
- For a requested MVT heartbeat, compare progress only with this run's
  heartbeat-last-check.json and update only that monitoring file after reporting.
  Historical failure counts are regression milestones, not completion evidence.
- MVT completion requires complete=true, zero unacknowledged comparison errors,
  all raw errors covered by the verified SSK encoding filter, and a nonempty
  test-job condition-code list containing only 0000. Report the exception
  explicitly; this resumed run is not a zero-raw-error clean comparison.
  Inspect stalls/failures honestly.
- The user explicitly requested progress reports every five minutes for each
  monitor. Preserve this notification intent. Update or pause only the relevant
  heartbeat, preserving its other fields; never alter the other monitor.
- On verified build/audit completion or a terminal failure needing attention,
  report the actual result and pause the ISE heartbeat. On verified MVT completion,
  user stop or failure needing attention, report it and pause the MVT heartbeat.

Use existing scripts and evidence to recover transient progress. Keep this file
focused on durable working rules, not a log of process IDs and elapsed counts.
