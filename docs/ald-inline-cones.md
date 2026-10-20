# Explicit combinational wire cones

`INLINE` marks local scalar Boolean definitions whose wire delay must not become
an additional model state. `NOCLOCK` ends the marked region. The compiler retains
each marked output for diagnostics and substitutes its expression into local
NOCLOCK consumers. This applies to generated C++ and to both VHDL Boolean passes.
Nested cones expand recursively; cycles, external outputs, vector outputs and
non-Boolean expressions are rejected. CLOCK consumers retain their previous
settled-state inputs. SPECIAL primitives are not expanded or rescheduled.

Use this annotation only where the original drawing establishes the cone and a
regression establishes its required behavior. It does not add a settling loop,
another Boolean pass, a falling-edge pass or a different clock domain.

## Status A, revision 1.41

KS121 5D-5H/6F generates negative RESET STAA combinationally; all five reset
products include P0. KS021's positive status-A inverter feeds the negative
status-A latch's hold product. Evaluating either wire as an independently delayed
state can lose the held status at the F4/P0 control handoff. Moving RESET STAA to
NOCLOCK in R139 removed one clock classification error but left these Boolean-pass
delays. Expanding the reset alone also fails: the independently evaluated
complementary feedback can already be oscillating before the handoff.

Both the reset cone and positive feedback inverter are now explicitly marked.
The hold term therefore reads the current reset expression and the complement
of the prior negative latch state within each existing pass. Original gate
polarities, reset/set products and clock-phase equations are unchanged. Open
triangles on the drawings are negative pins; junction diamonds do not invert.
The expansion also reaches two other local status-A consumers, for three changed
KS NOCLOCK assignments in total. Other sections and all generated headers remain
byte-identical to R139.

The captured R46 failure is SRDA R14,32 at 7553E2, shifting
00000000007816E8 to zero but returning CC2. Private full-CPU execution reproduces
the same input, instruction and CC failure with R139. The corrected model returns
CC0 at the production comparator's instruction-exit predicate. Its private exit
ROS is 227, whereas R46's saved fault ROS is 21B; the exact historical MVT
microcycle window was not retained. The reproduction establishes this defect and
its correction without claiming an exact replay of every historical transition.

## Regression commands

Run `tools/test_ald_inline.py` with the compiler, GHDL and an output directory for
512 actual generated C++/VHDL cycles covering nested cones, feedback, CLOCK
transitions, sampled inputs, halt/reset and invalid definitions. Run
`tools/test_srda_boundary.py` against the generated 48-section CPU for ten native
SRDA cases. Its `--negative` mode expects the old R139 failure. The runner extracts
the production exit predicate from `hercules/cpu.c` and checks CC there; sampling
only at the later result-store request misses the bug. These tests use private
memory and never control an emulator, guest or physical CPU.

Full Boolean/schedule audits, the existing 1,024-cycle generated settle fixture,
original KS121 pin tests and complete VHDL elaboration are also required. Passing
functional tests does not establish complete CPU/SPECIAL equivalence or routed
timing. R46's stopped process, logs, disks and capture evidence remain preserved.
