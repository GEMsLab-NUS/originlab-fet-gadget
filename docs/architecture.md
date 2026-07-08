# Architecture

## Runtime Flow

```text
App Gallery
  -> launch.ogs
  -> run.loadoc(.../src/FETAnalyzer.c)
  -> fet_analyzer_start()
  -> route by active window
      -> workbook/no graph:
           Import Data
           -> GetMultiOpenBox
           -> CSV importer
           -> Curves/RawMeta workbook only
      -> active graph:
           Analyze Graph
           -> build overlaid logAbsId/Id layers
           -> detect single or forward/backward scan segments
           -> read solid (+) and dash-dot (-) free cursor lines
           -> convert cursor X bounds to segment-local XYRange objects
           -> Origin C calculation core
           -> graph annotations + hidden fit-data workbook
           -> [FETResults]Summary
```

## Responsibilities

- Origin C parses CSV files, creates worksheets, creates graphs, computes parameters, and writes output.
- CSV column matching accepts several instrument naming styles for Vg/Vd/Ig/Id
  (verbose names, SPICE-style `Vgs`/`Ids`, and a last-resort match onto a
  leftover `I*`-named current column when no alias matches). Before accepting
  a block as a transfer curve, the importer also checks the B1500
  `Channel.VName`/`Channel.Func` metadata (when present): if Vg is confirmed
  held CONST or used as the secondary VAR2 sweep rather than the primary
  VAR1 sweep, the block is an Output/Id-Vd curve and is skipped rather than
  misread as a transfer curve. Built-in "ApplicationTest" exports (e.g.
  `Trans.`, `DaulPolarOutput`) carry no `Channel.VName`/`Channel.Func` at
  all; for those the same check falls back to
  `AnalysisSetup, ...XAxis.Name`, the swept-axis name the instrument
  recorded for its own graph. Files with neither signal import unfiltered
  as before.
- Header-row detection for the no-`DataName` ("Classic") layout rejects any
  candidate line that has a namespaced (dotted, e.g. `Output.List.Data`)
  token before its first recognized Vg/Id column. Real B1500 exports almost
  always carry an `Output.List.Data,Vg,Id,Ig,...`-style metadata line naming
  the same columns as the real header, just at different positions; without
  this guard that decoy line gets accepted as the header and every
  downstream column is misread against the wrong index.
- LabTalk is used for App launch, graph-object cursor glue, graph formatting, and graph-object button scripting.
- Graph cursors are free movable line objects, not Data Selector ranges. They do not snap to data points and are used only to mark X bounds.

## Graph Model

- Each imported curve writes six columns: `Vg`, `Id`, `Ig`, `absId`, `Vd`, `logAbsId`.
- Each acquisition direction is a separate visible plot. A double scan therefore
  creates forward and backward plots instead of one line crossing the turn point.
- The right layer also contains one hidden full-range source plot per imported
  curve. Analysis reads this source so visual segmentation does not break paired
  forward/backward extraction.
- The left layer plots `logAbsId`; its scientific tick labels transform the data
  to `|I_d| (uA/um)`. The right axis transforms `Id` to `I_d (uA/um)` and starts
  at zero. Both conversions use the configured channel width.
- The graph page is portrait, and both overlaid plot layers are sized to a 1:1
  width-to-height ratio. Origin preserves page paper ratios when assigning
  `page.width`/`page.height`, so the enforced ratio lives on the layer.
- `FET_CONFIG` is a `FET Gadget` text-button at the plot's upper-right
  corner (`label -p` positions are percent-of-frame with (0,0) at
  bottom-left, so the Y coordinate must be near 100 to land near the top).
- Solid `FET_SS_*` / `FET_VTH_*` cursors mark the forward (+) ranges.
- Dash-dot `FET_BWD_SS_*` / `FET_BWD_VTH_*` cursors mark the backward (-)
  ranges, so they're visually distinct from the solid forward cursors even
  in the same color.
- Backward cursors are only removed when the curve has no backward segment
  at all (a data fact from scan-turn detection). Choosing "Forward" in
  settings only skips backward analysis/plotting for that pass -- it leaves
  the backward cursors in place so switching back to "Both"/"Backward"
  immediately reuses them instead of starting from scratch.
- scanMode also hides (`layer -hp`, never deletes) the forward/backward
  visible curve for whichever direction isn't selected, so "Forward" in
  Settings visibly removes the backward curve from the graph rather than
  only skipping its analysis. Because a hidden segment now coexists with
  the always-hidden full-range source plot, curve identification
  (`_fet_get_analysis_plot_for_graph_layer`) keys off "the plot with the
  most points" rather than "the only hidden plot" to find that source --
  a segment is always a strict subset of it and can never have more points.
- Settings persist scan mode, smoothing window, automatic fit-window sizes,
  minimum R-square, fit visibility, and marker visibility. Curve/axis/marker
  colors are fixed (`_fet_default_dialog_options`), not user-configurable --
  the color pickers/presets that used to live under the "Graph" settings
  branch were removed by request.

## Analysis Output

- SS and linear-range fit lines are dashed, extended beyond their selected
  ranges, and use a dark blend of their associated curve color.
- Ion is a non-movable full-width short-dash reference level. Ioff defaults to
  the mean absolute current in the recognized off/SS region and is shown as a
  movable horizontal short-dash cursor; moving it refreshes Ion/Ioff.
- The hysteresis marker (`FET_HYST`) is a single user-movable horizontal
  cursor on the left (log) layer, drawn only when a backward segment is
  actively analyzed. It defaults to the vertical center of the left axis's
  *final* Y range (computed after Ion/Ioff-driven axis expansion, so it
  matches what's actually displayed) the first time it's created, and
  otherwise keeps whatever Y the user last dragged it to. Because leftLayer
  and rightLayer are fully overlaid on the same physical frame, the
  fraction of leftLayer's Y range the cursor sits at is also the fraction of
  rightLayer's Y range -- so the same cursor position yields two data-space
  levels (log for the left axis, linear Id for the right axis) without a
  second cursor. A `FET_HYST_SUMMARY` panel (labeled `[+/-]`, since it
  compares forward against backward rather than reporting one direction)
  shows both resulting Vg deltas as `\g(D)V-(line)` and `\g(D)V-(log)`; a
  level either segment never reaches shows as "N/A" rather than a bogus
  number. Both deltas are also written to
  `[FETGraphData]Extracted Parameters` (`Hyst Delta Vg` / `Hyst Delta Vg
  (linear)`) on the curve's forward (`[+]`) row.
- Scale-attached two-point lines (`_fet_add_scale_line`, used for the Ion
  reference line and the hysteresis cursor) must re-apply their `x1/y1/x2/y2`
  endpoints *after* `.ATTACH` is set, not just at `draw -lm` creation time --
  LabTalk does not retroactively reinterpret an object's already-set
  coordinates when its attach mode changes, so skipping this silently
  strands the object at the wrong Y.
- Extracted points are semi-transparent filled circles with solid edges.
- Forward and backward results are labeled `[+]` and `[-]`.
- Parameter tables are attached to the upper-left of the layer frame.
- Results are upserted into `[FETGraphData]Extracted Parameters`, one row per
  curve per direction, keyed by `"<curve dataset> [+]"` / `"[-]"` (the
  `Curve` column). Re-analyzing the same curve (e.g. while dragging a cursor
  to refine SS/Vth ranges) updates its existing row in place; analyzing a
  different curve or graph appends new rows instead of overwriting.

## Remaining Extensions

1. Persist graph-specific settings in graph-object storage or page tree.
2. Add constant-current and maximum-gm Vth methods.
3. Add gate leakage analysis from `Ig`.
4. Add batch report export.
