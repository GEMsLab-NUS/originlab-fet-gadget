param(
    [switch]$SkipSmoke
)

$ErrorActionPreference = 'Stop'

$root = Split-Path -Parent $PSScriptRoot
$appRoot = Join-Path $root 'origin-app\FET Analyzer'
$source = Join-Path $root 'origin-app\FET Analyzer\src\FETAnalyzer.c'
$smoke = Join-Path $root 'tests\FETAnalyzerSmoke.c'
$smokeCsv = Join-Path $root 'origin-app\FET Analyzer\examples\FET_transfer_instrument_multicurve.csv'
$smokeTableCsv = Join-Path $root 'origin-app\FET Analyzer\examples\FET_transfer_sample.csv'
$smokeDoubleCsv = Join-Path $root 'origin-app\FET Analyzer\examples\FET_transfer_double_scan.csv'
$smokeSplitCsv = Join-Path $root 'origin-app\FET Analyzer\examples\FET_transfer_split_scan.csv'
$smokeOutputCsv = Join-Path $root 'origin-app\FET Analyzer\examples\FET_output_curve_rejected.csv'
$smokeGenericCsv = Join-Path $root 'origin-app\FET Analyzer\examples\FET_transfer_generic_current.csv'
$smokeMetadataGuardCsv = Join-Path $root 'origin-app\FET Analyzer\examples\FET_transfer_metadata_prefix_guard.csv'
$smokeXAxisNameOutputCsv = Join-Path $root 'origin-app\FET Analyzer\examples\FET_output_curve_xaxisname_rejected.csv'
$actualCsvs = @(
    'C:\Users\NUS MSE\Desktop\Transfer curve 1V.csv',
    'C:\Users\NUS MSE\Desktop\Transfer curve 1V-2.csv',
    'C:\Users\NUS MSE\Desktop\Transfer curve 1V-3.csv'
)
$actualIdVgCsv = 'C:\Users\NUS MSE\Desktop\IdVg(1) [2nd batch MoS2.csv'
$build = Join-Path $root 'build'
$opx = Join-Path $build 'FET Gadget.opx'
$appsRoot = Join-Path $env:LOCALAPPDATA 'OriginLab\Apps'
$stagedApp = Join-Path $appsRoot 'FET Gadget'

New-Item -ItemType Directory -Path $build -Force | Out-Null

function Quote-LabTalkPath([string]$Path) { return $Path }

$origin = $null
try {
    $origin = New-Object -ComObject Origin.Application
    $origin.Visible = 0
    [void]$origin.Execute('sec -poc 60;')

    $srcLT = Quote-LabTalkPath $source
    [void]$origin.Execute("__fet_build=run.LoadOC(`"$srcLT`",16);")
    $compileCode = [int]$origin.LTVar('__fet_build')
    if ($compileCode -ne 0) {
        throw "Origin C compile failed: run.LoadOC returned $compileCode"
    }

    if (-not $SkipSmoke) {
        $smokeLT = Quote-LabTalkPath $smoke
        [void]$origin.Execute("__fet_smoke_build=run.LoadOC(`"$smokeLT`",16);")
        $smokeCompile = [int]$origin.LTVar('__fet_smoke_build')
        if ($smokeCompile -ne 0) {
            throw "Smoke test compile failed: run.LoadOC returned $smokeCompile"
        }
        $origin.LTStr('__FET_SMOKE_CSV$') = $smokeCsv
        $origin.LTStr('__FET_SMOKE_TABLE_CSV$') = $smokeTableCsv
        $origin.LTStr('__FET_SMOKE_DOUBLE_CSV$') = $smokeDoubleCsv
        $origin.LTStr('__FET_SMOKE_SPLIT_CSV$') = $smokeSplitCsv
        $origin.LTStr('__FET_SMOKE_OUTPUT_CSV$') = $smokeOutputCsv
        $origin.LTStr('__FET_SMOKE_GENERIC_CSV$') = $smokeGenericCsv
        $origin.LTStr('__FET_SMOKE_METADATA_GUARD_CSV$') = $smokeMetadataGuardCsv
        $origin.LTStr('__FET_SMOKE_XAXISNAME_OUTPUT_CSV$') = $smokeXAxisNameOutputCsv
        if (($actualCsvs | Where-Object { -not (Test-Path -LiteralPath $_) }).Count -eq 0) {
            for ($i = 0; $i -lt $actualCsvs.Count; $i++) {
                $origin.LTStr('__FET_ACTUAL_CSV_{0}$' -f ($i + 1)) = $actualCsvs[$i]
            }
        }
        if (Test-Path -LiteralPath $actualIdVgCsv) {
            $origin.LTStr('__FET_ACTUAL_IDVG_CSV$') = $actualIdVgCsv
        }
        [void]$origin.Execute('__fet_smoke_result=fet_analyzer_runtime_smoke();')
        $smokeResult = [int]$origin.LTVar('__fet_smoke_result')
        if ($smokeResult -ne 0) {
            throw "Runtime smoke test failed: code $smokeResult"
        }
    }

    # mkOPX's app mode is the supported way to preserve the App-relative file
    # layout. Synchronize the development folder into Origin's Apps root first.
    New-Item -ItemType Directory -Path $stagedApp -Force | Out-Null
    Copy-Item -Path (Join-Path $appRoot '*') -Destination $stagedApp -Recurse -Force

    if (Test-Path -LiteralPath $opx) {
        Remove-Item -LiteralPath $opx -Force
    }
    $opxLT = Quote-LabTalkPath $opx
    if (-not $origin.Execute("mkOPX app:=`"FET Gadget`" opx:=`"$opxLT`";")) {
        throw 'mkOPX command failed'
    }
    if (-not (Test-Path -LiteralPath $opx)) {
        throw 'mkOPX returned without creating the output file'
    }

    Get-Item -LiteralPath $opx | Select-Object FullName, Length, LastWriteTime
}
finally {
    if ($null -ne $origin) {
        try { $origin.IsModified = $false } catch {}
        try { [void]$origin.Exit() } catch {}
        try { [void][Runtime.InteropServices.Marshal]::FinalReleaseComObject($origin) } catch {}
    }
}
