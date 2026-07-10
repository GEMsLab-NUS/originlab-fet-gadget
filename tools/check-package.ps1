$ErrorActionPreference = 'Stop'

$root = Split-Path -Parent $PSScriptRoot
$app = Join-Path $root 'origin-app\FET Analyzer'
$required = @(
    'package.ini',
    'launch.ogs',
    'AppIcon.png',
    'FETAnalyzer16.bmp',
    'src\FETAnalyzer.c',
    'xfunctions\fet_analyze.variables.md',
    'examples\FET_transfer_sample.csv',
    'examples\FET_transfer_double_scan.csv',
    'examples\README.md'
)

$missing = @($required | Where-Object { -not (Test-Path -LiteralPath (Join-Path $app $_)) })
if ($missing.Count -gt 0) {
    throw "Missing package files: $($missing -join ', ')"
}

$packageIni = Join-Path $app 'package.ini'
$ini = Get-Content -LiteralPath $packageIni -Raw
foreach ($needle in @('Name=FET Gadget', 'LaunchScript=launch.ogs', 'Always=1', 'Graph=1', 'Workbook=1')) {
    if (-not $ini.Contains($needle)) {
        throw "package.ini does not contain '$needle'"
    }
}
$sourceVersion = [regex]::Match($ini, '(?m)^Version=(.+)$').Groups[1].Value.Trim()
if (-not $sourceVersion) {
    throw 'package.ini does not declare a Version'
}

$launch = Get-Content -LiteralPath (Join-Path $app 'launch.ogs') -Raw
if (-not $launch.Contains('run.loadoc') -or -not $launch.Contains('fet_analyzer_start')) {
    throw 'launch.ogs does not load and invoke the Origin C entry point'
}
if (-not $launch.Contains('%@A\FET Gadget\src\FETAnalyzer.c')) {
    throw 'launch.ogs does not use the installed App source path'
}

$source = Get-Content -LiteralPath (Join-Path $app 'src\FETAnalyzer.c') -Raw
if (-not $source.Contains('[CSV Files (*.csv)] *.csv')) {
    throw 'CSV open dialog filter is not configured for visible .csv files'
}
if (-not $source.Contains('fet_analyzer_get_launch_mode()') -or -not $source.Contains('fet_analyzer_import_csv()')) {
    throw 'App entry point does not route graph and non-graph launches'
}
if (-not $source.Contains('fet_analyzer_multi_analyze();')) {
    throw 'App entry point does not route the multi-curve analysis launch'
}
if (-not $source.Contains('fet_analyzer_scatter_hist();') -or -not $source.Contains('fet_analyzer_correlation_matrix();')) {
    throw 'App entry point does not route the scatter/correlation launches'
}
if (-not $source.Contains("FET Gadget v$sourceVersion")) {
    throw "Origin C dialog title does not expose the current app version ($sourceVersion)"
}
if (-not $source.Contains('FET_CURSOR_SS_START') -or -not $source.Contains('FET_CURSOR_VTH_START')) {
    throw 'Graph launch does not define SS/Vth free cursor objects'
}
if (-not $source.Contains('HMOVE=1') -or -not $source.Contains('fet_analyzer_ranges_from_free_cursors')) {
    throw 'Graph launch does not create free movable SS/Vth range cursors'
}
if (-not $source.Contains('layer.y.type=0') -or -not $source.Contains('layer.y.label.pre$=\"10\\+(\"') -or -not $source.Contains('baseCol + 5') -or -not $source.Contains('SetAttachedAxis')) {
    throw 'Imported graph is not configured as logAbsId/Id overlaid linear-axis plot'
}
if (-not $source.Contains('fet_analyzer_show_settings') -or -not $source.Contains('FET_CONFIG')) {
    throw 'Graph does not expose the FET Analyzer settings button'
}
foreach ($needle in @(
    'label -r legend',
    'FET Gadget',
    'label -p 88 97 -j 1 -n FET_CONFIG',
    '.background=0;',
    '_fet_add_horizontal_cursor_line',
    '_fet_plot_has_backward_scan',
    'options.coxMode = FET_COX_HFOX',
    '|\\i(I)\\-(d)| (uA/um)',
    '\\i(I)\\-(d) (uA/um)',
    '\\i(V)\\-(g) (V)',
    'layer.y.label.pre$=\"10\\+(\"',
    'layer.y.label.numFormat=1',
    'layer.y2.label.numFormat=1',
    'layer.x.grid.show=1',
    'layer.y.from=0',
    'gp.Resize(sizeUnits, sizeUnits, 101)',
    'page.kar=0;page.width=%d;page.height=%d;',
    'page.zoomWhole=1',
    'layer.width=76;layer.height=76',
    '_fet_remove_backward_range_cursors',
    'LINE_STYLE_SHORT_DASH',
    'FET_BACKWARD_LINE_WIDTH',
    '.VMOVE=1;',
    '_fet_add_segmented_visible_plots',
    '_fet_add_hidden_source_plot',
    'tr.Root.Symbol.Shape.nVal = 2',
    'plot.SetSymbol(11)',
    'tr.Root.Symbol.FillColor.nVal = color',
    '_fet_multi_curve_rgb',
    'label -p 88 97 -j 1 -n FET_MULTI',
    '_fet_find_multi_source_book',
    '_fet_detect_curve_layout',
    '_fet_build_multi_overlay_graph',
    '_fet_style_plot_alpha',
    'tr.Root.Line.Transparency.nVal',
    'IDM_PLOT_COLUMN',
    'IDM_PLOT_BAR',
    '_fet_stats_histogram',
    '_fet_render_stats_param',
    '_fet_tag_source_book',
    'fet_analyzer_stats_next_param',
    '_fet_set_graph_page_ratio(progressLayer, 500, 3000)',
    'FET_LAUNCH_SCATTER_HIST',
    'FET_LAUNCH_CORRELATION_MATRIX',
    'fet_analyzer_scatter_hist',
    'fet_analyzer_correlation_matrix',
    '_fet_pearson_correlation',
    '_fet_batch_param_col_name',
    'FETStatsData',
    'FETStatsGraph',
    'FETScatterGraph',
    'FETMultiOverlayGraph',
    '_fet_report_progress_status',
    '_fet_update_progress_label',
    'progressBox',
    '_fet_show_start_dialog',
    '_fet_show_more_dialog',
    '_fet_try_native_marginal_plot',
    '_fet_try_native_scatter_matrix',
    '_fet_pick_new_graph_page',
    '_fet_rename_graph_page_to',
    'FETCorrelationGraph'
)) {
    if (-not $source.Contains($needle)) {
        throw "Requested graph formatting is missing '$needle'"
    }
}
if (-not $source.Contains('_fet_find_vg_column') -or -not $source.Contains('Vg_V') -or -not $source.Contains('Id_A')) {
    throw 'CSV importer does not support ordinary Vg/Id table headers'
}
if (-not $source.Contains('FailedDetails') -or -not $source.Contains('_fet_import_error_text')) {
    throw 'CSV importer does not expose per-file failure details'
}

Write-Host 'FET Gadget package skeleton: OK'
Write-Host "Source package version: $sourceVersion"
$installedIni = Join-Path $env:LOCALAPPDATA 'OriginLab\Apps\FET Gadget\package.ini'
if (Test-Path -LiteralPath $installedIni) {
    $installed = Get-Content -LiteralPath $installedIni -Raw
    $installedVersion = [regex]::Match($installed, '(?m)^Version=(.+)$').Groups[1].Value.Trim()
    Write-Host "Installed app version: $installedVersion"
    if ($installedVersion -ne $sourceVersion) {
        Write-Warning "Installed FET Gadget is not current. Rebuild/reinstall the OPX to update it."
    }
}
Write-Host 'Run tools/build-opx.ps1 for Origin C compile, runtime smoke test, and OPX generation.'
