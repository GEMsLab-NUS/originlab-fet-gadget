# FET Gadget for Origin

<p align="center">
  <a href="LICENSE"><img alt="License: MIT" src="https://img.shields.io/badge/License-MIT-yellow.svg"></a>
  <a href="#requirements"><img alt="Origin" src="https://img.shields.io/badge/Origin-10.3%2B-blue.svg"></a>
  <a href="../../releases"><img alt="Release" src="https://img.shields.io/badge/release-latest-brightgreen.svg"></a>
  <a href="docs/architecture.md"><img alt="Origin C" src="https://img.shields.io/badge/built%20with-Origin%20C-informational.svg"></a>
</p>

<p align="center"><b>English</b> | <a href="README.zh-CN.md">简体中文</a></p>

An interactive FET (field-effect transistor) transfer-curve analysis App for Origin/OriginPro.
Import raw CSVs, drag range cursors on the graph to bracket the fit windows, and extract SS, Vth,
gm, mobility, and Ion/Ioff with one click — forward and backward sweeps are detected and extracted
separately, and results from every curve you analyze accumulate in one results table.

The App's internal identity is **FET Gadget** (that's the name in `package.ini`, the OPX filename,
and the folder Origin installs it under); the repository folder is still named `FET Analyzer` from
an earlier iteration — both names refer to the same App.

## Table of Contents

- [Features](#features)
- [Requirements](#requirements)
- [Installation](#installation)
  - [Option A — install a released OPX](#option-a--install-a-released-opx-recommended-for-users)
  - [Option B — build from source](#option-b--build-from-source-recommended-for-developers)
- [Usage](#usage)
- [Supported CSV formats](#supported-csv-formats)
- [Output](#output)
- [Project layout](#project-layout)
- [Development](#development)
- [Roadmap](#roadmap)
- [License](#license)

## Features

| | |
|---|---|
| **Flexible import** | Instrument multi-block `DataName/DataValue` CSVs, plain `Vg/Id` tables, or CSVs with explicit `Forward Vg/Forward Id/Backward Vg/Backward Id` columns. |
| **Automatic scan detection** | Forward (rising) and backward (falling) sweeps are detected and treated as paired curves automatically. |
| **Dual-axis graph** | Auto-generated Id-Vg graph: left axis is `\|Id\|` on a log scale, right axis is linear `Id`, both normalized to `uA/um` by channel width. |
| **Draggable fit windows** | Two pairs of free range cursors per direction — solid for forward, dash-dot for backward — mark the SS and Vth fit windows without snapping to data points. |
| **One-click extraction** | SS, Vth, max gm, field-effect mobility, Ion/Ioff, and hysteresis, with fit lines, reference lines, and a summary annotation drawn directly on the graph. |
| **Live Ioff** | Drag the Ioff reference line and Ioff / Ion-Ioff recompute immediately, on the graph and in the results table. |
| **Non-destructive scan mode** | Switching Auto/Forward/Backward/Both in Settings hides the unused direction's curve (not deletes it) and never discards the cursor positions you already tuned. |
| **Accumulating results** | One row per curve per direction in a hidden `Extracted Parameters` table — re-analyzing a curve updates its row in place, analyzing a different curve appends a new one. |

## Requirements

- **Origin or OriginPro 2022 or later** (`package.ini` declares a minimum of **10.3**).
- Windows (the Origin C / OPX toolchain is Windows-only).
- To build from source, additionally:
  - Origin registered as a COM server on the machine (this happens automatically on a normal install).
  - PowerShell (the version 5.1 that ships with Windows is enough).

## Installation

### Option A — install a released OPX (recommended for users)

1. Download the latest `FET Gadget.opx` from [Releases](../../releases).
2. Drag the `.opx` file onto a running Origin window and follow the install prompt.
3. **FET Gadget** now appears in the App Gallery / Apps panel.

### Option B — build from source (recommended for developers)

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

## Usage

1. **Import** — launch **FET Gadget** (App Gallery icon or toolbar button) from any workbook or
   blank window and pick one or more CSV files. The App creates one curve per file (or per
   `DataName` block within a file), a workbook (`Curves` + `RawMeta`), and a dual-Y-axis Id-Vg
   graph.
2. **Select a curve** — click the right-axis `Id` curve you want to analyze to make it the active
   plot (a graph can hold several curves; you must select one). Launch **FET Gadget** again: if
   there are no range cursors yet, the App places one set automatically (two sets if it detects a
   forward+backward double sweep).
3. **Adjust the fit windows** — drag the vertical cursors: the solid pair covers the forward SS
   subthreshold region / Vth linear turn-on segment, the dash-dot pair (if present) covers the
   backward equivalent. Drag the orange/red Ioff reference line to set the off-state level
   manually — it recomputes Ioff and Ion/Ioff on release. The Ion reference line is fixed.
4. **Read the results** — launch **FET Gadget** once more to compute. The graph shows the SS/Vth
   fit lines and a summary box (`[+]` forward, `[-]` backward). Full results are written to the
   hidden `FETGraphData` workbook's `Extracted Parameters` sheet (one row per curve per direction,
   accumulated across every curve you've analyzed).
5. **Settings** — click the `FET Gadget` button on the graph to open Device / Extraction / Cursors
   / Graph / Output settings: channel `L`/`W`, `Cox` (direct entry or by material/thickness), `Vd`,
   smoothing window, auto fit-window sizes, minimum R², scan mode, colors, and what gets drawn.
   Switching scan mode only changes what this pass draws/analyzes — it never deletes the other
   direction's cursors, so switching back reuses them instantly.

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
| Hidden workbook `FETGraphData` → `Extracted Parameters` | **The main results table.** One row per curve per direction — SS, Vth, gm, mobility, Ion/Ioff, hysteresis, and every fit parameter, ready to export or post-process. |

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

- Only linear extrapolation is implemented for Vth; constant-current and max-gm methods are not
  yet available.
- No batch/cross-curve report export yet (results already accumulate row-by-row in
  `Extracted Parameters`, which you can export with Origin's own tools).
- Gate leakage (`Ig`) is imported but not yet used in analysis.
- Graph/analysis settings are session-local; they aren't yet persisted to the graph object or
  project file.

## License

[MIT](LICENSE)
