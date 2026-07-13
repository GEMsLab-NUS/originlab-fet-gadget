<p align="center">
  <img src="origin-app/FET%20Analyzer/AppIcon.svg" width="72" height="72" alt="FET Gadget icon">
</p>

# FET Gadget for Origin

<p align="center">
  <a href="LICENSE"><img alt="License: MIT" src="https://img.shields.io/badge/License-MIT-yellow.svg"></a>
  <a href="#requirements"><img alt="Origin" src="https://img.shields.io/badge/Origin-2018%2B-blue.svg"></a>
  <a href="../../releases"><img alt="Release" src="https://img.shields.io/badge/release-latest-brightgreen.svg"></a>
  <a href="docs/architecture.md"><img alt="Origin C" src="https://img.shields.io/badge/built%20with-Origin%20C-informational.svg"></a>
</p>

<p align="center"><b>English</b> | <a href="README.zh-CN.md">简体中文</a></p>

<p align="center">
<a href="#demo-videos">Demo</a> | <a href="#requirements">Requirements</a> | <a href="#installation">Installation</a> | <a href="#usage">Usage</a> | <a href="#supported-csv-formats">CSV formats</a> | <a href="#output">Output</a> | <a href="#features">Features</a> | <a href="#project-layout">Project layout</a> | <a href="#development">Development</a> | <a href="#roadmap">Roadmap</a> | <a href="#license">License</a>
</p>

An interactive FET (field-effect transistor) transfer-curve analysis App for Origin/OriginPro.
Import raw CSVs, drag range cursors on the graph to bracket the fit windows, and extract SS, Vth,
gm, mobility, and Ion/Ioff with one click — forward and backward sweeps are detected and extracted
separately, and results from every curve you analyze accumulate in one results table.

The App's internal identity is **FET Gadget** (that's the name in `package.ini`, the OPX filename,
and the folder Origin installs it under); the repository folder is still named `FET Analyzer` from
an earlier iteration — both names refer to the same App.

<p align="center">
  <img src="docs/demo.png" alt="FET Gadget analyzing a forward/backward Id-Vg sweep in Origin" width="720">
  <br>
  <sub>Dual-axis Id-Vg graph with draggable fit-window cursors and the forward (<code>[+]</code>) /
  backward (<code>[-]</code>) parameter summary drawn directly on the plot.</sub>
</p>

## Demo videos

**Import & single-curve analysis**

https://github.com/user-attachments/assets/73d247df-0e6e-4a9b-8612-da2a005568a7

**Multi-curve analysis**

https://github.com/user-attachments/assets/25d83a72-da7c-4a33-8b2c-386c827eeda9

## Requirements

- **Origin or OriginPro 2018 or later** (`package.ini` declares a minimum of **9.5**, matching
  Origin 2018 — that's roughly when OriginLab's App Gallery/OPX packaging matured; the App can't
  install at all on versions that predate App Gallery support, regardless of this number). The
  code itself only uses long-standing Origin C / Apps APIs, but this floor hasn't been verified by
  actually running on Origin 2018 — please [open an issue](../../issues) if you hit a version-
  specific problem.
- Windows (the Origin C / OPX toolchain is Windows-only).
- To build from source, additionally:
  - Origin registered as a COM server on the machine (this happens automatically on a normal install).
  - PowerShell (the version 5.1 that ships with Windows is enough).

## Installation

### Option A — install a released OPX (recommended for users)

1. Download the latest `FET Gadget.opx` from [Releases](../../releases).
2. Drag the `.opx` file onto a running Origin window and follow the install prompt.
3. **FET Gadget** now appears in the App Gallery / Apps panel.

<details>
<summary><b>Option B — build from source</b> (recommended for developers)</summary>

```powershell
git clone https://github.com/GEMsLab-NUS/originlab-fet-gadget.git
cd originlab-fet-gadget
powershell.exe -NoProfile -ExecutionPolicy Bypass -File .\tools\build-opx.ps1
```

The build script:

1. Launches Origin (via COM) and compiles `origin-app/FET Analyzer/src/FETAnalyzer.c`.
2. Compiles and runs the runtime smoke test in `tests/FETAnalyzerSmoke.c` (skip with `-SkipSmoke`).
3. Syncs `origin-app/FET Analyzer` into `%LocalAppData%\OriginLab\Apps\FET Gadget` and packages it
   with `mkOPX` into `build/FET Gadget.opx`.

Since the build also installs/updates the App in your local Origin Apps directory, you can try it
in Origin immediately afterward without a separate drag-and-drop install. The resulting
`build/FET Gadget.opx` can still be copied to and installed on any other Origin machine (Option A).

<details>
<summary>Manual development install (debug directly in Code Builder, no OPX)</summary>

Copy `origin-app/FET Analyzer` to `%LocalAppData%\OriginLab\Apps\FET Gadget`, then add that folder
to your workspace in Origin's Code Builder to edit and debug in place. See
[docs/opx-packaging.md](docs/opx-packaging.md) for the full packaging/release walkthrough.

</details>

</details>

## Usage

Launching **FET Gadget** opens a dialog with buttons for **Import**, **Single-Curve Analysis**,
and **Multi-Curve Analysis**, plus a **More...** button for **Scatter + Histograms** and
**Correlation Matrix**.

### Import

Pick one or more CSV files.

- **One file** — full import (`Vg/Id/Ig/absId/Vd/logAbsId`) plus the dual-axis Id-Vg graph, ready
  for single-curve analysis.
- **Multiple files** — a compact `Vg`/`Id`-only workbook (XYXY column pairs, no graph) — the entry
  point for multi-curve analysis.

### Single-curve analysis

1. Click the `Id` curve on the graph to select it, then run **Single-curve analysis** — it
   auto-places range cursors (two pairs if it detects a forward+backward sweep).
2. Drag the cursors to set the SS/Vth fit windows (solid = forward, dash-dot = backward) and drag
   the Ioff reference line to set the off-state level.
3. Run **Single-curve analysis** again to compute. Fit lines and a summary box (`[+]`/`[-]`) are
   drawn on the graph; results are appended to `FETGraphData → Extracted Parameters`.
4. Click the `FET Gadget` button on the graph for Device/Extraction/Cursors/Graph/Output settings
   (`L`/`W`/`Cox`/`Vd`, smoothing, fit-window sizes, min R², scan mode, colors). Switching scan
   mode never discards the other direction's cursors.

### Multi-curve analysis

Reads whichever worksheet or graph is **already active** — no file prompt.

1. Run **Multi-Curve Analysis** (or click `[FET Multi]` on a result graph) and set device/fit
   parameters, the analyzed segment (`Forward (+)`, `Backward (-)`, or `Both (+/-)`),
   histogram bin count, and which parameter to open the stats graph on.
2. Every curve is fit automatically (progress bar + Cancel support), building:
   - **Overlay graph** (`FETMultiOverlayGraph`) — log `|Id|`/linear `Id`, all curves translucent
     except the lowest-SS one, which is solid and bold.
   - **Statistics graph** (`FETStatsGraph`) — one histogram at a time (SS, Vthgm, Vthcc, gm,
     µFE, Ion, log₁₀(Ion/Ioff)) with a normal-fit overlay; `[Prev]`/`[Next]` cycles through all seven.
   - **Data** (`FETStatsData`) — `Parameters`, `Statistics`, `Histogram`, `OverlayCurves` sheets.

### Scatter + Histograms / Correlation Matrix

Both need `[FETStatsData]Parameters` to exist (run Multi-Curve Analysis first) and never re-fit
curves.

- **Scatter + Histograms** — pick X/Y parameters (SS, Vthgm, Vthcc, gm, Mobility, Ion, Ioff,
  Ion/Ioff, or log₁₀ ratio); adds a synced 3-column source sheet inside `FETStatsData`
  (for example `SS-Mobility`: `Name`, X, Y, all linked back to `Parameters`) and
  builds `FETScatterGraph` with Origin's native Marginal Histograms template.
- **Correlation Matrix** — check 2+ parameters; builds `FETCorrelationGraph` (native "Scatter
  Matrix" template, Pearson-coefficient-table fallback at `[FETStatsData]Correlation`).

## Supported CSV formats

See [`origin-app/FET Analyzer/examples/`](origin-app/FET%20Analyzer/examples/) for sample files
covering each format:

| File | Format |
|---|---|
| `FET_transfer_sample.csv` | Plain two-column `Vg,Id` table. |
| `FET_transfer_double_scan.csv` | Single `Vg,Id` column, one continuous forward+backward sweep. |
| `FET_transfer_split_scan.csv` | Explicit `Forward Vg,Forward Id,Backward Vg,Backward Id` columns. |
| `FET_transfer_instrument_multicurve.csv` | Instrument export: metadata header + multiple `DataName/DataValue` blocks. |

## Output

| Location | Contents |
|---|---|
| Import workbook (e.g. `FETImportedData1`) | `Curves`: 6 columns per curve (`Vg`/`Id`/`Ig`/`absId`/`Vd`/`logAbsId`); `RawMeta`: skipped raw text lines, for troubleshooting import issues. |
| Id-Vg graph | Overlaid log `\|Id\|` (left) + linear `Id` (right) axes; forward is a solid line, backward is a thinner solid line; fit lines, reference lines, and the summary box are drawn on top. |
| Hidden workbook `FETGraphData` → `Curves` | Forward/backward split plus the full combined curve for whichever curve is currently being analyzed — feeds the graph's fit lines and hidden source plot; not normally something you need to open. |
| Hidden workbook `FETGraphData` → `Extracted Parameters` | **The main results table.** One row per curve per direction — SS, Vthgm, Vthcc, gm, mobility, Ion/Ioff, hysteresis, and every fit parameter, ready to export or post-process. |
| Multi-curve workbook (e.g. `FETMultiData1`) | `Curves`: 2 columns per curve (`Vg`/`Id` only, XYXY…) when more than one file was imported at once. No graph — run multi-curve analysis on this workbook next. |
| Graph `FETMultiOverlayGraph` | Log `\|Id\|` (left, indigo) + linear `Id` (right, amber) — the classic single-curve palette, colored axes included, no legend. Most curves are genuinely translucent; the lowest-SS curve is solid, bold, and fully opaque. Carries the `[FET Multi]` re-run button. |
| Workbook `FETStatsData` + graph `FETStatsGraph` | Multi-curve analysis output: `Parameters` (one row per analyzed curve segment; `Both (+/-)` suffixes `Curve` names with `[+]`/`[-]`), `Statistics` (N/mean/SD/median/min/max/CV per parameter), `Histogram` (bin data), `OverlayCurves` (derived data feeding the overlay graph), and a single-panel histogram graph with `[Prev]`/`[Next]` buttons to step through all seven parameters. Rebuilt on every multi-curve analysis run. |
| Workbook `FETStatsData` → `X-Y` sheet + graph `FETScatterGraph` | Scatter of two chosen batch parameters plus marginal histograms on each axis using Origin's native Marginal Histograms template. The source sheet is named from the selected parameters (for example `SS-Mobility`) and has exactly three main columns: `Name`, X, Y; all three columns are linked to `Parameters`, and masks update only when you run Sync Parameter Masks. |
| Graph `FETCorrelationGraph` (or workbook `FETStatsData` → `Correlation` as a fallback) | Native Scatter Matrix plot over the parameters you checked, or — if that can't be confirmed — a Pearson correlation coefficient table. Rebuilt on every Correlation Matrix run. |

## Features

| | |
|---|---|
| **Flexible import** | Multi-block instrument CSVs, plain `Vg/Id` tables, or explicit forward/backward columns. |
| **Auto scan detection** | Forward/backward sweeps detected and paired automatically. |
| **Dual-axis graph** | Log `\|Id\|` + linear `Id`, normalized to `uA/um`. |
| **Draggable fit windows** | Free range cursors per direction, no snapping to data. |
| **One-click extraction** | SS, Vthgm, Vthcc, gm, mobility, Ion/Ioff, hysteresis — fit lines drawn on the graph. |
| **Live Ioff** | Drag the reference line; Ioff / Ion-Ioff recompute instantly. |
| **Non-destructive scan mode** | Switching direction hides curves and keeps cursor positions. |
| **Accumulating results** | One row per curve/direction — updates in place or appends. |
| **Compact multi-curve import** | Multi-file selection builds a lightweight `Vg/Id`-only workbook. |
| **Unified multi-curve analysis** | One command builds the overlay and distribution graphs for every curve. |
| **Scatter + marginal histograms** | Any two batch parameters, synced to the `Parameters` sheet by curve name and mask state. |
| **Correlation matrix** | Native Scatter Matrix plot, Pearson-table fallback. |
| **Progress while it runs** | Native progress bar + Cancel for batch operations. |

## Project layout

```text
origin-app/FET Analyzer/       App source and packaging assets (installs as "FET Gadget")
  src/FETAnalyzer.c              Core Origin C implementation (import, graphing, analysis)
  launch.ogs                     App entry point (LabTalk)
  package.ini                    App metadata (name, version, enable conditions, ...)
  xfunctions/                    Optional X-Function wrapper notes (fet_analyze)
  examples/                      Sample CSVs covering every supported input format
  templates/                     Placeholder notes for graph/analysis templates
tests/FETAnalyzerSmoke.c       Runtime smoke test (calls the real Origin C functions end-to-end)
tools/
  build-opx.ps1                  One-shot compile + smoke test + OPX packaging
  check-package.ps1              Static package-structure/metadata check (no Origin required)
docs/
  architecture.md                Runtime data flow, layer/column layout, implementation notes
  opx-packaging.md               Detailed OPX packaging and release steps
data/                            Real measurement samples used during development
```

## Development

```powershell
# Full build: compile + smoke test + package
powershell.exe -NoProfile -ExecutionPolicy Bypass -File .\tools\build-opx.ps1

# Skip the runtime smoke test, compile + package only (faster, less confidence)
powershell.exe -NoProfile -ExecutionPolicy Bypass -File .\tools\build-opx.ps1 -SkipSmoke

# Static package-structure check, no Origin required
powershell.exe -NoProfile -ExecutionPolicy Bypass -File .\tools\check-package.ps1
```

`tests/FETAnalyzerSmoke.c` drives the real exported Origin C functions (import, cursor placement,
computation, scan-mode switching, ...) and, when real measurement CSVs are found on the desktop,
additionally runs a pass against real data — checking curve counts, layer structure, the config
button, cursor read/write, and computed parameters. See [docs/architecture.md](docs/architecture.md)
for the full runtime data-flow notes.

## Roadmap

- Vth is reported as `Vthgm` (linear extrapolation on the gm fit window) and `Vthcc`
  (constant-current threshold, default `1e-5 uA/um`, configurable in settings). Max-gm-only
  threshold variants are not yet available.
- Multi-curve analysis always uses automatic SS/Vth window picking; per-curve manual cursor
  adjustment is only available in the single-curve flow.
- Multi-curve overlay translucency uses Origin's line-transparency format tag, which round-trips
  correctly on this build but isn't confirmed on every Origin version; it falls back to a plain
  opaque line automatically if a given install doesn't support it, rather than failing.
- Gate leakage (`Ig`) is imported but not yet used in analysis.
- Graph/analysis settings are session-local; they aren't yet persisted to the graph object or
  project file.
- Scatter + Histograms uses Origin's native `plot_marginal` Marginal Histograms template first.
  It keeps visible source data in `FETStatsData` as a three-column `Name`/X/Y sheet; all three are
  linked back to `Parameters`, and mask synchronization is explicit via Sync Parameter Masks.
- Correlation Matrix tries Origin's native `plotmatrix` gallery template first and falls back
  automatically to a Pearson coefficient table if a real result can't be confirmed. This fallback
  logic was verified thoroughly, but whether the native template renders correctly wasn't
  independently confirmed in a live interactive Origin session — if you only ever see a
  plain Pearson coefficient table instead of a Scatter Matrix plot, please
  [open an issue](../../issues) so the detection logic can be tuned further.
- The hand-built Scatter + Histograms fallback still exists for Origin installs where
  `plot_marginal` does not produce a valid 3-layer graph.

## License

[MIT](LICENSE)
