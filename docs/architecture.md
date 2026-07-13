# Architecture

## Runtime Flow

```text
App Gallery
  -> launch.ogs
  -> run.loadoc(.../src/FETAnalyzer.c)
  -> fet_analyzer_start()
  -> a plain modal GetNBox dialog with 4 direct buttons (Import,
     Single-Curve Analysis, Multi-Curve Analysis, More...) plus a "More..."
     sub-dialog for the 2 cross-parameter operations -- see "Launcher Menu"
      -> Import:
           -> GetMultiOpenBox (single or multiple files)
           -> 1 file  -> full 6-col/curve layout + single-curve graph
           -> >1 file -> compact 2-col/curve (Vg/Id only) layout, no graph
           -> Curves/RawMeta workbook
      -> Single-Curve Analysis:
           -> requires an active graph with a curve selected
           -> build overlaid logAbsId/Id layers
           -> detect single or forward/backward scan segments
           -> read solid (+) and dash-dot (-) free cursor lines
           -> convert cursor X bounds to segment-local XYRange objects
           -> Origin C calculation core
           -> graph annotations + hidden fit-data workbook
           -> [FETGraphData]Extracted Parameters
      -> Multi-Curve Analysis:
           -> reads the ACTIVE worksheet or graph directly -- no file
              prompt; source is whatever Import (or a prior multi-curve
              run) already produced
           -> batch fitting settings dialog (device, segment,
              smoothing/windows/R2, histogram bins, starting parameter)
           -> per-curve auto SS/Vth windows + calculation core,
              on-graph + status-bar progress while it runs
           -> [FETStatsData]Parameters / Statistics / Histogram /
              OverlayCurves
           -> FETStatsGraph: one-parameter-at-a-time histogram with a
              scaled normal-distribution overlay, [Prev]/[Next] buttons
              cycle (closed-loop) through all 7 parameters
           -> FETMultiOverlayGraph: log+linear overlay, built together
              with the histograms in the same pass
      -> Scatter + Histograms:
           -> requires [FETStatsData]Parameters to already exist (run
              Multi-Curve Analysis at least once first) -- no curve
              re-fitting, reads batch results directly
           -> settings dialog: 2 dropdowns (X parameter, Y parameter)
           -> rebuilds a synced 3-column source sheet inside FETStatsData,
              named from the selected pair (for example SS-Mobility):
              Name, X, Y; all 3 are auto-recalculate links to Parameters
           -> tries Origin's native "Marginal Histograms" plot_marginal
              X-Function first; hand-built 3-layer graph is only fallback
      -> Correlation Matrix:
           -> also requires [FETStatsData]Parameters
           -> settings dialog: checkboxes, which parameters to include
           -> tries Origin's native "Scatter Matrix" plotmatrix X-Function
              first; falls back to a pairwise Pearson coefficient table in
              [FETStatsData]Correlation if that doesn't produce a real
              result -- either way ends up named FETCorrelationGraph when a
              graph is produced
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

## Multi-Curve Import and Analysis

- Import decides layout by file count (`fet_analyzer_import_csv`): more than
  one file selected switches `fet_import_transfer_csv_files_ex` to
  `multiStyle=true`, which writes only `Vg`/`Id` per curve (2 columns,
  `FET_IMPORT_COLS_PER_CURVE_COMPACT`) into a `FETMultiData` workbook and
  builds no graph. A single file keeps the full 6-column layout and the usual
  single-curve graph. `_fet_detect_curve_layout` recovers which layout a given
  `Curves` sheet uses later (by column count, then by whether column index 2's
  long name is `"Vg"`), so multi-curve analysis works on either.
- Multi-curve analysis (`fet_analyzer_multi_analyze`) never prompts for
  files. `_fet_find_multi_source_book` resolves the source workbook from
  whatever's active: the active worksheet's page directly if it has a
  `Curves` sheet; otherwise, for the active graph, it first checks a hidden
  `FET_SRC_BOOK` text tag (`_fet_tag_source_book`/`_fet_read_source_book_tag`)
  written onto the graph when it was built, and only falls back to matching
  plot dataset names against worksheet-page names
  (`_fet_find_import_book_for_graph`) if that tag is absent. The tag exists
  because neither `FETMultiOverlayGraph` nor `FETStatsGraph` plots the
  original import book directly (they plot `[FETStatsData]OverlayCurves` /
  `Histogram`), so dataset-name matching alone can't resolve it -- this is
  what the `[FET Multi]` re-run button on either graph relies on.
- Because compact imports carry no precomputed `logAbsId`, a hidden
  `[FETStatsData]OverlayCurves` sheet (4 columns per curve: `Vg`/`Id`/`absId`/
  `logAbsId`) is derived from whichever layout the source uses
  (`_fet_build_multi_graph_data`), purely to feed the overlay graph. It never
  touches the user-facing import table.
- Batch extraction (`_fet_multi_analyze_core`) runs the same auto range
  picking and calculation core as single-curve analysis, per curve, on the
  chosen sweep segment (forward, backward, or both). In both mode each
  successful row's `Curve` name is suffixed with `[+]` or `[-]` so downstream
  sheets do not see duplicate names. Curves too short or that fail the
  fit-quality gates are skipped and listed, never fatal. The curve with the
  lowest SS among those that succeed becomes `bestCurveIndex`.
- `FETMultiOverlayGraph` (`_fet_build_multi_overlay_graph`) uses the classic
  fixed single-curve palette, not a per-curve rainbow: every curve on the log
  (left) axis is drawn in `options.logCurveColor` (indigo) and every curve on
  the linear (right) axis in `options.linearCurveColor` (amber) -- the same
  color a plain single-curve import uses. No legend is drawn. Every curve
  except `bestCurveIndex` is drawn with real per-plot line transparency
  (`_fet_style_plot_alpha`, `tr.Root.Line.Transparency.nVal`, verified to
  round-trip through `GetFormat`/`ApplyFormat`; falls back to the plain
  opaque style if that tag fails to validate on some Origin version) at a
  1.0pt width; `bestCurveIndex` is fully opaque at 2.5pt. `_fet_set_y_axis_color`
  keeps the left/right axis line and label colored to match (indigo/amber),
  rather than left at Origin's default. The graph carries a `FET_MULTI`
  button (`[FET Multi]`) that re-runs `fet_analyzer_multi_analyze()` using
  its own source workbook.
- Outputs are singletons, rebuilt per run: `[FETStatsData]Parameters` (one row
  per analyzed curve segment: SS, R2s, Vthgm, Vthcc, gm in uS, mobility, Ion/Ioff
  densities in uA/um, ratio and log10 ratio), `[FETStatsData]Statistics`
  (N/mean/SD/median/min/max/CV per parameter, one row per of the 7 parameters), and
  `[FETStatsData]Histogram` (bin centers/counts plus normal-curve samples, 4
  columns per parameter). Histogram bins default to the sqrt rule clamped to
  [4, 15]; the normal overlay is scaled by `N * binWidth` so it sits directly
  on the counts.
- `FETStatsGraph` is a **single layer**, not a multi-layer grid. An earlier
  six-layer-per-page 2x3 grid (each layer individually positioned via
  `layer.left/top/width/height`) kept rendering as a stale, overlapping mess
  even though every layer's geometry read back correct immediately after
  being set (confirmed via headless COM probes reproducing the exact
  add-layer/position/plot sequence). `_fet_render_stats_param` now draws
  exactly one parameter's histogram + Gaussian overlay + N/mean/SD label into
  that one layer, reading only from the already-computed
  `[FETStatsData]Statistics`/`Histogram` sheets (so switching parameters
  never re-runs curve fitting), and `[Prev]`/`[Next]` buttons
  (`fet_analyzer_stats_prev_param`/`_next_param`, module-level
  `g_fet_stats_current_param` backed by a hidden `FET_STATS_PARAM_IDX_TAG`
  text object on the layer for robustness across separate button-click
  invocations, wrapped with `%` so the cycle is closed-loop in both
  directions) cycle through all 7. (A button-opens-a-`GETN_LIST`-dropdown
  "jump to any parameter directly" variant was tried and reverted back to
  plain `[Prev]`/`[Next]` per explicit user preference.) `_fet_stats_param_meta`
  is the single source of truth for each parameter's name/unit/axis title,
  shared by the data-computation step (`_fet_stats_compute`) and the
  renderer so the two can't drift out of sync on ordering. `layer -a` also
  auto-adds a legend for the 2-plot (bars + Gaussian) layer, immediately
  removed with `label -r legend;` in the same script -- not wanted since the
  color/axis label already identify the series. The N/mean/SD readout
  (`FET_HIST_STAT`) is a label above the frame acting as the title, at
  `-p 15 97 -j 0` -- the exact same page-percent placement style (and same
  Y) as `[FET Multi]`'s own `-p 88 97 -j 1`, which has sat reliably above
  this frame for the app's whole history. **Getting here took four failed
  attempts, all abandoned** rather than chasing a fifth workaround:
  (1) LabTalk's `-t` switch (the special object literally named `TITLE`)
  landed top-LEFT instead of Origin auto-centering it; (2) a manual
  `-p 50 96 -j 1` (hoping `-j 1` would center-justify around x=50) didn't
  either -- `-j` governs multi-line text alignment WITHIN the object's own
  box, not which point of the box the (x, y) coordinate anchors to, and for
  `label -p` that anchor is always the object's own Left-Top corner
  regardless of `-j` (confirmed via Object Properties > Position tab:
  Horizontal came back as exactly the input 50, unshifted for the text's
  rendered width); (3) even once a negative-Y target `(0, -7)` in "% of
  layer" units was identified as visually correct (verified by inspecting
  that same tab), `label -p 0 -7 -j 0 ...` in one shot silently failed to
  create the object at all, and `label -p 0 (-7) -j 0 ...` created it but at
  the WRONG position (read back as Y=10.5) -- both confirmed via COM probe;
  (4) the workaround for THAT -- create at neutral `-p 0 0`, then move it
  with a follow-up `FET_HIST_STAT.y=-7;` property assignment -- read back
  correctly at exactly -7 across every repeatable scenario a COM probe could
  throw at it (repeated Prev/Next, repeated full page rebuilds, a real
  visible window, even manually resizing that window), yet still drifted to
  `(0, 173)` in real use by an as-yet-unidentified trigger. Any negative or
  otherwise out-of-[0,100]-range "% of layer" Y is now avoided entirely.
  There's no separate "Parameter X / Y: name" header anymore, since
  `[Prev]`/`[Next]` and the X-axis label already show the parameter name.
  `[Prev]`/`[Next]` sit in the bottom corners (`-p 2 3` / `-p 88 3`),
  not the top-right, so they don't collide with the `[FET Multi]` button
  which lives at `-p 88 97`.
- **Never delete-and-recreate a GraphObject from inside the click script it
  is currently running.** `[Prev]`/`[Next]` (and, briefly, a button-based
  parameter picker that replaced and was later reverted back to them) used
  to call `_fet_delete_graph_object` then recreate themselves on every
  render, including the render triggered BY clicking that exact button --
  destroying a GraphObject mid-click leaves Origin's click routing on that
  layer wedged, so the button stops responding to further clicks until some
  other window is activated and this one reactivated. This turned out to be
  the actual root cause of what looked like several different "Prev/Next is
  stuck" symptoms reported over this feature's history.
  `_fet_render_stats_param` (and the title label above) now creates each
  GraphObject once (`if (!obj) { create }`) and only updates `.Text`/style
  on later renders, matching the create-once pattern `_fet_add_multi_button`
  already used for `[FET Multi]` (which never showed this symptom). This
  live click-routing behavior isn't independently verifiable via headless
  COM read-backs -- only the structural fix (no delete call on the hot
  path) is.
- **Page sizing root cause**: `GraphPage.Resize(w, h, 101)` only sets an
  initial GDI size hint. The actual final page size is a *separate*,
  absolute-unit LabTalk aspect-lock -- `page.kar=0;page.width=N;page.height=N;`
  where `N` is in units of 1/600 inch (`4560` = 7.6in, the size every
  single-curve graph already used) -- and Resize() alone silently has no
  visible effect unless page.width/height are set to match. The original
  six-layer stats grid never set page.width/height explicitly at all, so it
  inherited whatever tiny default a blank "Origin"-type page happens to get;
  that (not the multi-layer positioning, which read back correct) is what
  actually made 12pt+ text overlap everything -- `_fet_set_graph_page_ratio`
  now takes both `sizeUnits` and `pageWidthUnits` and every graph in this
  file (including `FETStatsGraph`, at 5.00in / `pageWidthUnits=3000`, and
  `FETScatterGraph`, at 6.00in / `pageWidthUnits=3600`) sets both together.
  `_fet_render_stats_param` re-asserts this on every parameter switch, not
  just once at graph creation, as cheap insurance against Origin reflowing
  the layer back to a smaller default after `RemovePlot`/`AddPlot` churn.
- The batch settings dialog (`_fet_get_multi_options`) shares
  `g_fet_dialog_options` device/extraction fields with the single-curve
  settings dialog, so tuning them anywhere tunes both flows. It also has a
  "Show parameter" dropdown that sets `g_fet_stats_current_param` directly,
  so a run can start on any of the 6 parameters, not always SS.
- Progress: CSV import and the per-curve batch-fitting loop both drive a
  native `progressBox` (confirmed working headlessly via COM probe) with
  `SetRange(0, N)` / `Set(i)` per item -- `Set()` returns `false` if the user
  clicked Cancel, which stops the loop early but keeps whatever was already
  imported/analyzed rather than discarding it. They also still report
  through `_fet_report_progress_status` (an ASCII `[####----]` bar written to
  Origin's status bar via `type -s`, with any embedded filename escaped
  first) alongside the progress box. The batch-fitting loop additionally
  shows a live on-graph readout (`_fet_update_progress_label`, the same
  `label -p` text-object pattern used for every other on-graph annotation in
  this file) on the statistics graph's first panel while it runs, removed
  once fitting finishes.

## Launcher Menu

- `fet_analyzer_start()` shows a plain `GetNBox` dialog (`_fet_show_start_dialog`)
  -- an ordinary modal Windows dialog, not a graph page. An earlier version
  drew the 5 operations as button `GraphObjects` on a hidden-axes graph page
  to get a guaranteed vertical layout; that read as "the App just opened a
  plot" rather than a menu and was reverted on user feedback. A later version
  used a `GETN_LIST` dropdown instead (since GetNBox's custom-button
  mechanism tops out at 4 extra buttons -- `GETNE_ON_CUSTOM_BUTTON5` does not
  exist, confirmed by probe -- one short of the 5 operations); that was also
  reverted on user feedback ("I don't want a dropdown, direct buttons are
  best").
- The current design is 2 levels of `GETN_CUSTOM_BUTTON` dialogs, respecting
  both constraints at once: the main dialog (`_fet_start_dialog_event`) has 4
  direct buttons -- Import, Single-Curve Analysis, Multi-Curve Analysis,
  More... -- and choosing "More..." (`s_fet_start_action == -1`) opens a
  second dialog (`_fet_show_more_dialog`/`_fet_more_dialog_event`) with 2
  more direct buttons -- Back, Scatter + Histograms, Correlation Matrix.
  `s_fet_start_action` (module-level) carries the chosen `FET_LAUNCH_*`
  constant back out through `PEVENT_GETN_RET_TO_CLOSE`, and
  `fet_analyzer_start()` dispatches to each entry function directly.

## Scatter + Marginal Histograms and Correlation Matrix

- Both read `[FETStatsData]Parameters` directly rather than re-fitting
  curves -- they're cross-parameter analyses over batch results that must
  already exist (Multi-Curve Analysis has to have run at least once).
  `_fet_batch_param_col_name` maps a 9-entry selection index (SS, Vthgm,
  Vthcc, gm, Mobility, Ion, Ioff, Ion/Ioff, log10(Ion/Ioff)) to its
  `Parameters` column;
  `_fet_read_batch_param_pair` reads two columns index-aligned to the same
  curve, skipping rows with an empty `Curve` name (padding) or either value
  missing.
- Scatter + Histograms keeps its visible source data in `FETStatsData`
  instead of creating a separate `FETScatterData` workbook.
  `_fet_build_scatter_source_sheet` rebuilds a three-column sheet named
  from the selected pair (`X-Y`, e.g. `SS-Mobility`) with `Name`, X, and Y.
  It keeps every non-empty `Curve` row from `[FETStatsData]Parameters`;
  masked rows are not filtered out. The Name/X/Y columns are set with
  `csetvalue ... recalculate:=1` links back to the corresponding
  `Parameters` columns, so numeric edits in `Parameters` can be propagated
  by Origin recalculation. Mask state is intentionally not copied at build
  time; the `[Sync Parameter Masks]` action is the only path that applies
  current `Parameters` masks to this source sheet.
- Correlation Matrix still tries Origin's own built-in graph-gallery
  template via the `plotmatrix` X-Function before falling back to a
  hand-built table. Headless probing showed a failed/degenerate plotmatrix
  call can report `LT_execute()==true` while only emitting several junk
  single-layer throwaway pages instead of one real matrix grid. The reliable
  check is to snapshot every `GraphPage` name before the call
  (`_fet_snapshot_graph_page_names`), diff after, and judge each newly
  created page by its **shape** (`GraphPage.Layers.Count()`), not just its
  existence. `_fet_pick_new_graph_page(before, minLayers, &graphName)` picks
  the new page with the highest layer count, requires it to clear
  `minLayers`, and destroys every OTHER newly created page either way so a
  degenerate X-Function call never leaves clutter pages behind. On success,
  `_fet_rename_graph_page_to` renames the X-Function's auto-generated page
  name to this App's fixed page name (destroying any stale page already
  occupying it first), so downstream code can find `FETCorrelationGraph`.
- `fet_analyzer_scatter_hist` / `_fet_get_scatter_options`: two `GETN_LIST`
  dropdowns pick the X and Y parameter. `_fet_build_scatter_hist_graph`
  first activates the paired `X-Y` source sheet and calls Origin's native
  `plot_marginal -r 1 iy:=(B,C) type:=0 top:=0 right:=0` Marginal
  Histograms template, then renames the resulting graph page to
  `FETScatterGraph`. This native path does not create an `X-Y-Hist` helper
  sheet. If the X-Function does not produce a real 3-layer graph, the
  fallback writes histogram bin data to `X-Y-Hist` and builds the main
  scatter plus top/right marginal histogram layers manually.
- `fet_analyzer_correlation_matrix` / `_fet_get_correlation_options`: 9
  `GETN_CHECK` boxes pick which parameters to include (>=2 required).
  `_fet_build_correlation_matrix` first tries `plotmatrix`
  (`_fet_try_native_scatter_matrix`, Origin's native "Scatter Matrix"
  gallery template, ellipse + linear fit overlays enabled); if that doesn't
  clear the layer-count bar above, it falls back to computing pairwise
  Pearson correlation (`_fet_pearson_correlation`) over every selected
  pair's paired values and writing an NxN coefficient table to
  `[FETStatsData]Correlation` (self-correlation forced to exactly `1.0` on
  the diagonal rather than computed, since a zero-variance pair would
  otherwise return `NANUM`) -- a **plain numeric table, not a colored
  heatmap image**, since Origin C's worksheet cell/conditional-formatting
  API isn't exercised anywhere else in this file. The native path is the
  intended way to get an actual correlation *plot* per the user's request;
  the table is strictly a fallback for when the native template can't be
  confirmed.

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
2. Add maximum-gm-only Vth variants beyond the current Vthgm/Vthcc outputs.
3. Add gate leakage analysis from `Ig`.
4. Add batch report export.
5. Confirm in a live interactive Origin session whether `plotmatrix`
   actually produces its native gallery template (headless COM testing this
   session could only confirm the *detection* logic is sound, not that the
   native template renders correctly end-to-end). If the native `plotmatrix`
   path never clears the layer-count bar in practice,
   the hand-built Pearson coefficient table remains the permanent fallback,
   and a color-coded heatmap would need its own follow-up API investigation.
6. Confirm `IDM_PLOT_BAR`'s rendered orientation on a live Origin session and
   swap to a swapped-axis line-density fallback if it isn't horizontal.
