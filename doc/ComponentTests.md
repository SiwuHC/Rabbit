# Component tests: the Stream family and SeriWrapComponent

`rabbit_App/tests/component_test.cpp` drives the components through exactly the API the
GUI controllers use, offscreen, with no board attached:

```bash
cd BRAM_Test/Rabbit
xmake build -j1 component_test            # -j1 on purpose: a parallel full-app build
QT_QPA_PLATFORM=offscreen ./build/linux/x86_64/release/component_test
# fallback without xmake:
QT_DIR=/home/camel/tools/qt/Qt/6.5.3/gcc_64 bash rabbit_App/tests/run_component_test.sh
```

Last run: **175 checks, 0 FAIL** (`component_test.log`).

## What is covered

| area | checks |
|---|---|
| factory + port names/types/counts for all 8 Stream components and SeriWrap | 46 |
| frame-bit layout after binding ports to real pins (`PinInfo` tables) | 18 |
| StreamInput 8/16/32/Float: three-phase CLK/STROBE pulse per value | 48 |
| StreamInput: `clk_hold`, value ordering, `target_count` | 12 |
| StreamOutput 8/16/32/Float: rising-edge + DATA_VALID capture, reset | 24 |
| SeriWrap: manifest load/reject, ready gating, send path, read path, reset | 13 |
| non-vacuity guards (data bits actually carried, frames confined to own pins) | 14 |

Values are injected through the real widgets (`QLineEdit` + the `onEnterPressed` /
`onSendClicked` slots) and read back from the widgets the components own, so the
widget wiring is under test too, not just the logic.

## Findings that are worth knowing (all confirmed on the running code)

1. **A StreamOutput's control pins are `CLK` and `DATA_VALID`**, and its data pins are
   `DATA[i]` -- not `DOUTi`/`VLD`.  Data bits are read at frame bit `pin_index-1`,
   control bits likewise; a StreamInput's pins sit at frame bit `pin_index`.
2. **`pin_index` is a placeholder until a project binds pins.**  `appendPort()`
   numbers ports sequentially at construction; `ProjectFileHandler` replaces the
   indices with `declIndexMap(pin)`.  Any frame-level test must bind first (the
   suite does, via `inputDeclIndexMap`/`outputDeclIndexMap`).
3. **StreamInput pulse train.**  Per value: SETUP (`CLK=0, STROBE=1`) held `clk_hold`
   frames, then `CLK=1, STROBE=1` held `clk_hold` frames -- that is the design's
   latch edge, and it occurs exactly once per value.  The FALLING state is *not*
   held: with another value queued it is merged into that value's SETUP frame, and
   when the queue drains it is emitted once before the component idles.
   The merged frame is then followed by a **full** SETUP phase as well, so the first
   word costs `2*clk_hold` frames and every later word `3*clk_hold` frames.  The
   pulse is still correct (one latch edge per word); the cost is throughput.
4. **`target_count` reached:** the closing FALLING frame is replaced by an all-zero
   write frame -- `CLK=0, STROBE=0` exactly as intended, with the data bits dropping
   at the same time.  Electrically identical, so not a defect, but a test that counts
   data-carrying FALLING frames will under-count.
5. **SeriWrap is ready-gated.**  Arming a frame is not enough: the design's `READY`
   bit must have been seen in a read frame first (`processReadData`), which is what
   the GUI does once per frame.  Without it `getWriteData()` stays 0 -- that is the
   behaviour a host must implement.
6. **SeriWrap manifest loading is now testable**: the parse-and-apply path moved into
   `SeriWrapRawComponent::loadManifestFile(path, &error)`; the `Manifest...` button
   keeps the file dialog and shows the returned error text in a `QMessageBox`.

## Why the pulse train matters on the real board

`probe_clk` measured on the FDP3P7 that **one host access advances the design by
exactly one clock** (see `probe_clk/README.md`).  Combined with finding 3, a 16-bit
word costs `3*clk_hold` host accesses in steady state -- 60 accesses at the default
`clk_hold = 20`.  Anyone budgeting invocation time for these Stream components on
that board has to count accesses, not just FPGA cycles; lowering `CLK Hold` (down to
1) is the direct lever, as long as the wrapped design still sees each level.