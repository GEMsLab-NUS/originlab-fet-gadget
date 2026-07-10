/*
 * FET Gadget MVP for Origin 2022+ / Origin 10.x.
 * Core runtime: Origin C. LabTalk is used only for App launch, graph object
 * cursor glue, and small graph-formatting operations.
 */

#include <Origin.h>
#include <GetNBox.h>
#include <xfutils.h>
#include <sys_utils.h>

#define FET_APP_TITLE "FET Gadget v0.11.0"
#define FET_MIN_POINTS 3
#define FET_IMPORT_COLS_PER_CURVE 6
#define FET_IMPORT_COLS_PER_CURVE_COMPACT 2
#define FET_LAUNCH_IMPORT_CSV 1
#define FET_LAUNCH_GRAPH_ANALYSIS 2
#define FET_LAUNCH_MULTI_ANALYZE 3
#define FET_LAUNCH_SCATTER_HIST 4
#define FET_LAUNCH_CORRELATION_MATRIX 5
#define FET_AUTO_RANGE_MAX_POINTS 30
#define FET_CURSOR_SS_START "FET_SS_START"
#define FET_CURSOR_SS_END "FET_SS_END"
#define FET_CURSOR_VTH_START "FET_VTH_START"
#define FET_CURSOR_VTH_END "FET_VTH_END"
#define FET_CURSOR_BWD_SS_START "FET_BWD_SS_START"
#define FET_CURSOR_BWD_SS_END "FET_BWD_SS_END"
#define FET_CURSOR_BWD_VTH_START "FET_BWD_VTH_START"
#define FET_CURSOR_BWD_VTH_END "FET_BWD_VTH_END"
#define FET_CONFIG_BUTTON "FET_CONFIG"
#define FET_PREVIEW_PAGE "FETPreviewData"
#define FET_FIT_PAGE "FETFitData"
#define FET_AXIS_LEFT 0
#define FET_AXIS_RIGHT 1
#define FET_SCAN_AUTO 0
#define FET_SCAN_FORWARD 1
#define FET_SCAN_BACKWARD 2
#define FET_SCAN_BOTH 3
#define FET_COX_DIRECT 0
#define FET_COX_HFOX 1
#define FET_COX_ALOX 2
#define FET_COX_SIOX 3
#define FET_COX_MANUAL_KAPPA 4
#define FET_CURSOR_WIDTH 1.25
#define FET_FORWARD_LINE_WIDTH 2.0
#define FET_BACKWARD_LINE_WIDTH 1.0
#define FET_FIT_EXTENSION 0.15
#define FET_HYST_CURSOR "FET_HYST"
#define FET_HYST_SUMMARY "FET_HYST_SUMMARY"
#define FET_GRAPH_DATA_PAGE "FETGraphData"
#define FET_MULTI_BUTTON "FET_MULTI"
#define FET_MULTI_PROGRESS_LABEL "FET_PROGRESS"
#define FET_SOURCE_BOOK_TAG "FET_SRC_BOOK"
#define FET_STATS_DATA_PAGE "FETStatsData"
#define FET_STATS_GRAPH_PAGE "FETStatsGraph"
#define FET_MULTI_OVERLAY_GRAPH_PAGE "FETMultiOverlayGraph"
#define FET_STATS_PARAM_COUNT 6
#define FET_BATCH_PARAM_COUNT 8
#define FET_SCATTER_DATA_PAGE "FETScatterData"
#define FET_SCATTER_GRAPH_PAGE "FETScatterGraph"
#define FET_CORRELATION_GRAPH_PAGE "FETCorrelationGraph"

typedef struct tagFETOptions
{
    double L_um;
    double W_um;
    double Cox_F_cm2;
    int coxMode;
    double oxideThickness_nm;
    double oxideKappa;
    double Vd_V;
} FETOptions;

typedef struct tagFETResult
{
    double ss_mV_dec;
    double ss_slope_dec_V;
    double ss_intercept;
    double ss_r2;
    double vth_V;
    double vth_slope_A_V;
    double vth_intercept_A;
    double vth_r2;
    double gm_S;
    double gm_Vg_V;
    double mobility_cm2_Vs;
    double ion_A;
    double ioff_A;
    double ion_ioff;
    double ion_Vg_V;
    double ioff_Vg_V;
    double ion_signed_A;
    double ioff_signed_A;
    double gm_Id_A;
    double hysteresis_level_logA;
    double hysteresis_forward_Vg;
    double hysteresis_backward_Vg;
    double hysteresis_delta_Vg;
    double hysteresis_delta_Vg_linear;
    string source_range;
    string ss_range;
    string vth_range;
    string gate_warning;
} FETResult;

typedef struct tagFETImportContext
{
    Worksheet curves;
    Worksheet meta;
    GraphLayer graphLayer;
    GraphLayer linearGraphLayer;
    int curveCount;
    int metaRow;
    int maxRows;
    bool makeGraph;
    // Compact multi-curve mode: only Vg/Id are written per curve (2 columns
    // instead of the full 6), and no graph is built at import time -- the
    // overlay/statistics graphs are built later by multi-curve analysis.
    bool multiStyle;
    int colsPerCurve;
} FETImportContext;

typedef struct tagFETRangeIndices
{
    int ssBegin;
    int ssEnd;
    int vthBegin;
    int vthEnd;
    int segmentBegin;
    int segmentEnd;
    bool backward;
} FETRangeIndices;

typedef struct tagFETDialogOptions
{
    FETOptions device;
    bool annotate;
    bool appendSummary;
    bool refreshGraphStyle;
    bool recreateCursors;
    bool showFitLines;
    bool showMarkers;
    bool showOnOffArrow;
    bool showHysteresisCursor;
    int scanMode;
    int smoothingWindow;
    int ssWindowPoints;
    int vthWindowPoints;
    double minFitR2;
    int logCurveColor;
    int linearCurveColor;
    int ssColor;
    int vthColor;
    int markerColor;
} FETDialogOptions;

static void _fet_add_config_button(GraphLayer& gl);
static void _fet_add_multi_button(GraphLayer& gl);
static int _fet_rgb_color(int red, int green, int blue);
static bool _fet_is_helper_plot(DataPlot& plot);
static void _fet_style_plot(DataPlot& plot, int color, bool symbols,
                            int lineStyle = LINE_STYLE_SOLID,
                            double lineWidth = 2.0);
static bool _fet_detect_scan_turn(const vector& vx, int& turnIndex);
static void _fet_default_dialog_options(FETDialogOptions& options);
static bool _fet_range_from_x_bounds_segment(DataPlot& plot,
                                             double x1, double x2,
                                             int segmentBegin,
                                             int segmentEnd,
                                             XYRange& range);
static bool _fet_ranges_from_named_cursors(GraphLayer& gl, DataPlot& plot,
                                           LPCSTR ssStartName,
                                           LPCSTR ssEndName,
                                           LPCSTR vthStartName,
                                           LPCSTR vthEndName,
                                           int segmentBegin,
                                           int segmentEnd,
                                           XYRange& ssRange,
                                           XYRange& vthRange);
int fet_analyzer_refresh_preview();

static FETDialogOptions g_fet_dialog_options;
static bool g_fet_dialog_options_initialized = false;

void fet_analyzer_reset_options_for_test()
{
    _fet_default_dialog_options(g_fet_dialog_options);
    g_fet_dialog_options_initialized = true;
}

void fet_analyzer_set_scan_mode_for_test(int scanMode)
{
    if (!g_fet_dialog_options_initialized)
    {
        _fet_default_dialog_options(g_fet_dialog_options);
        g_fet_dialog_options_initialized = true;
    }
    g_fet_dialog_options.scanMode = scanMode;
}

static void _fet_get_effective_dialog_options(FETDialogOptions& options)
{
    if (!g_fet_dialog_options_initialized)
    {
        _fet_default_dialog_options(g_fet_dialog_options);
        g_fet_dialog_options_initialized = true;
    }
    options = g_fet_dialog_options;
}

static void _fet_default_device_options(FETOptions& options)
{
    options.L_um = 1.0;
    options.W_um = 1.0;
    options.Cox_F_cm2 = 1.15e-8;
    options.coxMode = FET_COX_HFOX;
    options.oxideThickness_nm = 10.0;
    options.oxideKappa = 20.0;
    options.Vd_V = 1.0;
}

static double _fet_kappa_for_cox_mode(int mode, double manualKappa)
{
    if (mode == FET_COX_HFOX)
        return 20.0;
    if (mode == FET_COX_ALOX)
        return 9.0;
    if (mode == FET_COX_SIOX)
        return 3.9;
    return manualKappa;
}

static double _fet_effective_cox(const FETOptions& options)
{
    if (options.coxMode == FET_COX_DIRECT)
        return options.Cox_F_cm2;
    double kappa = _fet_kappa_for_cox_mode(options.coxMode,
                                           options.oxideKappa);
    if (kappa <= 0 || options.oxideThickness_nm <= 0)
        return NANUM;
    return 8.854187817e-14 * kappa / (options.oxideThickness_nm * 1.0e-7);
}

static int _fet_rgb_color(int red, int green, int blue)
{
    return RGB2OCOLOR(RGB(red, green, blue));
}

static int _fet_blend_color(int color, int target, double targetWeight)
{
    COLORREF sourceRgb = okutil_convert_ocolor_to_RGB(color);
    COLORREF targetRgb = okutil_convert_ocolor_to_RGB(target);
    targetWeight = max(0.0, min(1.0, targetWeight));
    double sourceWeight = 1.0 - targetWeight;
    int red = (int)(GetRValue(sourceRgb) * sourceWeight
                  + GetRValue(targetRgb) * targetWeight + 0.5);
    int green = (int)(GetGValue(sourceRgb) * sourceWeight
                    + GetGValue(targetRgb) * targetWeight + 0.5);
    int blue = (int)(GetBValue(sourceRgb) * sourceWeight
                   + GetBValue(targetRgb) * targetWeight + 0.5);
    return _fet_rgb_color(red, green, blue);
}

// 10-color categorical palette for the statistics histogram panels, tuned to
// read like a high-impact-journal figure: muted, high-contrast hues that
// stay distinguishable when blended for histogram fills.
static int _fet_multi_curve_rgb(int index, int& red, int& green, int& blue)
{
    vector<int> reds   = {  13, 230,   0, 225, 126,  77,  60, 183,  35, 140 };
    vector<int> greens = {  93,  75, 160, 135,  47, 187,  84,  34, 139,  86 };
    vector<int> blues  = { 165,  53, 135,  39, 142, 213, 136,  48,  69,  75 };
    int ii = index % reds.GetSize();
    if (ii < 0)
        ii += reds.GetSize();
    red = reds[ii];
    green = greens[ii];
    blue = blues[ii];
    return _fet_rgb_color(red, green, blue);
}

static double _fet_current_density_factor()
{
    FETDialogOptions options;
    _fet_get_effective_dialog_options(options);
    return options.device.W_um > 0 ? 1.0e6 / options.device.W_um : 1.0e6;
}

static void _fet_message(LPCSTR lpcszMessage, UINT nFlags = MB_OK | MB_ICONINFORMATION)
{
    MessageBox(GetWindow(OGW_MAIN), lpcszMessage, FET_APP_TITLE, nFlags);
}

static bool _fet_valid_number(double value)
{
    return !is_missing_value(value);
}

// Status-bar text can't safely embed an arbitrary filename verbatim inside a
// quoted LabTalk script string (a stray quote or backslash would break the
// command), so escape both before formatting it in.
static void _fet_escape_labtalk_string(LPCSTR in, string& out)
{
    out = in;
    out.Replace("\\", "\\\\");
    out.Replace("\"", "\\\"");
}

// Lightweight ASCII progress bar written to Origin's status bar -- used for
// both multi-file CSV import and multi-curve batch analysis so long-running
// operations show visible progress without a custom dialog.
static void _fet_report_progress_status(LPCSTR phase, int current, int total,
                                        LPCSTR label)
{
    string safeLabel;
    _fet_escape_labtalk_string(label, safeLabel);

    int ticks = 20;
    int filled = 0;
    if (total > 0)
    {
        filled = (int)(ticks * 1.0 * current / total + 0.5);
        if (filled < 0) filled = 0;
        if (filled > ticks) filled = ticks;
    }
    string bar;
    int ii;
    for (ii = 0; ii < filled; ii++) bar += "#";
    for (ii = filled; ii < ticks; ii++) bar += "-";
    int pct = total > 0 ? (int)(100.0 * current / total) : 0;

    string msg;
    msg.Format("%s [%s] %d%% (%d/%d) %s", phase, bar, pct, current, total,
              safeLabel);
    string script;
    script.Format("type -s \"%s\";", msg);
    LT_execute(script);
}

static void _fet_clean_xy(const vector& vxIn, const vector& vyIn, vector& vx, vector& vy)
{
    vx.SetSize(0);
    vy.SetSize(0);
    int n = min(vxIn.GetSize(), vyIn.GetSize());
    for (int ii = 0; ii < n; ii++)
    {
        if (_fet_valid_number(vxIn[ii]) && _fet_valid_number(vyIn[ii]))
        {
            vx.Add(vxIn[ii]);
            vy.Add(vyIn[ii]);
        }
    }
}

static bool _fet_get_xy(const XYRange& range, vector& vx, vector& vy)
{
    vector vxRaw, vyRaw;
    XYRange copy(range);
    if (!copy.GetData(vyRaw, vxRaw))
        return false;
    _fet_clean_xy(vxRaw, vyRaw, vx, vy);
    return vx.GetSize() >= FET_MIN_POINTS;
}

static bool _fet_range_from_string(const string& strRange, XYRange& range)
{
    if (strRange.IsEmpty())
        return false;
    return okxf_init_range_from_string(&range, strRange) && range.IsValid();
}

static void _fet_trim_string(string& text)
{
    text.TrimLeft();
    text.TrimRight();
    text.TrimLeft('"');
    text.TrimRight('"');
    text.TrimLeft('\'');
    text.TrimRight('\'');
}

static bool _fet_starts_with_token(const string& text, LPCSTR token)
{
    vector<string> tokens;
    string temp(text);
    temp.GetTokens(tokens, ',');
    if (tokens.GetSize() < 1)
        return false;
    _fet_trim_string(tokens[0]);
    return tokens[0].CompareNoCase(token) == 0;
}

static void _fet_split_csv_line(const string& line, vector<string>& tokens)
{
    string clean(line);
    clean.TrimRight();
    tokens.SetSize(0);
    clean.GetTokens(tokens, ',');
    if (tokens.GetSize() <= 1)
        clean.GetTokens(tokens, '\t');
    if (tokens.GetSize() <= 1)
        clean.GetTokens(tokens, ';');
    for (int ii = 0; ii < tokens.GetSize(); ii++)
        _fet_trim_string(tokens[ii]);
}

static double _fet_parse_double(const vector<string>& tokens, int index)
{
    if (index < 0 || index >= tokens.GetSize())
        return NANUM;
    string value(tokens[index]);
    _fet_trim_string(value);
    if (value.IsEmpty())
        return NANUM;
    return atof(value);
}

static int _fet_find_named_column(const vector<string>& names, LPCSTR name,
                                  int startIndex = 0)
{
    for (int ii = startIndex; ii < names.GetSize(); ii++)
    {
        string token(names[ii]);
        _fet_trim_string(token);
        if (token.CompareNoCase(name) == 0)
            return ii;
    }
    return -1;
}

static int _fet_find_any_column(const vector<string>& names,
                                const vector<string>& aliases,
                                int startIndex = 0)
{
    for (int aa = 0; aa < aliases.GetSize(); aa++)
    {
        int index = _fet_find_named_column(names, aliases[aa], startIndex);
        if (index >= 0)
            return index;
    }
    return -1;
}

static int _fet_find_vg_column(const vector<string>& names, int startIndex = 0)
{
    vector<string> aliases = {
        "Vg", "Vg_V", "VG", "GateV", "Gate_V", "GateVoltage", "Gate Voltage",
        "Vgate", "Vgate_V", "Gate Voltage (V)", "Gate Voltage[V]",
        "Vg (V)", "Vg[V]", "SMU4 V", "SMU4 Voltage", "Vgs", "VGS", "vgs"
    };
    return _fet_find_any_column(names, aliases, startIndex);
}

static int _fet_find_vd_column(const vector<string>& names, int startIndex = 0)
{
    vector<string> aliases = {
        "Vd", "Vd_V", "VD", "DrainV", "Drain_V", "DrainVoltage",
        "Drain Voltage", "Vdrain", "Vdrain_V", "Drain Voltage (V)",
        "Drain Voltage[V]", "Vd (V)", "Vd[V]", "SMU2 V", "SMU2 Voltage",
        "Vds", "VDS", "vds"
    };
    return _fet_find_any_column(names, aliases, startIndex);
}

static int _fet_find_ig_column(const vector<string>& names, int startIndex = 0)
{
    vector<string> aliases = {
        "Ig", "Ig_A", "IG", "GateI", "Gate_I", "GateCurrent",
        "Gate Current", "Igate", "Igate_A", "Gate Current (A)",
        "Gate Current[A]", "Ig (A)", "Ig[A]", "SMU4 I", "SMU4 Current",
        "Igs", "IGS", "igs"
    };
    return _fet_find_any_column(names, aliases, startIndex);
}

static int _fet_find_id_column(const vector<string>& names, int startIndex = 0)
{
    vector<string> aliases = {
        "Id", "Id_A", "ID", "DrainI", "Drain_I", "DrainCurrent",
        "Drain Current", "Idrain", "Idrain_A", "Drain Current (A)",
        "Drain Current[A]", "Id (A)", "Id[A]", "SMU2 I", "SMU2 Current",
        "Ids", "IDS", "ids"
    };
    return _fet_find_any_column(names, aliases, startIndex);
}

// Some exports leave the drain-current column with a generic per-channel
// name (e.g. "I1") instead of "Id"/"Ids". Once Vg (and, when present, Vd/Ig)
// are identified, treat the first remaining current-like column ("I...",
// but not the source-current "Is") as Id rather than failing the import.
static int _fet_find_fallback_id_column(const vector<string>& names,
                                        int idxVg, int idxVd, int idxIg,
                                        int startIndex = 0)
{
    for (int ii = startIndex; ii < names.GetSize(); ii++)
    {
        if (ii == idxVg || ii == idxVd || ii == idxIg)
            continue;
        string token(names[ii]);
        _fet_trim_string(token);
        if (token.IsEmpty())
            continue;
        if (token.CompareNoCase("Is") == 0 || token.CompareNoCase("Is_A") == 0
            || token.CompareNoCase("Source Current") == 0)
            continue;
        char c = token.GetAt(0);
        if (c != 'I' && c != 'i')
            continue;
        return ii;
    }
    return -1;
}

static int _fet_find_absid_column(const vector<string>& names, int startIndex = 0)
{
    vector<string> aliases = {
        "absId", "absId_A", "AbsId", "AbsId_A", "Abs(Id)", "Abs Id",
        "|Id|", "absDrainCurrent", "abs(Id) (A)", "absId (A)", "absId[A]"
    };
    return _fet_find_any_column(names, aliases, startIndex);
}

// B1500 metadata lines like "Output.List.Data,Vg,Id,Ig" or
// "TestParameter, Output.List.Data, Vg, absId, Id, Ig, Vd" name the very
// same Vg/Id columns the real data header does, just prefixed by one or two
// namespaced ("Foo.Bar") descriptive keys. If such a line were accepted as
// the data header, every subsequent column index would be off by the prefix
// width, silently misreading every value (this is what a user saw: a real
// column mismatch, not a missing-alias problem). A genuine data header
// never has a dotted key token before its first recognized channel column,
// so reject any candidate whose tokens up to that point contain a ".".
static bool _fet_header_prefix_looks_like_metadata(const vector<string>& tokens,
                                                    int firstMatchedIndex)
{
    for (int ii = 0; ii < firstMatchedIndex && ii < tokens.GetSize(); ii++)
    {
        if (tokens[ii].Find(".") >= 0)
            return true;
    }
    return false;
}

static int _fet_find_directional_column(const vector<string>& names,
                                        LPCSTR direction,
                                        const vector<string>& aliases,
                                        int startIndex = 0)
{
    for (int ii = startIndex; ii < names.GetSize(); ii++)
    {
        string token(names[ii]);
        _fet_trim_string(token);
        if (token.Find(direction) < 0)
            continue;
        for (int aa = 0; aa < aliases.GetSize(); aa++)
        {
            if (token.Find(aliases[aa]) >= 0)
                return ii;
        }
    }
    return -1;
}

static int _fet_find_directional_vg_column(const vector<string>& names,
                                           LPCSTR direction,
                                           int startIndex = 0)
{
    vector<string> aliases = {
        "Vg", "VG", "GateV", "Gate Voltage", "Vgate", "SMU4 V"
    };
    return _fet_find_directional_column(names, direction, aliases, startIndex);
}

static int _fet_find_directional_id_column(const vector<string>& names,
                                           LPCSTR direction,
                                           int startIndex = 0)
{
    vector<string> aliases = {
        "Id", "ID", "DrainI", "Drain Current", "Idrain", "SMU2 I"
    };
    return _fet_find_directional_column(names, direction, aliases, startIndex);
}

static int _fet_find_directional_absid_column(const vector<string>& names,
                                              LPCSTR direction,
                                              int startIndex = 0)
{
    vector<string> aliases = {
        "absId", "AbsId", "Abs(Id)", "Abs Id", "|Id|", "absDrainCurrent"
    };
    return _fet_find_directional_column(names, direction, aliases, startIndex);
}

// B1500 exports record which channel is the primary swept variable in
// "Channel.VName"/"Channel.Func" metadata lines (as either
// "TestParameter, Channel.VName, Vg, Vd, Vs" or the "Classic" layout's bare
// "Channel.VName,Vd,Vs,Vg"). Capture whichever of the two tags a meta line
// carries so a later block can check whether Vg is really VAR1 (a transfer
// curve) rather than a CONST/VAR2 bias column of an Output (Id-Vd) curve
// that merely happens to also report Vg.
static bool _fet_capture_channel_role_line(const vector<string>& tokens,
                                           LPCSTR key, vector<string>& outValues)
{
    int keyIndex = -1;
    if (tokens.GetSize() > 0 && tokens[0].CompareNoCase(key) == 0)
        keyIndex = 0;
    else if (tokens.GetSize() > 1 && tokens[1].CompareNoCase(key) == 0)
        keyIndex = 1;
    if (keyIndex < 0)
        return false;

    outValues.SetSize(0);
    for (int ii = keyIndex + 1; ii < tokens.GetSize(); ii++)
    {
        string value(tokens[ii]);
        _fet_trim_string(value);
        outValues.Add(value);
    }
    return true;
}

// Built-in Keysight "ApplicationTest" exports (e.g. "Trans.", "DaulPolarOutput")
// don't carry Channel.VName/Channel.Func at all -- instead they record the
// graphed sweep axis as "AnalysisSetup, Analysis.Setup.Vector.Graph.XAxis.Name, Vg"
// (or "...Name, Vd" for an Output curve). Capture that single value the same
// way, as a second, independent signal for _fet_vg_sweep_status.
static bool _fet_capture_single_value_line(const vector<string>& tokens,
                                           LPCSTR key, string& outValue)
{
    int keyIndex = -1;
    if (tokens.GetSize() > 0 && tokens[0].CompareNoCase(key) == 0)
        keyIndex = 0;
    else if (tokens.GetSize() > 1 && tokens[1].CompareNoCase(key) == 0)
        keyIndex = 1;
    if (keyIndex < 0 || keyIndex + 1 >= tokens.GetSize())
        return false;
    outValue = tokens[keyIndex + 1];
    _fet_trim_string(outValue);
    return true;
}

// Returns 1 if Vg is confirmed as the primary swept channel -- a transfer
// curve; 0 if Vg is confirmed present but is not the swept channel -- an
// Output/Id-Vd curve, not a transfer curve; or -1 if neither role signal
// could be read, in which case callers should not filter and should fall
// back to the old "any recognized Vg/Id columns" behavior. Prefers
// Channel.VName/Channel.Func (VAR1 check); falls back to the
// ApplicationTest XAxis.Name when that metadata isn't present.
static int _fet_vg_sweep_status(const vector<string>& channelVNames,
                                const vector<string>& channelFuncs,
                                const string& xAxisName)
{
    if (channelVNames.GetSize() > 0
        && channelVNames.GetSize() == channelFuncs.GetSize())
    {
        int idx = _fet_find_vg_column(channelVNames, 0);
        if (idx >= 0)
        {
            string func(channelFuncs[idx]);
            _fet_trim_string(func);
            return func.CompareNoCase("VAR1") == 0 ? 1 : 0;
        }
    }
    if (!xAxisName.IsEmpty())
    {
        vector<string> single;
        single.Add(xAxisName);
        return _fet_find_vg_column(single, 0) >= 0 ? 1 : 0;
    }
    return -1;
}

static void _fet_import_error_text(int code, string& text)
{
    if (code == 0)
        text = "no Vg/Id data rows were found";
    else if (code == -1)
        text = "file could not be opened";
    else if (code == -2)
        text = "DataName/header row did not contain recognized Vg and Id columns";
    else if (code == -3)
        text = "Vg is not the swept variable (this looks like an Output/Id-Vd "
               "curve, not a transfer curve) -- skipped";
    else
        text.Format("error %d", code);
}

static bool _fet_prepare_import_context(FETImportContext& ctx, bool makeGraph,
                                        bool multiStyle = false)
{
    WorksheetPage page;
    page.Create("Origin");
    page.SetName(multiStyle ? "FETMultiData" : "FETImportedData",
                 OCD_ENUM_NEXT);
    ctx.curves = page.Layers(0);
    ctx.curves.SetName("Curves");
    ctx.curves.SetSize(0, 0);

    int metaIndex = page.AddLayer("RawMeta");
    ctx.meta = page.Layers(metaIndex);
    ctx.meta.SetSize(0, 3);
    ctx.meta.Columns(0).SetLongName("File");
    ctx.meta.Columns(1).SetLongName("Line");
    ctx.meta.Columns(2).SetLongName("Text");

    ctx.curveCount = 0;
    ctx.metaRow = 0;
    ctx.maxRows = 0;
    ctx.makeGraph = makeGraph;
    ctx.multiStyle = multiStyle;
    ctx.colsPerCurve = multiStyle ? FET_IMPORT_COLS_PER_CURVE_COMPACT
                                  : FET_IMPORT_COLS_PER_CURVE;

    if (makeGraph)
    {
        GraphPage gp;
        gp.Create("DOUBLEY");
        gp.SetName(multiStyle ? "FETMultiGraph" : "FETImportedGraph",
                   OCD_ENUM_NEXT);
        ctx.graphLayer = gp.Layers(0);
        ctx.linearGraphLayer = gp.Layers(1);
    }
    return ctx.curves && ctx.meta;
}

static void _fet_append_import_meta(FETImportContext& ctx, LPCSTR fileName,
                                    int lineNumber, LPCSTR text)
{
    ctx.meta.SetCell(ctx.metaRow, 0, GetFileName(fileName, false));
    ctx.meta.SetCell(ctx.metaRow, 1, lineNumber);
    ctx.meta.SetCell(ctx.metaRow, 2, text);
    ctx.metaRow++;
}

static void _fet_set_import_column_labels(Worksheet& wks, int baseCol,
                                          int colsPerCurve, LPCSTR label)
{
    vector<string> names = { "Vg", "Id", "Ig", "absId", "Vd", "logAbsId" };
    vector<string> units = { "V", "A", "A", "A", "V", "log10(A)" };
    for (int ii = 0; ii < colsPerCurve; ii++)
    {
        Column col = wks.Columns(baseCol + ii);
        col.SetLongName(names[ii]);
        col.SetUnits(units[ii]);
        col.SetComments(label);
    }
    wks.Columns(baseCol).SetType(OKDATAOBJ_DESIGNATION_X);
    wks.Columns(baseCol + 1).SetType(OKDATAOBJ_DESIGNATION_Y);
}

static bool _fet_set_left_tick_formula(GraphLayer& gl, LPCSTR formula)
{
    if (!gl)
        return false;
    Axis axis = gl.YAxis;
    Tree format;
    format = axis.GetFormat(FPB_ALL, FOB_AXIS_LABELS, true, true);
    TreeNode labels = tree_get_node_by_tagname(format, "LeftLabels", true);
    if (!labels)
        return false;
    tree_check_get_node(labels, "Formula").strVal = formula;
    if (0 != axis.UpdateThemeIDs(format.Root, "Error", "Unknown tag"))
        return false;
    return axis.ApplyFormat(format, true, true, true);
}

static bool _fet_set_right_tick_formula(GraphLayer& gl, LPCSTR formula)
{
    if (!gl)
        return false;
    Axis axis = gl.YAxis;
    Tree format;
    format = axis.GetFormat(FPB_ALL, FOB_AXIS_LABELS, true, true);
    TreeNode labels = tree_get_node_by_tagname(format, "RightLabels", true);
    if (!labels)
        return false;
    tree_check_get_node(labels, "Formula").strVal = formula;
    if (0 != axis.UpdateThemeIDs(format.Root, "Error", "Unknown tag"))
        return false;
    return axis.ApplyFormat(format, true, true, true);
}

// page.width/page.height are in 1/600 inch (4560/600 = 7.6in, the original
// single-curve graph size) -- independent of the Resize() call above, which
// only sets an initial GDI size hint before this LabTalk aspect-lock takes
// over. The two must be changed together, or a Resize() alone silently has
// no visible effect on the final page size.
static void _fet_set_graph_page_ratio(GraphLayer& gl, int sizeUnits = 420,
                                      int pageWidthUnits = 4560)
{
    if (!gl)
        return;
    GraphPage gp = gl.GetPage();
    if (gp)
        gp.Resize(sizeUnits, sizeUnits, 101);
    string script;
    script.Format("page.kar=0;page.width=%d;page.height=%d;"
                  "layer.unit=0;layer.left=14;layer.top=10;layer.width=76;layer.height=76;",
                  pageWidthUnits, pageWidthUnits);
    gl.LT_execute(script);
    gl.LT_execute("page.zoomWhole=1;");
}

static void _fet_configure_transfer_graph(GraphLayer& gl)
{
    if (!gl)
        return;

    set_active_layer(gl);
    string script;
    double factor = _fet_current_density_factor();
    string formula;
    formula.Format("x+%.15g", log10(factor));
    script  = "layer.y.type=0;";
    script += "layer.y.showAxes=1;";
    script += "layer.y.showLabels=1;";
    script += "layer.x.type=0;";
    script += "layer.y.grid.show=1;";
    script += "layer.x.grid.show=1;";
    script += "layer.y.label.type=1;";
    script += "layer.y.label.numFormat=1;";
    script += "layer.y.label.decPlaces=0;";
    script += "layer.y.label.pre$=\"10\\+(\";";
    script += "layer.y.label.suf$=\")\";";
    script += "label -yl \"|\\i(I)\\-(d)| (uA/um)\";";
    script += "label -xb \"\\i(V)\\-(g) (V)\";";
    script += "layer -a;label -r legend;";
    gl.LT_execute(script);
    _fet_set_left_tick_formula(gl, formula);
    _fet_set_graph_page_ratio(gl);
}

static void _fet_configure_linear_transfer_graph(GraphLayer& gl)
{
    if (!gl)
        return;

    set_active_layer(gl);
    string script;
    double factor = _fet_current_density_factor();
    string formula;
    formula.Format("x*%.15g", factor);
    script  = "layer.y.type=0;";
    script += "layer.x.type=0;";
    script += "layer.y.showAxes=2;";
    script += "layer.y.showLabels=2;";
    script += "layer.y.grid.show=0;";
    script += "layer.x.grid.show=0;";
    script += "layer.y2.label.type=1;";
    script += "layer.y2.label.numFormat=1;";
    script += "layer.y2.label.decPlaces=-1;";
    script += "layer.y2.label.pre$=\"\";";
    script += "layer.y2.label.suf$=\"\";";
    script += "label -yr \"\\i(I)\\-(d) (uA/um)\";";
    script += "label -xb \"\\i(V)\\-(g) (V)\";";
    script += "layer -a;layer.y.from=0;label -r legend;";
    gl.LT_execute(script);
    _fet_set_right_tick_formula(gl, formula);
    _fet_set_graph_page_ratio(gl);
}

static void _fet_set_y_axis_color(GraphLayer& gl, bool rightAxis, int color)
{
    if (!gl)
        return;
    AxisObject axisObject = gl.YAxis.AxisObjects(
        rightAxis ? AXISOBJPOS_AXIS_SECOND : AXISOBJPOS_AXIS_FIRST);
    if (!axisObject)
        return;

    Tree format;
    TreeNode global = tree_check_get_node(format.Root, "Global");
    tree_check_get_node(global, "Color").nVal = color;
    if (0 == axisObject.UpdateThemeIDs(format.Root))
        axisObject.ApplyFormat(format, true, false);

    string script;
    if (rightAxis)
        script.Format("layer.y2.label.color=%d;YR.color=%d;", color, color);
    else
        script.Format("layer.y.label.color=%d;YL.color=%d;", color, color);
    gl.LT_execute(script);
}

static void _fet_style_data_curves(GraphLayer& gl, int color)
{
    if (!gl)
        return;
    for (int ii = 0; ii < gl.DataPlots.Count(); ii++)
    {
        DataPlot plot = gl.DataPlots(ii);
        if (plot && plot.IsShow() && !_fet_is_helper_plot(plot))
            _fet_style_plot(plot, color, false);
    }
}

static int _fet_get_layer_y_type(GraphLayer& gl)
{
    if (!gl)
        return -1;
    set_active_layer(gl);
    LT_execute("__FET_AXIS_TYPE=layer.y.type;");
    double axisType = -1;
    if (!LT_get_var("__FET_AXIS_TYPE", &axisType))
        return -1;
    return (int)axisType;
}

static bool _fet_is_plot_from_page(DataPlot& plot, LPCSTR pageName)
{
    if (!plot)
        return false;
    string dataset = plot.GetDatasetName();
    return dataset.Find(pageName) >= 0;
}

static bool _fet_is_output_plot(DataPlot& plot)
{
    if (!plot)
        return false;

    string dataset = plot.GetDatasetName();
    return dataset.Find("SS Fit") >= 0
        || dataset.Find("Vth Fit") >= 0
        || dataset.Find("Vth Marker") >= 0
        || dataset.Find("On/Off") >= 0
        || dataset.Find("gm Marker") >= 0
        || dataset.Find("Extracted Parameters") >= 0;
}

static bool _fet_is_helper_plot(DataPlot& plot)
{
    return _fet_is_plot_from_page(plot, FET_PREVIEW_PAGE)
        || _fet_is_plot_from_page(plot, FET_FIT_PAGE)
        || (_fet_is_plot_from_page(plot, FET_GRAPH_DATA_PAGE)
            && _fet_is_output_plot(plot));
}

static void _fet_remove_plots_from_page(GraphLayer& gl, LPCSTR pageName)
{
    if (!gl)
        return;

    for (int ii = gl.DataPlots.Count() - 1; ii >= 0; ii--)
    {
        DataPlot plot = gl.DataPlots(ii);
        if (_fet_is_plot_from_page(plot, pageName))
            gl.RemovePlot(ii);
    }
}

static void _fet_remove_helper_plots(GraphLayer& gl)
{
    if (!gl)
        return;
    for (int ii = gl.DataPlots.Count() - 1; ii >= 0; ii--)
    {
        DataPlot plot = gl.DataPlots(ii);
        if (_fet_is_helper_plot(plot))
            gl.RemovePlot(ii);
    }
}

static DataPlot _fet_get_active_analysis_plot(GraphLayer& gl)
{
    DataPlot plot = gl.DataPlots();
    if (plot && !_fet_is_helper_plot(plot))
        return plot;

    for (int ii = 0; ii < gl.DataPlots.Count(); ii++)
    {
        plot = gl.DataPlots(ii);
        if (plot && !_fet_is_helper_plot(plot))
            return plot;
    }
    DataPlot emptyPlot;
    return emptyPlot;
}

static int _fet_plot_point_count(DataPlot& plot)
{
    if (!plot)
        return 0;
    XYRange input;
    plot.GetDataRange(input, 0, -1);
    vector vx, vy;
    return _fet_get_xy(input, vx, vy) ? vx.GetSize() : 0;
}

static bool _fet_plot_has_backward_scan(DataPlot& plot, int& pointCount)
{
    pointCount = 0;
    if (!plot)
        return false;
    XYRange input;
    plot.GetDataRange(input, 0, -1);
    vector vx, vy;
    if (!_fet_get_xy(input, vx, vy))
        return false;
    pointCount = vx.GetSize();
    int turnIndex = -1;
    return _fet_detect_scan_turn(vx, turnIndex);
}

static DataPlot _fet_get_analysis_plot_for_graph_layer(GraphLayer& gl)
{
    if (!gl)
    {
        DataPlot emptyPlot;
        return emptyPlot;
    }

    GraphPage gp = gl.GetPage();
    GraphLayer rightLayer;
    if (gp && gp.Layers.Count() > 1)
        rightLayer = gp.Layers(1);
    if (rightLayer && rightLayer.DataPlots.Count() > 0)
    {
        DataPlot selected = rightLayer.DataPlots();
        string selectedDataset;
        if (selected && !_fet_is_helper_plot(selected))
            selectedDataset = selected.GetDatasetName();
        DataPlot firstHiddenSource;
        DataPlot firstVisibleSource;
        DataPlot largestBackwardSource;
        DataPlot largestSameDatasetSource;
        DataPlot largestSource;
        int largestBackwardPoints = 0;
        int largestSameDatasetPoints = 0;
        int largestPoints = 0;
        vector<string> distinctDatasets;

        for (int ii = 0; ii < rightLayer.DataPlots.Count(); ii++)
        {
            DataPlot candidate = rightLayer.DataPlots(ii);
            if (!candidate || _fet_is_helper_plot(candidate))
                continue;
            int candidatePoints = 0;
            bool hasBackwardScan = _fet_plot_has_backward_scan(candidate,
                                                               candidatePoints);
            string candidateDataset = candidate.GetDatasetName();
            bool datasetKnown = false;
            for (int dd = 0; dd < distinctDatasets.GetSize(); dd++)
            {
                if (distinctDatasets[dd].CompareNoCase(candidateDataset) == 0)
                {
                    datasetKnown = true;
                    break;
                }
            }
            if (!datasetKnown)
                distinctDatasets.Add(candidateDataset);
            if (hasBackwardScan && candidatePoints > largestBackwardPoints)
            {
                largestBackwardSource = candidate;
                largestBackwardPoints = candidatePoints;
            }
            if (candidatePoints > largestPoints)
            {
                largestSource = candidate;
                largestPoints = candidatePoints;
            }
            if (!selectedDataset.IsEmpty()
                && candidateDataset.CompareNoCase(selectedDataset) == 0
                && candidatePoints > largestSameDatasetPoints)
            {
                largestSameDatasetSource = candidate;
                largestSameDatasetPoints = candidatePoints;
            }
            if (!candidate.IsShow())
            {
                // No early return here even on a dataset match: a segment
                // that scanMode has hidden (see
                // _fet_set_curve_segment_visibility) is also "hidden" but is
                // NOT the analysis source, and it can match selectedDataset
                // just as well as the true hidden source can. Let the
                // largestSameDatasetSource/largestSource checks below decide
                // instead -- they compare point counts, so the full-range
                // source (always the largest) wins over a hidden segment.
                if (!firstHiddenSource)
                    firstHiddenSource = candidate;
            }
            else if (!firstVisibleSource)
            {
                firstVisibleSource = candidate;
            }
        }
        // The plot with the most points in the layer can only be a
        // full-range hidden source -- forward/backward segments are always
        // strict subsets of it. So if that largest plot is hidden, it is
        // unambiguously the one true analysis source for its curve, no
        // matter how many differently-labeled visible segments (forward/
        // backward can have distinct dataset names from each other and from
        // the hidden source) sit alongside it, and regardless of whether
        // scanMode has also hidden the non-analyzed segment (which would
        // otherwise make more than one candidate look "hidden" here). Use
        // it directly rather than falling through to "selected"-based
        // disambiguation that doesn't apply to a single-curve layer.
        if (largestSource && !largestSource.IsShow())
            return largestSource;
        // If the layer only holds one curve, use it directly -- there is no
        // real ambiguity to resolve. This matters because Origin's "active
        // plot" for a layer (what `selected` below is based on) drifts on
        // its own as analysis adds/removes fit-line and marker plots across
        // repeated runs, so trusting it even for a single-curve graph could
        // resolve to a partial visible segment (e.g. just the forward half)
        // instead of the full hidden source, silently losing the backward
        // segment from data reorganization even though nothing about the
        // curve itself changed.
        if (distinctDatasets.GetSize() == 1)
        {
            // largestSource before firstHiddenSource: point count is a more
            // reliable "is this the full curve" signal than "is it hidden"
            // now that scanMode can hide a segment too (the case where the
            // true source itself is hidden is already handled above).
            if (largestSource)
                return largestSource;
            if (firstHiddenSource)
                return firstHiddenSource;
            if (firstVisibleSource)
                return firstVisibleSource;
        }
        // Multiple curves: curve identity (the dataset the user actually
        // selected) must always win over any curve-agnostic "largest" guess
        // below. Otherwise, on a graph with several imported curves,
        // analysis can silently lock onto a different curve's backward
        // segment just because it happens to have more points -- this
        // previously made backward extraction look "wrong" even though the
        // turn-detection math was fine.
        if (largestSameDatasetSource)
            return largestSameDatasetSource;
        if (selected && !_fet_is_helper_plot(selected))
            return selected;
        // No usable selection (e.g. the active plot was itself a helper
        // fit-line/marker) -- fall back to best-effort guesses.
        if (firstHiddenSource)
            return firstHiddenSource;
        if (largestBackwardSource)
            return largestBackwardSource;
        if (largestSource)
            return largestSource;
        if (firstVisibleSource)
            return firstVisibleSource;
    }
    return _fet_get_active_analysis_plot(gl);
}

static void _fet_attach_plot_to_axis(DataPlot& plot, int axis)
{
    if (!plot)
        return;
    plot.SetAttachedAxis(axis);
}

// Real per-plot line transparency: Origin's Line format Tree accepts a
// Transparency tag (0-100) alongside Color/Width/Style, verified via
// UpdateThemeIDs + a GetFormat round-trip. Falls back to the plain opaque
// style if that tag fails to validate on some Origin version, so a bad
// fallback never also loses the color/width that DID validate.
static void _fet_style_plot_alpha(DataPlot& plot, int color, double lineWidth,
                                  int transparencyPercent)
{
    if (!plot)
        return;
    if (transparencyPercent <= 0)
    {
        _fet_style_plot(plot, color, false, LINE_STYLE_SOLID, lineWidth);
        return;
    }
    Tree tr;
    tr = plot.GetFormat(FPB_ALL, FOB_ALL, true, true);
    tr.Root.Line.Color.nVal = color;
    tr.Root.Line.Width.dVal = lineWidth;
    tr.Root.Line.Style.nVal = LINE_STYLE_SOLID;
    tr.Root.Line.Transparency.nVal = transparencyPercent;
    if (0 == plot.UpdateThemeIDs(tr.Root))
        plot.ApplyFormat(tr, true, true);
    else
        _fet_style_plot(plot, color, false, LINE_STYLE_SOLID, lineWidth);
    plot.SetLineType(LINE_STYLE_SOLID);
}

static int _fet_add_segmented_visible_plots(GraphLayer& gl, Worksheet& wks,
                                            int xColumn, int yColumn,
                                            int rowCount, const vector& vx,
                                            int color, int axis,
                                            double forwardWidth = FET_FORWARD_LINE_WIDTH,
                                            double backwardWidth = FET_BACKWARD_LINE_WIDTH,
                                            int transparencyPercent = 0)
{
    if (!gl || !wks || rowCount < FET_MIN_POINTS)
        return 0;

    int turnIndex = -1;
    bool hasBackward = _fet_detect_scan_turn(vx, turnIndex);
    int forwardEnd = hasBackward ? turnIndex : rowCount - 1;
    DataRange forwardRange;
    forwardRange.Add(wks, xColumn, "X", xColumn, 0, forwardEnd);
    forwardRange.Add(wks, yColumn, "Y", yColumn, 0, forwardEnd);
    int forwardIndex = gl.AddPlot(forwardRange, IDM_PLOT_LINE);
    DataPlot forwardPlot = gl.DataPlots(forwardIndex);
    _fet_style_plot_alpha(forwardPlot, color, forwardWidth, transparencyPercent);
    _fet_attach_plot_to_axis(forwardPlot, axis);
    int added = forwardPlot ? 1 : 0;

    if (hasBackward)
    {
        DataRange backwardRange;
        backwardRange.Add(wks, xColumn, "X", xColumn, turnIndex, rowCount - 1);
        backwardRange.Add(wks, yColumn, "Y", yColumn, turnIndex, rowCount - 1);
        int backwardIndex = gl.AddPlot(backwardRange, IDM_PLOT_LINE);
        DataPlot backwardPlot = gl.DataPlots(backwardIndex);
        _fet_style_plot_alpha(backwardPlot, color, backwardWidth, transparencyPercent);
        _fet_attach_plot_to_axis(backwardPlot, axis);
        if (backwardPlot)
            added++;
    }
    return added;
}

static DataPlot _fet_add_hidden_source_plot(GraphLayer& gl, Worksheet& wks,
                                            int xColumn, int yColumn,
                                            int rowCount, int axis)
{
    DataPlot emptyPlot;
    if (!gl || !wks || rowCount < FET_MIN_POINTS)
        return emptyPlot;
    DataRange sourceRange;
    sourceRange.Add(wks, xColumn, "X", xColumn, 0, rowCount - 1);
    sourceRange.Add(wks, yColumn, "Y", yColumn, 0, rowCount - 1);
    int sourceIndex = gl.AddPlot(sourceRange, IDM_PLOT_LINE);
    DataPlot sourcePlot = gl.DataPlots(sourceIndex);
    _fet_attach_plot_to_axis(sourcePlot, axis);
    set_active_layer(gl);
    string hideScript;
    hideScript.Format("layer -hp 1 %d;", sourceIndex + 1);
    gl.LT_execute(hideScript);
    return sourcePlot;
}

// Forward and backward already live in their own column pairs (each
// restarting at row 0), so this just adds one plot per direction directly
// -- unlike _fet_add_segmented_visible_plots, it doesn't need to re-detect
// a turn point in a single continuous column. Using this instead of
// re-splitting the combined columns is what gives these plots distinct
// "Forward Id"/"Backward Id" labels instead of both showing up as the
// generic "Combined Id" the hidden source also uses.
static int _fet_add_forward_backward_visible_plots(GraphLayer& gl, Worksheet& wks,
                                                    int fwdXCol, int fwdYCol,
                                                    int fwdCount, int bwdXCol,
                                                    int bwdYCol, int bwdCount,
                                                    int color, int axis)
{
    int added = 0;
    if (gl && wks && fwdCount >= FET_MIN_POINTS)
    {
        DataRange forwardRange;
        forwardRange.Add(wks, fwdXCol, "X", fwdXCol, 0, fwdCount - 1);
        forwardRange.Add(wks, fwdYCol, "Y", fwdYCol, 0, fwdCount - 1);
        int forwardIndex = gl.AddPlot(forwardRange, IDM_PLOT_LINE);
        DataPlot forwardPlot = gl.DataPlots(forwardIndex);
        _fet_style_plot(forwardPlot, color, false, LINE_STYLE_SOLID,
                       FET_FORWARD_LINE_WIDTH);
        _fet_attach_plot_to_axis(forwardPlot, axis);
        if (forwardPlot)
            added++;
    }
    if (gl && wks && bwdCount >= FET_MIN_POINTS)
    {
        DataRange backwardRange;
        backwardRange.Add(wks, bwdXCol, "X", bwdXCol, 0, bwdCount - 1);
        backwardRange.Add(wks, bwdYCol, "Y", bwdYCol, 0, bwdCount - 1);
        int backwardIndex = gl.AddPlot(backwardRange, IDM_PLOT_LINE);
        DataPlot backwardPlot = gl.DataPlots(backwardIndex);
        _fet_style_plot(backwardPlot, color, false, LINE_STYLE_SOLID,
                       FET_BACKWARD_LINE_WIDTH);
        _fet_attach_plot_to_axis(backwardPlot, axis);
        if (backwardPlot)
            added++;
    }
    return added;
}

static bool _fet_create_doubley_graph_from_plot(DataPlot& sourcePlot,
                                                GraphLayer& outLeftLayer)
{
    if (!sourcePlot)
        return false;

    XYRange input;
    sourcePlot.GetDataRange(input, 0, -1);
    vector vx, vy;
    if (!_fet_get_xy(input, vx, vy))
        return false;

    // Use the same "Curves" sheet and column layout that every later
    // fet_analyzer_refresh_preview() call writes via
    // _fet_write_graph_curves_data(), instead of a separate bespoke layout
    // on the same sheet. The two used to disagree about what each column
    // meant, so whichever one ran most recently silently rewrote the
    // other's columns out from under the plots still bound to them.
    int turnIndex = -1;
    bool hasBackward = _fet_detect_scan_turn(vx, turnIndex);
    if (!_fet_write_graph_curves_data(vx, vy, turnIndex, hasBackward))
        return false;
    WorksheetPage page(FET_GRAPH_DATA_PAGE);
    Worksheet wks;
    if (page)
        wks = page.Layers("Curves");
    if (!wks)
        return false;

    GraphPage gp;
    gp.Create("DOUBLEY");
    gp.SetName("FETAnalysisGraph", OCD_ENUM_NEXT);
    GraphLayer leftLayer = gp.Layers(0);
    GraphLayer rightLayer = gp.Layers(1);
    if (!leftLayer || !rightLayer)
        return false;

    int forwardCount = hasBackward ? turnIndex + 1 : vx.GetSize();
    int backwardCount = hasBackward ? vx.GetSize() - turnIndex : 0;

    FETDialogOptions graphOptions;
    _fet_get_effective_dialog_options(graphOptions);
    _fet_add_forward_backward_visible_plots(leftLayer, wks, 0, 3, forwardCount,
                                            4, 7, backwardCount,
                                            graphOptions.logCurveColor,
                                            FET_AXIS_LEFT);
    _fet_add_forward_backward_visible_plots(rightLayer, wks, 0, 1, forwardCount,
                                            4, 5, backwardCount,
                                            graphOptions.linearCurveColor,
                                            FET_AXIS_RIGHT);
    _fet_add_hidden_source_plot(rightLayer, wks, 8, 9, vx.GetSize(),
                                FET_AXIS_RIGHT);

    _fet_configure_transfer_graph(leftLayer);
    _fet_configure_linear_transfer_graph(rightLayer);
    _fet_add_config_button(leftLayer);
    gp.SetShow(PAGE_ACTIVATE);
    set_active_layer(leftLayer);
    outLeftLayer = leftLayer;
    return true;
}

static bool _fet_rebuild_log_layer(GraphLayer& leftLayer,
                                   GraphLayer& rightLayer)
{
    if (!leftLayer || !rightLayer || rightLayer.DataPlots.Count() < 1)
        return false;

    WorksheetPage page;
    page.Create("Origin", CREATE_HIDDEN);
    page.SetName("FETLogData", OCD_ENUM_NEXT);
    Worksheet wks = page.Layers(0);
    if (!wks)
        return false;
    wks.SetName("LogCurves");

    for (int ii = leftLayer.DataPlots.Count() - 1; ii >= 0; ii--)
        leftLayer.RemovePlot(ii);

    FETDialogOptions options;
    _fet_get_effective_dialog_options(options);
    DataPlot sourcePlot = _fet_get_analysis_plot_for_graph_layer(leftLayer);
    if (!sourcePlot)
        sourcePlot = rightLayer.DataPlots(0);
    XYRange input;
    sourcePlot.GetDataRange(input, 0, -1);
    vector vx, vy;
    if (!_fet_get_xy(input, vx, vy))
        return false;

    wks.SetSize(vx.GetSize(), 2);
    wks.Columns(0).SetType(OKDATAOBJ_DESIGNATION_X);
    wks.Columns(1).SetType(OKDATAOBJ_DESIGNATION_Y);
    wks.Columns(0).SetLongName("Vg");
    wks.Columns(1).SetLongName("logAbsId");
    wks.Columns(0).SetUnits("V");
    wks.Columns(1).SetUnits("log10(A)");
    for (int rr = 0; rr < vx.GetSize(); rr++)
    {
        double current = fabs(vy[rr]);
        wks.SetCell(rr, 0, vx[rr]);
        wks.SetCell(rr, 1, current > 0 ? log10(current) : NANUM);
    }

    return _fet_add_segmented_visible_plots(leftLayer, wks, 0, 1,
                                            vx.GetSize(), vx,
                                            options.logCurveColor,
                                            FET_AXIS_LEFT) > 0;
}

static GraphLayer _fet_prepare_graph_for_analysis(GraphLayer& activeLayer)
{
    GraphLayer analysisLayer = activeLayer;
    if (!analysisLayer)
        return analysisLayer;

    GraphPage gp = analysisLayer.GetPage();
    GraphLayer rightLayer;
    if (gp && gp.Layers.Count() > 1)
        rightLayer = gp.Layers(1);

    if (rightLayer && rightLayer.DataPlots.Count() > 0)
    {
        GraphLayer leftLayer = gp.Layers(0);
        _fet_rebuild_log_layer(leftLayer, rightLayer);
        _fet_configure_transfer_graph(leftLayer);
        _fet_configure_linear_transfer_graph(rightLayer);
        FETDialogOptions options;
        _fet_get_effective_dialog_options(options);
        _fet_style_data_curves(leftLayer, options.logCurveColor);
        _fet_style_data_curves(rightLayer, options.linearCurveColor);
        _fet_set_y_axis_color(leftLayer, false, options.logCurveColor);
        _fet_set_y_axis_color(rightLayer, true, options.linearCurveColor);
        _fet_add_config_button(leftLayer);
        set_active_layer(leftLayer);
        return leftLayer;
    }

    DataPlot sourcePlot = _fet_get_active_analysis_plot(analysisLayer);
    GraphLayer newLayer;
    if (_fet_create_doubley_graph_from_plot(sourcePlot, newLayer))
        return newLayer;
    return analysisLayer;
}

static bool _fet_flush_import_curve(FETImportContext& ctx, LPCSTR filePath,
                                    int blockIndex, const vector& vVg,
                                    const vector& vVd, const vector& vIg,
                                    const vector& vId, const vector& vAbsId)
{
    int nRows = min(vVg.GetSize(), vId.GetSize());
    if (nRows < FET_MIN_POINTS)
        return false;

    int curveIndex = ctx.curveCount;
    int baseCol = curveIndex * ctx.colsPerCurve;
    if (nRows > ctx.maxRows)
        ctx.maxRows = nRows;
    ctx.curves.SetSize(ctx.maxRows, baseCol + ctx.colsPerCurve);

    string label = GetFileName(filePath, true);
    if (blockIndex > 1)
    {
        string suffix;
        suffix.Format(" #%d", blockIndex);
        label += suffix;
    }
    _fet_set_import_column_labels(ctx.curves, baseCol, ctx.colsPerCurve, label);

    for (int rr = 0; rr < nRows; rr++)
    {
        ctx.curves.SetCell(rr, baseCol, vVg[rr]);
        ctx.curves.SetCell(rr, baseCol + 1, vId[rr]);
        if (ctx.colsPerCurve < FET_IMPORT_COLS_PER_CURVE)
            continue;
        ctx.curves.SetCell(rr, baseCol + 2, rr < vIg.GetSize() ? vIg[rr] : NANUM);
        ctx.curves.SetCell(rr, baseCol + 3, rr < vAbsId.GetSize() ? vAbsId[rr] : fabs(vId[rr]));
        ctx.curves.SetCell(rr, baseCol + 4, rr < vVd.GetSize() ? vVd[rr] : NANUM);
        double absValue = rr < vAbsId.GetSize() ? vAbsId[rr] : fabs(vId[rr]);
        ctx.curves.SetCell(rr, baseCol + 5, absValue > 0 ? log10(absValue) : NANUM);
    }

    // Compact (Vg/Id-only) imports carry no precomputed logAbsId column, so
    // there is nothing to feed the log-axis layer here -- only reachable if
    // a future caller asks for makeGraph with the compact layout, which the
    // current entry points never do (multi-curve analysis builds its own
    // overlay graph from derived data instead).
    if (ctx.makeGraph && ctx.graphLayer && ctx.linearGraphLayer
        && ctx.colsPerCurve >= FET_IMPORT_COLS_PER_CURVE)
    {
        FETDialogOptions graphOptions;
        _fet_get_effective_dialog_options(graphOptions);
        _fet_add_segmented_visible_plots(ctx.graphLayer, ctx.curves,
                                         baseCol, baseCol + 5, nRows, vVg,
                                         graphOptions.logCurveColor,
                                         FET_AXIS_LEFT);
        _fet_add_segmented_visible_plots(ctx.linearGraphLayer, ctx.curves,
                                         baseCol, baseCol + 1, nRows, vVg,
                                         graphOptions.linearCurveColor,
                                         FET_AXIS_RIGHT);
        _fet_add_hidden_source_plot(ctx.linearGraphLayer, ctx.curves,
                                    baseCol, baseCol + 1, nRows,
                                    FET_AXIS_RIGHT);
    }

    ctx.curveCount++;
    return true;
}

static bool _fet_flush_import_scan_block(FETImportContext& ctx,
                                         LPCSTR filePath,
                                         int blockIndex,
                                         const vector& vVg,
                                         const vector& vVd,
                                         const vector& vIg,
                                         const vector& vId,
                                         const vector& vAbsId,
                                         const vector& vBwdVg,
                                         const vector& vBwdVd,
                                         const vector& vBwdIg,
                                         const vector& vBwdId,
                                         const vector& vBwdAbsId)
{
    vector allVg, allVd, allIg, allId, allAbsId;
    int ii;
    for (ii = 0; ii < vVg.GetSize() && ii < vId.GetSize(); ii++)
    {
        allVg.Add(vVg[ii]);
        allId.Add(vId[ii]);
        allIg.Add(ii < vIg.GetSize() ? vIg[ii] : NANUM);
        allVd.Add(ii < vVd.GetSize() ? vVd[ii] : NANUM);
        allAbsId.Add(ii < vAbsId.GetSize() ? vAbsId[ii] : fabs(vId[ii]));
    }
    for (ii = 0; ii < vBwdVg.GetSize() && ii < vBwdId.GetSize(); ii++)
    {
        allVg.Add(vBwdVg[ii]);
        allId.Add(vBwdId[ii]);
        allIg.Add(ii < vBwdIg.GetSize() ? vBwdIg[ii] : NANUM);
        allVd.Add(ii < vBwdVd.GetSize() ? vBwdVd[ii] : NANUM);
        allAbsId.Add(ii < vBwdAbsId.GetSize() ? vBwdAbsId[ii] : fabs(vBwdId[ii]));
    }
    return _fet_flush_import_curve(ctx, filePath, blockIndex,
                                   allVg, allVd, allIg, allId, allAbsId);
}

static int _fet_import_one_csv(FETImportContext& ctx, LPCSTR filePath)
{
    file inputFile(filePath, file::modeRead);
    if (!inputFile.IsOpen())
        return -1;

    DWORD fileSize = inputFile.GetLength();
    if (fileSize <= 0)
    {
        inputFile.Close();
        return 0;
    }

    string contents;
    char* buffer = contents.GetBufferSetLength(fileSize);
    if (!buffer)
    {
        inputFile.Close();
        return 0;
    }
    inputFile.Read(buffer, fileSize);
    contents.ReleaseBuffer();
    inputFile.Close();

    vector<string> lines;
    contents.GetTokens(lines, '\n');

    int lineNumber = 0;
    int blockIndex = 0;
    int curvesBefore = ctx.curveCount;
    bool inData = false;
    bool requireDataValue = false;
    int idxVg = -1, idxVd = -1, idxIg = -1, idxId = -1, idxAbsId = -1;
    int idxBwdVg = -1, idxBwdId = -1, idxBwdAbsId = -1;
    bool splitScanColumns = false;
    vector vVg, vVd, vIg, vId, vAbsId;
    vector vBwdVg, vBwdVd, vBwdIg, vBwdId, vBwdAbsId;
    vector<string> channelVNames, channelFuncs;
    string xAxisName;
    bool blockIsTransfer = true;
    bool anyRoleRejected = false;

    for (int ll = 0; ll < lines.GetSize(); ll++)
    {
        lineNumber++;
        string line(lines[ll]);
        line.TrimRight();
        if (line.IsEmpty())
            continue;

        if (_fet_starts_with_token(line, "DataName"))
        {
            if (inData)
            {
                if (blockIsTransfer)
                    _fet_flush_import_scan_block(ctx, filePath, blockIndex,
                                                 vVg, vVd, vIg, vId, vAbsId,
                                                 vBwdVg, vBwdVd, vBwdIg,
                                                 vBwdId, vBwdAbsId);
                vVg.SetSize(0); vVd.SetSize(0); vIg.SetSize(0);
                vId.SetSize(0); vAbsId.SetSize(0);
                vBwdVg.SetSize(0); vBwdVd.SetSize(0); vBwdIg.SetSize(0);
                vBwdId.SetSize(0); vBwdAbsId.SetSize(0);
            }

            vector<string> names;
            _fet_split_csv_line(line, names);
            int idxFwdVg = _fet_find_directional_vg_column(names, "Forward", 1);
            int idxFwdId = _fet_find_directional_id_column(names, "Forward", 1);
            idxBwdVg = _fet_find_directional_vg_column(names, "Backward", 1);
            idxBwdId = _fet_find_directional_id_column(names, "Backward", 1);
            idxBwdAbsId = _fet_find_directional_absid_column(names, "Backward", 1);
            splitScanColumns = idxFwdVg >= 0 && idxFwdId >= 0
                            && idxBwdVg >= 0 && idxBwdId >= 0;
            idxVg = _fet_find_vg_column(names, 1);
            idxVd = _fet_find_vd_column(names, 1);
            idxIg = _fet_find_ig_column(names, 1);
            idxId = _fet_find_id_column(names, 1);
            idxAbsId = _fet_find_absid_column(names, 1);
            if (idxId < 0 && idxVg >= 0)
                idxId = _fet_find_fallback_id_column(names, idxVg, idxVd, idxIg, 1);
            if (splitScanColumns)
            {
                idxVg = idxFwdVg;
                idxId = idxFwdId;
                idxAbsId = _fet_find_directional_absid_column(names, "Forward", 1);
            }
            if (idxVg < 0 || idxId < 0)
            {
                return -2;
            }
            int vgSweep = _fet_vg_sweep_status(channelVNames, channelFuncs, xAxisName);
            blockIsTransfer = (vgSweep != 0);
            if (!blockIsTransfer)
                anyRoleRejected = true;
            blockIndex++;
            inData = true;
            requireDataValue = true;
            continue;
        }

        if (!inData)
        {
            vector<string> headerTokens;
            _fet_split_csv_line(line, headerTokens);

            vector<string> roleCapture;
            if (_fet_capture_channel_role_line(headerTokens, "Channel.VName", roleCapture))
                channelVNames = roleCapture;
            if (_fet_capture_channel_role_line(headerTokens, "Channel.Func", roleCapture))
                channelFuncs = roleCapture;
            string xAxisCapture;
            if (_fet_capture_single_value_line(headerTokens,
                    "Analysis.Setup.Vector.Graph.XAxis.Name", xAxisCapture))
                xAxisName = xAxisCapture;

            int idxFwdVg = _fet_find_directional_vg_column(headerTokens, "Forward");
            int idxFwdId = _fet_find_directional_id_column(headerTokens, "Forward");
            idxBwdVg = _fet_find_directional_vg_column(headerTokens, "Backward");
            idxBwdId = _fet_find_directional_id_column(headerTokens, "Backward");
            idxBwdAbsId = _fet_find_directional_absid_column(headerTokens, "Backward");
            splitScanColumns = idxFwdVg >= 0 && idxFwdId >= 0
                            && idxBwdVg >= 0 && idxBwdId >= 0;
            idxVg = _fet_find_vg_column(headerTokens);
            idxVd = _fet_find_vd_column(headerTokens);
            idxIg = _fet_find_ig_column(headerTokens);
            idxId = _fet_find_id_column(headerTokens);
            idxAbsId = _fet_find_absid_column(headerTokens);
            if (idxId < 0 && idxVg >= 0)
                idxId = _fet_find_fallback_id_column(headerTokens, idxVg, idxVd, idxIg);
            if (splitScanColumns)
            {
                idxVg = idxFwdVg;
                idxId = idxFwdId;
                idxAbsId = _fet_find_directional_absid_column(headerTokens, "Forward");
            }
            int firstMatchedIndex = idxVg;
            if (idxId >= 0 && (firstMatchedIndex < 0 || idxId < firstMatchedIndex))
                firstMatchedIndex = idxId;
            bool looksLikeMetadataLine = firstMatchedIndex >= 0
                && _fet_header_prefix_looks_like_metadata(headerTokens, firstMatchedIndex);

            if (idxVg >= 0 && idxId >= 0 && !looksLikeMetadataLine)
            {
                int vgSweep = _fet_vg_sweep_status(channelVNames, channelFuncs, xAxisName);
                blockIsTransfer = (vgSweep != 0);
                if (!blockIsTransfer)
                    anyRoleRejected = true;
                blockIndex++;
                inData = true;
                requireDataValue = false;
                continue;
            }

            _fet_append_import_meta(ctx, filePath, lineNumber, line);
            continue;
        }

        if (blockIsTransfer
            && (!requireDataValue || _fet_starts_with_token(line, "DataValue")))
        {
            vector<string> tokens;
            _fet_split_csv_line(line, tokens);
            if (splitScanColumns)
            {
                double fwdVg = _fet_parse_double(tokens, idxVg);
                double fwdId = _fet_parse_double(tokens, idxId);
                if (_fet_valid_number(fwdVg) && _fet_valid_number(fwdId))
                {
                    double vd = _fet_parse_double(tokens, idxVd);
                    double ig = _fet_parse_double(tokens, idxIg);
                    double absId = _fet_parse_double(tokens, idxAbsId);
                    if (!_fet_valid_number(absId))
                        absId = fabs(fwdId);
                    vVg.Add(fwdVg);
                    vId.Add(fwdId);
                    vIg.Add(ig);
                    vAbsId.Add(absId);
                    vVd.Add(vd);
                }

                double bwdVg = _fet_parse_double(tokens, idxBwdVg);
                double bwdId = _fet_parse_double(tokens, idxBwdId);
                if (_fet_valid_number(bwdVg) && _fet_valid_number(bwdId))
                {
                    double bwdAbsId = _fet_parse_double(tokens, idxBwdAbsId);
                    if (!_fet_valid_number(bwdAbsId))
                        bwdAbsId = fabs(bwdId);
                    vBwdVg.Add(bwdVg);
                    vBwdId.Add(bwdId);
                    vBwdIg.Add(NANUM);
                    vBwdAbsId.Add(bwdAbsId);
                    vBwdVd.Add(_fet_parse_double(tokens, idxVd));
                }
                continue;
            }

            double vg = _fet_parse_double(tokens, idxVg);
            double id = _fet_parse_double(tokens, idxId);
            if (!_fet_valid_number(vg) || !_fet_valid_number(id))
                continue;

            double vd = _fet_parse_double(tokens, idxVd);
            double ig = _fet_parse_double(tokens, idxIg);
            double absId = _fet_parse_double(tokens, idxAbsId);
            if (!_fet_valid_number(absId))
                absId = fabs(id);

            vVg.Add(vg);
            vId.Add(id);
            vIg.Add(ig);
            vAbsId.Add(absId);
            vVd.Add(vd);
        }
    }

    if (inData && blockIsTransfer)
        _fet_flush_import_scan_block(ctx, filePath, blockIndex,
                                     vVg, vVd, vIg, vId, vAbsId,
                                     vBwdVg, vBwdVd, vBwdIg,
                                     vBwdId, vBwdAbsId);

    int producedCount = ctx.curveCount - curvesBefore;
    if (producedCount <= 0 && anyRoleRejected)
        return -3;
    return producedCount;
}

int fet_import_transfer_csv_files_ex(LPCSTR lpcszFiles, TreeNode& resultTree,
                                     bool makeGraph, bool multiStyle)
{
    string files(lpcszFiles);
    if (files.IsEmpty())
        return 1;

    vector<string> paths;
    files.GetTokens(paths, '|');
    if (paths.GetSize() < 1)
        return 1;

    FETImportContext ctx;
    if (!_fet_prepare_import_context(ctx, makeGraph, multiStyle))
        return 2;

    int failed = 0;
    string failedDetails;
    progressBox pb("Importing FET transfer CSV files...");
    pb.SetRange(0, paths.GetSize());
    for (int ii = 0; ii < paths.GetSize(); ii++)
    {
        _fet_trim_string(paths[ii]);
        if (paths[ii].IsEmpty())
            continue;
        if (!pb.Set(ii))
            break;  // user clicked Cancel -- keep whatever imported so far
        _fet_report_progress_status("Importing", ii + 1, paths.GetSize(),
                                    GetFileName(paths[ii], true));
        int imported = _fet_import_one_csv(ctx, paths[ii]);
        if (imported <= 0)
        {
            failed++;
            string detail;
            string importError;
            _fet_import_error_text(imported, importError);
            detail.Format("%s: %s", paths[ii], importError);
            if (!failedDetails.IsEmpty())
                failedDetails += "\n";
            failedDetails += detail;
        }
    }
    pb.Set(paths.GetSize());
    LT_execute("type -s \"Import complete.\";");

    if (ctx.makeGraph && ctx.graphLayer)
    {
        _fet_configure_transfer_graph(ctx.graphLayer);
        _fet_configure_linear_transfer_graph(ctx.linearGraphLayer);
        _fet_add_config_button(ctx.graphLayer);
        ctx.graphLayer.GetPage().Refresh();
    }
    ctx.curves.AutoSize();
    ctx.meta.AutoSize();

    resultTree.Curves.nVal = ctx.curveCount;
    resultTree.FailedFiles.nVal = failed;
    resultTree.FailedDetails.strVal = failedDetails;
    resultTree.Workbook.strVal = ctx.curves.GetPage().GetName();
    resultTree.Graph.strVal = "";
    resultTree.LeftAxisType.nVal = -1;
    resultTree.RightAxisType.nVal = -1;
    if (ctx.graphLayer)
    {
        resultTree.Graph.strVal = ctx.graphLayer.GetPage().GetName();
        resultTree.LeftAxisType.nVal = _fet_get_layer_y_type(ctx.graphLayer);
        resultTree.RightAxisType.nVal = _fet_get_layer_y_type(ctx.linearGraphLayer);
    }

    return ctx.curveCount > 0 ? 0 : 3;
}

int fet_import_transfer_csv_files(LPCSTR lpcszFiles, TreeNode& resultTree,
                                  bool makeGraph = true)
{
    return fet_import_transfer_csv_files_ex(lpcszFiles, resultTree,
                                            makeGraph, false);
}

static bool _fet_linear_fit(const vector& vx, const vector& vy,
                            double& slope, double& intercept, double& r2)
{
    int n = min(vx.GetSize(), vy.GetSize());
    if (n < FET_MIN_POINTS)
        return false;

    double sx = 0, sy = 0, sxx = 0, sxy = 0;
    for (int ii = 0; ii < n; ii++)
    {
        sx += vx[ii];
        sy += vy[ii];
        sxx += vx[ii] * vx[ii];
        sxy += vx[ii] * vy[ii];
    }

    double denom = n * sxx - sx * sx;
    if (fabs(denom) < 1e-30)
        return false;

    slope = (n * sxy - sx * sy) / denom;
    intercept = (sy - slope * sx) / n;

    double meanY = sy / n;
    double ssTot = 0, ssRes = 0;
    for (ii = 0; ii < n; ii++)
    {
        double fitted = slope * vx[ii] + intercept;
        ssTot += (vy[ii] - meanY) * (vy[ii] - meanY);
        ssRes += (vy[ii] - fitted) * (vy[ii] - fitted);
    }
    r2 = ssTot > 0 ? 1.0 - ssRes / ssTot : 1.0;
    return true;
}

static int _fet_odd_window(int requested, int nPoints)
{
    int window = requested;
    if (window < 1)
        window = 1;
    if (window > nPoints)
        window = nPoints;
    if (window % 2 == 0)
        window--;
    return window < 1 ? 1 : window;
}

static void _fet_smooth_vector(const vector& input, int requestedWindow,
                               vector& output)
{
    int n = input.GetSize();
    output.SetSize(n);
    int window = _fet_odd_window(requestedWindow, n);
    int half = window / 2;
    for (int ii = 0; ii < n; ii++)
    {
        int begin = max(0, ii - half);
        int end = min(n - 1, ii + half);
        double sum = 0;
        int count = 0;
        for (int jj = begin; jj <= end; jj++)
        {
            if (_fet_valid_number(input[jj]))
            {
                sum += input[jj];
                count++;
            }
        }
        output[ii] = count > 0 ? sum / count : NANUM;
    }
}

static bool _fet_extract_ss(const XYRange& ssRange, FETResult& result)
{
    vector vx, vy, smoothY, vlog;
    if (!_fet_get_xy(ssRange, vx, vy))
        return false;

    FETDialogOptions dialogOptions;
    _fet_get_effective_dialog_options(dialogOptions);
    _fet_smooth_vector(vy, dialogOptions.smoothingWindow, smoothY);

    vector vxPositive;
    for (int ii = 0; ii < smoothY.GetSize(); ii++)
    {
        double current = fabs(smoothY[ii]);
        if (current > 0)
        {
            vxPositive.Add(vx[ii]);
            vlog.Add(log10(current));
        }
    }
    if (!_fet_linear_fit(vxPositive, vlog, result.ss_slope_dec_V,
                         result.ss_intercept, result.ss_r2))
        return false;
    if (fabs(result.ss_slope_dec_V) < 1e-30)
        return false;

    result.ss_mV_dec = fabs(1000.0 / result.ss_slope_dec_V);
    return true;
}

static bool _fet_extract_vth(const XYRange& vthRange, FETResult& result)
{
    vector vx, vy, smoothY;
    if (!_fet_get_xy(vthRange, vx, vy))
        return false;
    FETDialogOptions dialogOptions;
    _fet_get_effective_dialog_options(dialogOptions);
    _fet_smooth_vector(vy, dialogOptions.smoothingWindow, smoothY);
    if (!_fet_linear_fit(vx, smoothY, result.vth_slope_A_V,
                         result.vth_intercept_A, result.vth_r2))
        return false;
    if (fabs(result.vth_slope_A_V) < 1e-30)
        return false;

    // Linear extrapolation to Id = 0. No Vd/2 correction is applied in MVP.
    result.vth_V = -result.vth_intercept_A / result.vth_slope_A_V;
    return true;
}

static bool _fet_extract_curve_metrics(const XYRange& input,
                                       const FETOptions& options,
                                       FETResult& result)
{
    vector vx, vy, smoothY;
    if (!_fet_get_xy(input, vx, vy))
        return false;

    FETDialogOptions dialogOptions;
    _fet_get_effective_dialog_options(dialogOptions);
    _fet_smooth_vector(vy, dialogOptions.smoothingWindow, smoothY);

    double largest = -1;
    int ionIndex = -1;
    int ii;
    for (ii = 0; ii < smoothY.GetSize(); ii++)
    {
        double value = fabs(smoothY[ii]);
        if (value > largest)
        {
            largest = value;
            ionIndex = ii;
        }
    }
    if (ionIndex < 0)
        return false;

    int offWindow = _fet_auto_window_size(smoothY.GetSize(), 0, 12);
    double bestOffMean = 1e300;
    int bestOffStart = -1;
    for (int start = 0; start <= smoothY.GetSize() - offWindow; start++)
    {
        double trialSum = 0;
        int trialCount = 0;
        for (int kk = start; kk < start + offWindow; kk++)
        {
            double value = fabs(smoothY[kk]);
            if (value > 0 && _fet_valid_number(value))
            {
                trialSum += value;
                trialCount++;
            }
        }
        if (trialCount > 0)
        {
            double trialMean = trialSum / trialCount;
            if (trialMean < bestOffMean)
            {
                bestOffMean = trialMean;
                bestOffStart = start;
            }
        }
    }
    if (bestOffStart < 0 || bestOffMean <= 0)
        return false;

    double offAbsSum = 0;
    double offSignedSum = 0;
    double offXSum = 0;
    int offCount = 0;
    for (ii = bestOffStart; ii < bestOffStart + offWindow; ii++)
    {
        double value = fabs(smoothY[ii]);
        if (value > 0 && _fet_valid_number(value)
            && _fet_valid_number(vx[ii]))
        {
            offAbsSum += value;
            offSignedSum += vy[ii];
            offXSum += vx[ii];
            offCount++;
        }
    }
    if (offCount <= 0)
        return false;

    result.ion_A = largest;
    result.ioff_A = offAbsSum / offCount;
    result.ion_ioff = largest / result.ioff_A;
    result.ion_Vg_V = vx[ionIndex];
    result.ioff_Vg_V = offXSum / offCount;
    result.ion_signed_A = vy[ionIndex];
    result.ioff_signed_A = offSignedSum / offCount;

    double bestAbsGm = -1;
    int gmIndex = -1;
    double bestGm = NANUM;
    for (ii = 1; ii < vx.GetSize() - 1; ii++)
    {
        double dx = vx[ii + 1] - vx[ii - 1];
        if (fabs(dx) < 1e-30)
            continue;
        double gm = (smoothY[ii + 1] - smoothY[ii - 1]) / dx;
        if (fabs(gm) > bestAbsGm)
        {
            bestAbsGm = fabs(gm);
            bestGm = gm;
            gmIndex = ii;
        }
    }
    if (gmIndex < 0)
        return false;

    result.gm_S = bestGm;
    result.gm_Vg_V = vx[gmIndex];
    result.gm_Id_A = vy[gmIndex];
    double effectiveCox = _fet_effective_cox(options);
    if (!_fet_valid_number(effectiveCox) || effectiveCox <= 0)
        return false;
    result.mobility_cm2_Vs = fabs(bestGm) * (options.L_um / options.W_um)
                           / (effectiveCox * fabs(options.Vd_V));
    result.gate_warning = "Not evaluated (Ig not supplied)";

    XYRange inputCopy(input);
    inputCopy.GetRangeString(result.source_range, NTYPE_NO_CHECK_PLOT);
    return true;
}

static int _fet_clamp_int(int value, int lower, int upper)
{
    if (value < lower)
        return lower;
    if (value > upper)
        return upper;
    return value;
}

static int _fet_auto_window_size(int nPoints, int requested, int divisor)
{
    int width = requested > 0 ? requested : nPoints / divisor;
    if (width < FET_MIN_POINTS + 2)
        width = FET_MIN_POINTS + 2;
    if (width > FET_AUTO_RANGE_MAX_POINTS)
        width = FET_AUTO_RANGE_MAX_POINTS;
    if (width > nPoints)
        width = nPoints;
    return width;
}

static bool _fet_pick_subthreshold_range(const vector& vx, const vector& vy,
                                         int requestedWidth, double minR2,
                                         int& begin, int& end)
{
    int n = min(vx.GetSize(), vy.GetSize());
    int width = _fet_auto_window_size(n, requestedWidth, 12);
    if (n < width || width < FET_MIN_POINTS)
        return false;

    double bestScore = -1e300;
    begin = 0;
    end = width - 1;
    for (int start = 0; start <= n - width; start++)
    {
        vector winX, winLog;
        double minLog = 1e300;
        double maxLog = -1e300;
        for (int ii = start; ii < start + width; ii++)
        {
            double current = fabs(vy[ii]);
            if (!_fet_valid_number(vx[ii]) || !_fet_valid_number(current)
                || current <= 0)
                continue;
            double logCurrent = log10(current);
            winX.Add(vx[ii]);
            winLog.Add(logCurrent);
            if (logCurrent < minLog) minLog = logCurrent;
            if (logCurrent > maxLog) maxLog = logCurrent;
        }

        double slope, intercept, r2;
        if (!_fet_linear_fit(winX, winLog, slope, intercept, r2))
            continue;
        if (r2 < minR2)
            continue;
        double logSpan = maxLog - minLog;
        if (logSpan < 0.25 || fabs(slope) < 1e-12)
            continue;

        double score = r2 * r2 * fabs(slope) * logSpan;
        if (score > bestScore)
        {
            bestScore = score;
            begin = start;
            end = start + width - 1;
        }
    }
    return bestScore > -1e200;
}

static bool _fet_pick_vth_range(const vector& vx, const vector& vy,
                                int ssBegin, int ssEnd, int requestedWidth,
                                double minR2, int& begin, int& end)
{
    int n = min(vx.GetSize(), vy.GetSize());
    int width = _fet_auto_window_size(n, requestedWidth, 10);
    if (n < width || width < FET_MIN_POINTS)
        return false;

    double bestScore = -1e300;
    begin = _fet_clamp_int(ssEnd + 1, 0, n - width);
    end = begin + width - 1;
    for (int start = 0; start <= n - width; start++)
    {
        vector winX, winY;
        for (int ii = start; ii < start + width; ii++)
        {
            if (_fet_valid_number(vx[ii]) && _fet_valid_number(vy[ii]))
            {
                winX.Add(vx[ii]);
                winY.Add(vy[ii]);
            }
        }

        double slope, intercept, r2;
        if (!_fet_linear_fit(winX, winY, slope, intercept, r2))
            continue;
        if (r2 < minR2)
            continue;
        if (fabs(slope) < 1e-30)
            continue;

        int trialEnd = start + width - 1;
        int overlap = min(trialEnd, ssEnd) - max(start, ssBegin) + 1;
        double overlapPenalty = overlap > 0 ? 0.25 : 1.0;
        double score = fabs(slope) * (r2 > 0 ? r2 : 0) * overlapPenalty;
        if (score > bestScore)
        {
            bestScore = score;
            begin = start;
            end = start + width - 1;
        }
    }
    return bestScore > -1e200;
}

static bool _fet_auto_pick_ranges_from_xy(const vector& vx, const vector& vy,
                                          int& ssBegin, int& ssEnd,
                                          int& vthBegin, int& vthEnd)
{
    int n = min(vx.GetSize(), vy.GetSize());
    if (n < FET_MIN_POINTS * 2)
        return false;
    FETDialogOptions dialogOptions;
    _fet_get_effective_dialog_options(dialogOptions);
    vector smoothY;
    _fet_smooth_vector(vy, dialogOptions.smoothingWindow, smoothY);
    if (!_fet_pick_subthreshold_range(vx, smoothY,
                                      dialogOptions.ssWindowPoints,
                                      dialogOptions.minFitR2,
                                      ssBegin, ssEnd))
        return false;
    if (!_fet_pick_vth_range(vx, smoothY, ssBegin, ssEnd,
                             dialogOptions.vthWindowPoints,
                             dialogOptions.minFitR2,
                             vthBegin, vthEnd))
        return false;
    return true;
}

// Detects a single V-shaped double sweep (Vg goes out to one extreme then
// back), returning the index of the extreme point as the forward/backward
// split. Direction is inferred from the sign of vx[probe]-vx[0] at ~10% into
// the curve, so it assumes the sweep starts moving consistently in one
// direction right away; it does not support more than one turn.
static bool _fet_detect_scan_turn(const vector& vx, int& turnIndex)
{
    int n = vx.GetSize();
    if (n < FET_MIN_POINTS * 2)
        return false;

    int probe = min(n - 1, max(1, n / 10));
    double initialDelta = vx[probe] - vx[0];
    if (fabs(initialDelta) < 1e-15)
        initialDelta = vx[n - 1] - vx[0];
    if (fabs(initialDelta) < 1e-15)
        return false;

    turnIndex = 0;
    double extreme = vx[0];
    for (int ii = 1; ii < n; ii++)
    {
        bool moreExtreme = initialDelta > 0 ? vx[ii] > extreme : vx[ii] < extreme;
        if (moreExtreme)
        {
            extreme = vx[ii];
            turnIndex = ii;
        }
    }

    return turnIndex >= FET_MIN_POINTS
        && n - turnIndex >= FET_MIN_POINTS;
}

static bool _fet_auto_pick_segment_indices(const vector& vx, const vector& vy,
                                           int segmentBegin, int segmentEnd,
                                           bool backward,
                                           FETRangeIndices& indices)
{
    if (segmentBegin < 0 || segmentEnd >= vx.GetSize()
        || segmentEnd - segmentBegin + 1 < FET_MIN_POINTS * 2)
        return false;

    vector segmentX, segmentY;
    for (int ii = segmentBegin; ii <= segmentEnd; ii++)
    {
        segmentX.Add(vx[ii]);
        segmentY.Add(vy[ii]);
    }

    int ssBegin, ssEnd, vthBegin, vthEnd;
    if (!_fet_auto_pick_ranges_from_xy(segmentX, segmentY, ssBegin, ssEnd,
                                       vthBegin, vthEnd))
        return false;

    indices.ssBegin = segmentBegin + ssBegin;
    indices.ssEnd = segmentBegin + ssEnd;
    indices.vthBegin = segmentBegin + vthBegin;
    indices.vthEnd = segmentBegin + vthEnd;
    indices.segmentBegin = segmentBegin;
    indices.segmentEnd = segmentEnd;
    indices.backward = backward;
    return true;
}

static bool _fet_auto_pick_indices_for_plot(DataPlot& plot,
                                            FETRangeIndices& indices)
{
    if (!plot)
        return false;

    XYRange input;
    plot.GetDataRange(input, 0, -1);
    vector vx, vy;
    if (!_fet_get_xy(input, vx, vy))
        return false;

    return _fet_auto_pick_segment_indices(vx, vy, 0, vx.GetSize() - 1,
                                          false, indices);
}

int fet_analyzer_auto_ranges_for_plot(DataPlot& plot, XYRange& ssRange,
                                      XYRange& vthRange)
{
    if (!plot)
        return 1;

    FETRangeIndices indices;
    if (!_fet_auto_pick_indices_for_plot(plot, indices))
        return 3;

    plot.GetDataRange(ssRange, indices.ssBegin, indices.ssEnd);
    plot.GetDataRange(vthRange, indices.vthBegin, indices.vthEnd);
    if (!ssRange || !vthRange)
        return 4;
    return 0;
}

int fet_analyzer_detect_scan_for_plot(DataPlot& plot, TreeNode& resultTree)
{
    if (!plot)
        return 1;
    XYRange input;
    plot.GetDataRange(input, 0, -1);
    vector vx, vy;
    if (!_fet_get_xy(input, vx, vy))
        return 2;

    int turnIndex = -1;
    bool isDouble = _fet_detect_scan_turn(vx, turnIndex);
    resultTree.IsDouble.nVal = isDouble ? 1 : 0;
    resultTree.TurnIndex.nVal = isDouble ? turnIndex : -1;
    resultTree.ForwardBegin.nVal = 0;
    resultTree.ForwardEnd.nVal = isDouble ? turnIndex : vx.GetSize() - 1;
    resultTree.BackwardBegin.nVal = isDouble ? turnIndex : -1;
    resultTree.BackwardEnd.nVal = isDouble ? vx.GetSize() - 1 : -1;
    return 0;
}

static bool _fet_range_from_x_bounds(DataPlot& plot, double x1, double x2,
                                     XYRange& range)
{
    XYRange input;
    plot.GetDataRange(input, 0, -1);
    vector vx, vy;
    if (!_fet_get_xy(input, vx, vy))
        return false;
    return _fet_range_from_x_bounds_segment(plot, x1, x2, 0,
                                            vx.GetSize() - 1, range);
}

static bool _fet_range_from_x_bounds_segment(DataPlot& plot,
                                             double x1, double x2,
                                             int segmentBegin,
                                             int segmentEnd,
                                             XYRange& range)
{
    if (!plot)
        return false;

    XYRange input;
    plot.GetDataRange(input, 0, -1);
    vector vx, vy;
    if (!_fet_get_xy(input, vx, vy))
        return false;

    double xmin = min(x1, x2);
    double xmax = max(x1, x2);
    int begin = -1;
    int end = -1;
    segmentBegin = _fet_clamp_int(segmentBegin, 0, vx.GetSize() - 1);
    segmentEnd = _fet_clamp_int(segmentEnd, segmentBegin, vx.GetSize() - 1);
    for (int ii = segmentBegin; ii <= segmentEnd; ii++)
    {
        if (vx[ii] >= xmin && vx[ii] <= xmax)
        {
            if (begin < 0)
                begin = ii;
            end = ii;
        }
    }
    if (begin < 0 || end < 0 || end - begin + 1 < FET_MIN_POINTS)
        return false;

    plot.GetDataRange(range, begin, end);
    return range.IsValid();
}

static bool _fet_get_graph_object_x(GraphLayer& gl, LPCSTR name, double& x)
{
    GraphObject go = gl.GraphObjects(name);
    if (!go)
        return false;

    string script;
    script.Format("__FET_CURSOR_X=%s.x1;", name);
    gl.LT_execute(script);
    if (!LT_get_var("__FET_CURSOR_X", &x))
        return false;
    return _fet_valid_number(x);
}

static bool _fet_get_graph_object_y(GraphLayer& gl, LPCSTR name, double& y)
{
    GraphObject go = gl.GraphObjects(name);
    if (!go)
        return false;

    string script;
    script.Format("__FET_CURSOR_Y=%s.y1;", name);
    gl.LT_execute(script);
    if (!LT_get_var("__FET_CURSOR_Y", &y))
        return false;
    return _fet_valid_number(y);
}

static bool _fet_ranges_from_free_cursors(GraphLayer& gl, DataPlot& plot,
                                          XYRange& ssRange, XYRange& vthRange)
{
    XYRange input;
    plot.GetDataRange(input, 0, -1);
    vector vx, vy;
    if (!_fet_get_xy(input, vx, vy))
        return false;
    return _fet_ranges_from_named_cursors(
        gl, plot, FET_CURSOR_SS_START, FET_CURSOR_SS_END,
        FET_CURSOR_VTH_START, FET_CURSOR_VTH_END,
        0, vx.GetSize() - 1, ssRange, vthRange);
}

static bool _fet_ranges_from_named_cursors(GraphLayer& gl, DataPlot& plot,
                                           LPCSTR ssStartName,
                                           LPCSTR ssEndName,
                                           LPCSTR vthStartName,
                                           LPCSTR vthEndName,
                                           int segmentBegin,
                                           int segmentEnd,
                                           XYRange& ssRange,
                                           XYRange& vthRange)
{
    double ssStart, ssEnd, vthStart, vthEnd;
    if (!_fet_get_graph_object_x(gl, ssStartName, ssStart)
        || !_fet_get_graph_object_x(gl, ssEndName, ssEnd)
        || !_fet_get_graph_object_x(gl, vthStartName, vthStart)
        || !_fet_get_graph_object_x(gl, vthEndName, vthEnd))
        return false;

    return _fet_range_from_x_bounds_segment(plot, ssStart, ssEnd,
                                            segmentBegin, segmentEnd, ssRange)
        && _fet_range_from_x_bounds_segment(plot, vthStart, vthEnd,
                                            segmentBegin, segmentEnd, vthRange);
}

static bool _fet_ranges_from_named_or_auto(GraphLayer& gl, DataPlot& plot,
                                           LPCSTR ssStartName,
                                           LPCSTR ssEndName,
                                           LPCSTR vthStartName,
                                           LPCSTR vthEndName,
                                           const vector& vx,
                                           const vector& vy,
                                           int segmentBegin,
                                           int segmentEnd,
                                           bool backward,
                                           XYRange& ssRange,
                                           XYRange& vthRange)
{
    if (_fet_ranges_from_named_cursors(gl, plot, ssStartName, ssEndName,
                                       vthStartName, vthEndName,
                                       segmentBegin, segmentEnd,
                                       ssRange, vthRange))
        return true;

    FETRangeIndices indices;
    if (!_fet_auto_pick_segment_indices(vx, vy, segmentBegin, segmentEnd,
                                        backward, indices))
        return false;
    plot.GetDataRange(ssRange, indices.ssBegin, indices.ssEnd);
    plot.GetDataRange(vthRange, indices.vthBegin, indices.vthEnd);
    return ssRange && vthRange;
}

static void _fet_delete_graph_object(GraphLayer& gl, LPCSTR name)
{
    GraphObject old = gl.GraphObjects(name);
    if (old)
        old.Destroy();
}

static void _fet_add_free_cursor_line(GraphLayer& gl, LPCSTR name, double x,
                                      int color,
                                      int lineStyle = LINE_STYLE_SOLID)
{
    _fet_delete_graph_object(gl, name);
    set_active_layer(gl);
    string script;
    script.Format("draw -n %s -lm -v %.15g;", name, x);
    script += name;
    script += ".HMOVE=1;";
    script += name;
    script += ".VMOVE=0;";
    string colorText;
    colorText.Format("%d", color);
    script += name;
    script += ".color=";
    script += colorText;
    script += ";";
    script += name;
    string widthText;
    widthText.Format(".linewidth=%.2f;", FET_CURSOR_WIDTH);
    script += widthText;
    script += name;
    string styleText;
    styleText.Format(".LineType=%d;", lineStyle);
    script += styleText;
    script += name;
    script += ".script$=\"fet_analyzer_refresh_preview();\";";
    script += name;
    script += ".script=2;";
    gl.LT_execute(script);
}

static void _fet_add_config_button(GraphLayer& gl)
{
    if (!gl)
        return;

    set_active_layer(gl);
    GraphObject button = gl.GraphObjects(FET_CONFIG_BUTTON);
    if (!button)
    {
        // -p is percent-of-frame with (0,0) at bottom-left, so Y needs to be
        // near 100 (not near 0) to land in the top-right corner -- the old
        // Y=12 put this in the lower-right, right on top of the curve data.
        gl.LT_execute("label -p 88 97 -j 1 -n FET_CONFIG [FET Gadget];");
        button = gl.GraphObjects(FET_CONFIG_BUTTON);
    }
    if (!button)
        return;

    button.Text = "[FET Gadget]";

    string script;
    script += FET_CONFIG_BUTTON;
    script += ".fsize=9;";
    script += FET_CONFIG_BUTTON;
    script += ".color=color(37,99,235);";
    script += FET_CONFIG_BUTTON;
    script += ".background=0;";
    script += FET_CONFIG_BUTTON;
    script += ".show=1;";
    script += FET_CONFIG_BUTTON;
    script += ".script$=\"fet_analyzer_show_settings();\";";
    script += FET_CONFIG_BUTTON;
    script += ".script=1;";
    gl.LT_execute(script);
}

// [FET Multi] button on multi-curve overlay/statistics graphs. Clicking it
// re-runs the unified multi-curve analysis (settings dialog, then batch
// extraction + overlay + histograms) using this graph's own source workbook,
// so the user never has to return to the start menu to refresh a batch.
static void _fet_add_multi_button(GraphLayer& gl)
{
    if (!gl)
        return;

    set_active_layer(gl);
    GraphObject button = gl.GraphObjects(FET_MULTI_BUTTON);
    if (!button)
    {
        gl.LT_execute("label -p 88 97 -j 1 -n FET_MULTI [FET Multi];");
        button = gl.GraphObjects(FET_MULTI_BUTTON);
    }
    if (!button)
        return;

    button.Text = "[FET Multi]";

    string script;
    script += FET_MULTI_BUTTON;
    script += ".fsize=9;";
    script += FET_MULTI_BUTTON;
    script += ".color=color(37,99,235);";
    script += FET_MULTI_BUTTON;
    script += ".background=0;";
    script += FET_MULTI_BUTTON;
    script += ".show=1;";
    script += FET_MULTI_BUTTON;
    script += ".script$=\"fet_analyzer_multi_analyze();\";";
    script += FET_MULTI_BUTTON;
    script += ".script=1;";
    gl.LT_execute(script);
}

// Records which imported Curves workbook a multi-curve graph was built
// from, as a hidden text object on that graph. The [FET Multi] re-run
// button needs to resolve this workbook when clicked from EITHER the
// overlay graph or the statistics graph -- neither plots directly from the
// original import book (the overlay plots derived OverlayCurves data, the
// statistics graph plots binned Histogram data), so dataset-name matching
// against worksheet-page names can't find it the way it does for the
// single-curve settings button. A direct tag sidesteps that entirely.
static void _fet_tag_source_book(GraphLayer& gl, LPCSTR bookName)
{
    if (!gl || !bookName || !bookName[0])
        return;
    _fet_delete_graph_object(gl, FET_SOURCE_BOOK_TAG);
    set_active_layer(gl);
    string script;
    script.Format("label -p 1 1 -n %s placeholder;", FET_SOURCE_BOOK_TAG);
    gl.LT_execute(script);
    GraphObject tag = gl.GraphObjects(FET_SOURCE_BOOK_TAG);
    if (!tag)
        return;
    tag.Text = bookName;
    string hideScript;
    hideScript.Format("%s.show=0;", FET_SOURCE_BOOK_TAG);
    gl.LT_execute(hideScript);
}

static bool _fet_read_source_book_tag(GraphLayer& gl, string& bookName)
{
    if (!gl)
        return false;
    GraphObject tag = gl.GraphObjects(FET_SOURCE_BOOK_TAG);
    if (!tag)
        return false;
    bookName = tag.Text;
    return !bookName.IsEmpty();
}

// On-graph progress readout for the batch curve-fitting loop in multi-curve
// analysis (the slow, per-curve step). Reuses the same "label -p" text-object
// pattern as every other on-graph annotation in this file, so it needs no
// unverified drawing API -- just a centered text block that gets overwritten
// each iteration and deleted once the loop finishes.
static void _fet_update_progress_label(GraphLayer& gl, LPCSTR phase,
                                       int current, int total, LPCSTR label)
{
    if (!gl)
        return;
    GraphObject existing = gl.GraphObjects(FET_MULTI_PROGRESS_LABEL);
    if (!existing)
    {
        set_active_layer(gl);
        string createScript;
        createScript.Format("label -p 50 55 -j 2 -n %s placeholder;",
                            FET_MULTI_PROGRESS_LABEL);
        gl.LT_execute(createScript);
        existing = gl.GraphObjects(FET_MULTI_PROGRESS_LABEL);
    }
    if (!existing)
        return;

    int ticks = 24;
    int filled = 0;
    if (total > 0)
    {
        filled = (int)(ticks * 1.0 * current / total + 0.5);
        if (filled < 0) filled = 0;
        if (filled > ticks) filled = ticks;
    }
    string bar;
    int ii;
    for (ii = 0; ii < filled; ii++) bar += "#";
    for (ii = filled; ii < ticks; ii++) bar += "-";
    int pct = total > 0 ? (int)(100.0 * current / total) : 0;

    string text;
    text.Format("%s\n[%s] %d%%\n(%d / %d) %s", phase, bar, pct, current,
               total, label);
    existing.Text = text;
    string style;
    style.Format("%s.fsize=11;%s.fillcolor=color(255,255,255);%s.transparency=8;",
                 FET_MULTI_PROGRESS_LABEL, FET_MULTI_PROGRESS_LABEL,
                 FET_MULTI_PROGRESS_LABEL);
    gl.LT_execute(style);
}

static void _fet_remove_progress_label(GraphLayer& gl)
{
    if (!gl)
        return;
    _fet_delete_graph_object(gl, FET_MULTI_PROGRESS_LABEL);
}

static void _fet_remove_backward_range_cursors(GraphLayer& gl)
{
    vector<string> names = {
        FET_CURSOR_BWD_SS_START, FET_CURSOR_BWD_SS_END,
        FET_CURSOR_BWD_VTH_START, FET_CURSOR_BWD_VTH_END
    };
    for (int ii = 0; ii < names.GetSize(); ii++)
        _fet_delete_graph_object(gl, names[ii]);
}

static void _fet_remove_range_cursors(GraphLayer& gl)
{
    vector<string> names = {
        FET_CURSOR_SS_START, FET_CURSOR_SS_END,
        FET_CURSOR_VTH_START, FET_CURSOR_VTH_END,
        FET_CURSOR_BWD_SS_START, FET_CURSOR_BWD_SS_END,
        FET_CURSOR_BWD_VTH_START, FET_CURSOR_BWD_VTH_END
    };
    for (int ii = 0; ii < names.GetSize(); ii++)
        _fet_delete_graph_object(gl, names[ii]);
}

static bool _fet_add_cursor_set(GraphLayer& gl, const vector& vx,
                                const FETRangeIndices& indices,
                                bool backward,
                                const FETDialogOptions& options)
{
    LPCSTR ssStart = backward ? FET_CURSOR_BWD_SS_START : FET_CURSOR_SS_START;
    LPCSTR ssEnd = backward ? FET_CURSOR_BWD_SS_END : FET_CURSOR_SS_END;
    LPCSTR vthStart = backward ? FET_CURSOR_BWD_VTH_START : FET_CURSOR_VTH_START;
    LPCSTR vthEnd = backward ? FET_CURSOR_BWD_VTH_END : FET_CURSOR_VTH_END;
    int lineStyle = backward ? LINE_STYLE_DASH_DOT : LINE_STYLE_SOLID;
    int ssCursorColor = _fet_blend_color(options.logCurveColor,
                                         SYSCOLOR_WHITE, 0.48);
    int gmCursorColor = _fet_blend_color(options.linearCurveColor,
                                         SYSCOLOR_WHITE, 0.48);
    _fet_add_free_cursor_line(gl, ssStart, vx[indices.ssBegin],
                               ssCursorColor, lineStyle);
    _fet_add_free_cursor_line(gl, ssEnd, vx[indices.ssEnd],
                               ssCursorColor, lineStyle);
    _fet_add_free_cursor_line(gl, vthStart, vx[indices.vthBegin],
                               gmCursorColor, lineStyle);
    _fet_add_free_cursor_line(gl, vthEnd, vx[indices.vthEnd],
                               gmCursorColor, lineStyle);
    return true;
}

static bool _fet_add_free_range_cursors(GraphLayer& gl, DataPlot& plot)
{
    XYRange input;
    plot.GetDataRange(input, 0, -1);
    vector vx, vy;
    if (!_fet_get_xy(input, vx, vy))
        return false;

    FETDialogOptions options;
    _fet_get_effective_dialog_options(options);
    int turnIndex = -1;
    bool hasBackward = _fet_detect_scan_turn(vx, turnIndex);
    bool useForward = options.scanMode != FET_SCAN_BACKWARD;
    bool useBackward = hasBackward
                    && (options.scanMode == FET_SCAN_AUTO
                        || options.scanMode == FET_SCAN_BACKWARD
                        || options.scanMode == FET_SCAN_BOTH);

    _fet_remove_range_cursors(gl);
    bool added = false;
    if (useForward)
    {
        int end = hasBackward ? turnIndex : vx.GetSize() - 1;
        FETRangeIndices forwardIndices;
        if (_fet_auto_pick_segment_indices(vx, vy, 0, end, false,
                                           forwardIndices))
        {
            _fet_add_cursor_set(gl, vx, forwardIndices, false, options);
            added = true;
        }
    }
    if (useBackward)
    {
        FETRangeIndices backwardIndices;
        if (_fet_auto_pick_segment_indices(vx, vy, turnIndex,
                                           vx.GetSize() - 1, true,
                                           backwardIndices))
        {
            _fet_add_cursor_set(gl, vx, backwardIndices, true, options);
            added = true;
        }
    }

    if (!added)
        return false;
    _fet_add_config_button(gl);
    fet_analyzer_refresh_preview();
    gl.GetPage().Refresh();
    return true;
}

int fet_analyzer_place_free_cursors_for_plot(DataPlot& plot)
{
    GraphLayer gl;
    gl = Project.ActiveLayer();
    if (!gl)
        return 1;
    return _fet_add_free_range_cursors(gl, plot) ? 0 : 2;
}

int fet_analyzer_ranges_from_free_cursors(DataPlot& plot, XYRange& ssRange,
                                          XYRange& vthRange)
{
    GraphLayer gl;
    gl = Project.ActiveLayer();
    if (!gl)
        return 1;
    if (!_fet_ranges_from_free_cursors(gl, plot, ssRange, vthRange))
        return 2;
    return 0;
}

static void _fet_range_x_limits(const XYRange& range, double& xmin, double& xmax,
                                double& firstSignedY)
{
    vector vx, vy;
    _fet_get_xy(range, vx, vy);
    xmin = vx[0];
    xmax = vx[0];
    firstSignedY = vy[0];
    for (int ii = 1; ii < vx.GetSize(); ii++)
    {
        if (vx[ii] < xmin) xmin = vx[ii];
        if (vx[ii] > xmax) xmax = vx[ii];
    }
}

static void _fet_style_plot(DataPlot& plot, int color, bool symbols,
                            int lineStyle, double lineWidth)
{
    if (!plot)
        return;
    if (symbols)
        plot.SetSymbol(11);
    Tree tr;
    tr = plot.GetFormat(FPB_ALL, FOB_ALL, true, true);
    if (symbols)
    {
        tr.Root.Symbol.Shape.nVal = 2;
        tr.Root.Symbol.Shape.nVal64 = 2;
        tr.Root.Symbol.Interior.nVal = 1;
        tr.Root.Symbol.Interior.nVal64 = 1;
        tr.Root.Symbol.EdgeColor.nVal = color;
        tr.Root.Symbol.EdgeColor.nVal64 = color;
        tr.Root.Symbol.FillColor.nVal = color;
        tr.Root.Symbol.FillColor.nVal64 = color;
        tr.Root.Symbol.Size.dVal = 8;
    }
    else
    {
        tr.Root.Line.Color.nVal = color;
        tr.Root.Line.Width.dVal = lineWidth;
        tr.Root.Line.Style.nVal = lineStyle;
    }
    if (0 == plot.UpdateThemeIDs(tr.Root))
        plot.ApplyFormat(tr, true, true);
    if (!symbols)
        plot.SetLineType(lineStyle);
    if (symbols)
    {
        int fillTransparency = 45;
        plot.LabTalk("paaf", &fillTransparency, true);
    }
}

static void _fet_add_vertical_line(GraphLayer& gl, LPCSTR name, double x,
                                   int color)
{
    GraphObject old = gl.GraphObjects(name);
    if (old)
        old.Destroy();

    GraphObject line = gl.CreateGraphObject(GROT_LINE, name);
    if (!line)
        return;
    line.Attach = ATTACH_TO_SCALE;
    line.X = x;
    Tree tr;
    tr.Root.Direction.nVal = 2;
    tr.Root.Span.nVal = 1;
    tr.Root.Color.nVal = color;
    tr.Root.Width.dVal = 1.5;
    if (0 == line.UpdateThemeIDs(tr.Root))
        line.ApplyFormat(tr, true, true);
}

static void _fet_add_scale_line(GraphLayer& gl, LPCSTR name,
                                double x1, double y1, double x2, double y2,
                                int color, int lineStyle,
                                bool arrowBegin = false,
                                bool arrowEnd = false,
                                bool movableY = false,
                                bool refreshOnMove = false)
{
    _fet_delete_graph_object(gl, name);
    set_active_layer(gl);
    string script;
    script.Format("draw -n %s -lm %.15g %.15g %.15g %.15g;",
                  name, x1, y1, x2, y2);
    script += name;
    script += ".ATTACH=2;";
    // "draw -lm" places the object under whatever the default attach mode
    // is at creation time, and setting .ATTACH afterward does not
    // retroactively reinterpret the coordinates it was created with under
    // the new mode -- the object silently lands somewhere else entirely.
    // Re-apply the real endpoints now that scale-attach is actually in
    // effect.
    script += name;
    string x1Text;
    x1Text.Format(".x1=%.15g;", x1);
    script += x1Text;
    script += name;
    string y1Text;
    y1Text.Format(".y1=%.15g;", y1);
    script += y1Text;
    script += name;
    string x2Text;
    x2Text.Format(".x2=%.15g;", x2);
    script += x2Text;
    script += name;
    string y2Text;
    y2Text.Format(".y2=%.15g;", y2);
    script += y2Text;
    script += name;
    string colorText;
    colorText.Format(".color=%d;", color);
    script += colorText;
    script += name;
    script += ".linewidth=1.5;";
    script += name;
    string lineTypeText;
    lineTypeText.Format(".LineType=%d;", lineStyle);
    script += lineTypeText;
    script += name;
    script += ".HMOVE=0;";
    script += name;
    script += movableY ? ".VMOVE=1;" : ".VMOVE=0;";
    script += name;
    if (refreshOnMove)
    {
        script += ".script$=\"fet_analyzer_refresh_preview();\";";
        script += name;
        script += ".script=2;";
    }
    else
    {
        script += ".script=0;";
    }
    if (arrowBegin)
    {
        script += name;
        script += ".ArrowBeginShape=1;";
    }
    if (arrowEnd)
    {
        script += name;
        script += ".ArrowEndShape=1;";
    }
    gl.LT_execute(script);
}

static void _fet_add_horizontal_cursor_line(GraphLayer& gl, LPCSTR name,
                                            double y, int color,
                                            int lineStyle,
                                            bool movableY)
{
    _fet_delete_graph_object(gl, name);
    set_active_layer(gl);
    string script;
    script.Format("draw -n %s -l -h %.15g;", name, y);
    script += name;
    script += ".ATTACH=2;";
    script += name;
    string colorText;
    colorText.Format(".color=%d;", color);
    script += colorText;
    script += name;
    script += ".linewidth=1.5;";
    script += name;
    string lineTypeText;
    lineTypeText.Format(".LineType=%d;", lineStyle);
    script += lineTypeText;
    script += name;
    script += ".HMOVE=0;";
    script += name;
    script += movableY ? ".VMOVE=1;" : ".VMOVE=0;";
    if (movableY)
    {
        // Without this, dragging the Ioff cursor (the only movable use of
        // this helper) never re-runs the analysis, so Ioff/Ion-Ioff in the
        // graph and summary sheet silently go stale after the user moves it.
        script += name;
        script += ".script$=\"fet_analyzer_refresh_preview();\";";
        script += name;
        script += ".script=2;";
    }
    else
    {
        script += name;
        script += ".script=0;";
    }
    gl.LT_execute(script);
}

static void _fet_add_scale_text(GraphLayer& gl, LPCSTR name, LPCSTR text,
                                double x, double y)
{
    _fet_delete_graph_object(gl, name);
    GraphObject label = gl.CreateGraphObject(GROT_TEXT, name);
    if (!label)
        return;
    label.Text = text;
    label.Attach = ATTACH_TO_SCALE;
    label.X = x;
    label.Y = y;
}

static void _fet_clear_graph_output(GraphLayer& leftLayer,
                                    GraphLayer& rightLayer)
{
    _fet_remove_helper_plots(leftLayer);
    _fet_remove_helper_plots(rightLayer);

    vector<string> names = {
        "FET_VTH", "FET_GM", "FET_SUMMARY",
        "FET_ON_PLUS", "FET_OFF_PLUS", "FET_RATIO_PLUS",
        "FET_RATIO_LABEL_PLUS", "FET_SUMMARY_PLUS",
        "FET_ON_MINUS", "FET_OFF_MINUS", "FET_RATIO_MINUS",
        "FET_RATIO_LABEL_MINUS", "FET_SUMMARY_MINUS"
    };
    for (int ii = 0; ii < names.GetSize(); ii++)
    {
        _fet_delete_graph_object(leftLayer, names[ii]);
        _fet_delete_graph_object(rightLayer, names[ii]);
    }
}

static Worksheet _fet_named_page_sheet(LPCSTR pageName, LPCSTR sheetName,
                                       int minCols, bool clearExisting)
{
    WorksheetPage page(pageName);
    bool created = false;
    if (!page)
    {
        page.Create("Origin");
        page.SetName(pageName, OCD_ENUM_NEXT);
        created = true;
    }
    Worksheet wks = page.Layers(sheetName);
    bool sheetCreated = false;
    if (!wks && created)
    {
        wks = page.Layers(0);
        if (wks)
            wks.SetName(sheetName);
        sheetCreated = true;
    }
    if (!wks)
    {
        int layerIndex = page.AddLayer(sheetName);
        wks = page.Layers(layerIndex);
        sheetCreated = true;
    }
    if (!wks)
        return wks;
    // A brand-new worksheet layer inherits Origin's default template row
    // count. SetSize(0, ...) can't shrink it below Origin's own minimum
    // (observed: 14 rows) -- see _fet_summary_row_for_key, which reuses
    // those leftover blank rows instead of writing past them. Still worth
    // requesting 0 here to shed as much of the template default as Origin
    // allows, on first creation and whenever the caller explicitly asks.
    if (clearExisting || sheetCreated)
        wks.SetSize(0, minCols);
    else if (wks.GetNumCols() < minCols)
        wks.SetSize(wks.GetNumRows(), minCols);
    return wks;
}

static Worksheet _fet_graph_data_sheet(LPCSTR sheetName, int minCols,
                                       bool clearExisting)
{
    return _fet_named_page_sheet(FET_GRAPH_DATA_PAGE, sheetName, minCols,
                                 clearExisting);
}

static Worksheet _fet_extracted_sheet(bool clearExisting)
{
    return _fet_graph_data_sheet("Extracted Parameters", 80, clearExisting);
}

static Worksheet _fet_fit_sheet(bool clearExisting)
{
    WorksheetPage page(FET_FIT_PAGE);
    if (!page)
    {
        page.Create("Origin", CREATE_HIDDEN);
        page.SetName(FET_FIT_PAGE, OCD_ENUM_NEXT);
    }
    Worksheet wks = page.Layers(0);
    if (wks)
        wks.SetName("Fits");
    if (!wks)
        return wks;
    if (clearExisting)
        wks.SetSize(0, 80);
    else if (wks.GetNumCols() < 80)
        wks.SetSize(wks.GetNumRows(), 80);
    return wks;
}

static Worksheet _fet_curves_sheet(bool clearExisting)
{
    return _fet_graph_data_sheet("Curves", 10, clearExisting);
}

// Columns 0-7 are the forward/backward split (each restarting at row 0),
// used for the user-facing table. Columns 8-9 are the full curve verbatim,
// in original order, unsplit -- this is the one stable, always-consistent
// source that hidden/segmented plots for a graph built by
// _fet_create_doubley_graph_from_plot can bind to. Earlier this function
// only ever wrote 0-7 and that same graph type separately owned its own
// bespoke 4-column layout on this exact sheet; whichever of the two ran
// last clobbered the other's columns out from under any plot still bound
// to them (backward data silently vanishing after any re-analysis). Always
// writing both here, unconditionally, removes that conflict instead of
// working around it.
static bool _fet_write_graph_curves_data(const vector& vx, const vector& vy,
                                         int turnIndex, bool hasBackward)
{
    Worksheet wks = _fet_curves_sheet(true);
    if (!wks)
        return false;
    int rows = max(vx.GetSize(), 1);
    wks.SetSize(max(wks.GetNumRows(), rows), 12);
    vector<string> names = {
        "Forward Vg", "Forward Id", "Forward |Id|", "Forward log10(|Id|)",
        "Backward Vg", "Backward Id", "Backward |Id|", "Backward log10(|Id|)",
        "Combined Vg", "Combined Id", "Combined |Id|", "Combined log10(|Id|)"
    };
    vector<string> units = {
        "V", "A", "A", "", "V", "A", "A", "", "V", "A", "A", ""
    };
    for (int cc = 0; cc < names.GetSize(); cc++)
    {
        wks.Columns(cc).SetLongName(names[cc]);
        wks.Columns(cc).SetUnits(units[cc]);
        if (cc == 0 || cc == 4 || cc == 8)
            wks.Columns(cc).SetType(OKDATAOBJ_DESIGNATION_X);
        else
            wks.Columns(cc).SetType(OKDATAOBJ_DESIGNATION_Y);
    }

    for (int rr = 0; rr < wks.GetNumRows(); rr++)
    {
        for (int cc = 0; cc < 12; cc++)
            wks.SetCell(rr, cc, NANUM);
    }

    int forwardEnd = hasBackward ? turnIndex : vx.GetSize() - 1;
    int row = 0;
    int ii;
    for (ii = 0; ii <= forwardEnd && ii < vx.GetSize(); ii++)
    {
        double absId = fabs(vy[ii]);
        wks.SetCell(row, 0, vx[ii]);
        wks.SetCell(row, 1, vy[ii]);
        wks.SetCell(row, 2, absId);
        if (absId > 0)
            wks.SetCell(row, 3, log10(absId));
        row++;
    }

    if (hasBackward)
    {
        row = 0;
        for (ii = turnIndex; ii < vx.GetSize(); ii++)
        {
            double absId = fabs(vy[ii]);
            wks.SetCell(row, 4, vx[ii]);
            wks.SetCell(row, 5, vy[ii]);
            wks.SetCell(row, 6, absId);
            if (absId > 0)
                wks.SetCell(row, 7, log10(absId));
            row++;
        }
    }

    for (ii = 0; ii < vx.GetSize(); ii++)
    {
        double absId = fabs(vy[ii]);
        wks.SetCell(ii, 8, vx[ii]);
        wks.SetCell(ii, 9, vy[ii]);
        wks.SetCell(ii, 10, absId);
        if (absId > 0)
            wks.SetCell(ii, 11, log10(absId));
    }
    wks.AutoSize();
    return true;
}

static bool _fet_find_vg_at_log_current(const vector& vx, const vector& vy,
                                        int begin, int end,
                                        double targetLog, double& vg)
{
    begin = _fet_clamp_int(begin, 0, vx.GetSize() - 1);
    end = _fet_clamp_int(end, begin, vx.GetSize() - 1);
    for (int ii = begin; ii < end; ii++)
    {
        double y1 = fabs(vy[ii]);
        double y2 = fabs(vy[ii + 1]);
        if (y1 <= 0 || y2 <= 0)
            continue;
        double l1 = log10(y1);
        double l2 = log10(y2);
        if (!_fet_valid_number(l1) || !_fet_valid_number(l2))
            continue;
        double d1 = l1 - targetLog;
        double d2 = l2 - targetLog;
        if (d1 == 0)
        {
            vg = vx[ii];
            return true;
        }
        if (d1 * d2 <= 0 && fabs(l2 - l1) > 1e-15)
        {
            double fraction = (targetLog - l1) / (l2 - l1);
            vg = vx[ii] + fraction * (vx[ii + 1] - vx[ii]);
            return true;
        }
    }
    return false;
}

static bool _fet_find_vg_at_linear_current(const vector& vx, const vector& vy,
                                           int begin, int end,
                                           double targetLinear, double& vg)
{
    begin = _fet_clamp_int(begin, 0, vx.GetSize() - 1);
    end = _fet_clamp_int(end, begin, vx.GetSize() - 1);
    for (int ii = begin; ii < end; ii++)
    {
        double y1 = vy[ii];
        double y2 = vy[ii + 1];
        double d1 = y1 - targetLinear;
        double d2 = y2 - targetLinear;
        if (d1 == 0)
        {
            vg = vx[ii];
            return true;
        }
        if (d1 * d2 <= 0 && fabs(y2 - y1) > 1e-30)
        {
            double fraction = (targetLinear - y1) / (y2 - y1);
            vg = vx[ii] + fraction * (vx[ii + 1] - vx[ii]);
            return true;
        }
    }
    return false;
}

static bool _fet_layer_y_range(GraphLayer& gl, double& from, double& to)
{
    set_active_layer(gl);
    gl.LT_execute("__FET_HYST_Y_FROM=layer.y.from;__FET_HYST_Y_TO=layer.y.to;");
    from = NANUM;
    to = NANUM;
    LT_get_var("__FET_HYST_Y_FROM", &from);
    LT_get_var("__FET_HYST_Y_TO", &to);
    return _fet_valid_number(from) && _fet_valid_number(to) && to > from;
}

static void _fet_format_delta_v(double value, string& out)
{
    if (_fet_valid_number(value))
        out.Format("%.4g V", value);
    else
        out = "N/A";
}

// The hysteresis cursor is a single user-movable horizontal line drawn on
// leftLayer (log axis). Because leftLayer and rightLayer are fully overlaid
// on the same physical frame (see docs/architecture.md), the fraction of
// leftLayer's Y range the cursor sits at is also the fraction of
// rightLayer's Y range it sits at -- so one dragged line yields two
// data-space Y levels (log for the left/log axis, linear for the right/Id
// axis) without needing a separate cursor per layer. Vg is found at each
// level independently on the forward and backward segments; a level the
// curve never reaches on one side (out of that segment's data range) leaves
// the corresponding delta as NaN rather than a bogus number.
static bool _fet_update_hysteresis_cursor(GraphLayer& leftLayer,
                                          GraphLayer& rightLayer,
                                          const vector& vx,
                                          const vector& vy,
                                          int turnIndex,
                                          const FETDialogOptions& options,
                                          FETResult& result)
{
    result.hysteresis_level_logA = NANUM;
    result.hysteresis_forward_Vg = NANUM;
    result.hysteresis_backward_Vg = NANUM;
    result.hysteresis_delta_Vg = NANUM;
    result.hysteresis_delta_Vg_linear = NANUM;

    if (!options.showHysteresisCursor || turnIndex < FET_MIN_POINTS)
    {
        _fet_delete_graph_object(leftLayer, FET_HYST_CURSOR);
        _fet_delete_graph_object(leftLayer, FET_HYST_SUMMARY);
        return false;
    }

    double leftFrom, leftTo;
    if (!_fet_layer_y_range(leftLayer, leftFrom, leftTo))
        return false;

    double targetLog = NANUM;
    GraphObject existingCursor = leftLayer.GraphObjects(FET_HYST_CURSOR);
    if (existingCursor)
        _fet_get_graph_object_y(leftLayer, FET_HYST_CURSOR, targetLog);
    if (!_fet_valid_number(targetLog))
        targetLog = (leftFrom + leftTo) / 2.0;

    double inputMin = vx[0];
    double inputMax = vx[0];
    for (int ii = 1; ii < vx.GetSize(); ii++)
    {
        inputMin = min(inputMin, vx[ii]);
        inputMax = max(inputMax, vx[ii]);
    }
    _fet_add_scale_line(leftLayer, FET_HYST_CURSOR, inputMin, targetLog,
                        inputMax, targetLog, options.markerColor,
                        LINE_STYLE_SHORT_DASH, false, false, true, true);

    double forwardLogVg = NANUM, backwardLogVg = NANUM;
    bool haveLog = _fet_find_vg_at_log_current(vx, vy, 0, turnIndex,
                                               targetLog, forwardLogVg)
                && _fet_find_vg_at_log_current(vx, vy, turnIndex,
                                               vx.GetSize() - 1, targetLog,
                                               backwardLogVg);

    result.hysteresis_level_logA = targetLog;
    if (haveLog)
    {
        result.hysteresis_forward_Vg = forwardLogVg;
        result.hysteresis_backward_Vg = backwardLogVg;
        result.hysteresis_delta_Vg = fabs(backwardLogVg - forwardLogVg);
    }

    double rightFrom, rightTo;
    if (_fet_layer_y_range(rightLayer, rightFrom, rightTo))
    {
        double frac = (targetLog - leftFrom) / (leftTo - leftFrom);
        double targetLinear = rightFrom + frac * (rightTo - rightFrom);

        double forwardLinearVg = NANUM, backwardLinearVg = NANUM;
        if (_fet_find_vg_at_linear_current(vx, vy, 0, turnIndex,
                                           targetLinear, forwardLinearVg)
            && _fet_find_vg_at_linear_current(vx, vy, turnIndex,
                                              vx.GetSize() - 1, targetLinear,
                                              backwardLinearVg))
            result.hysteresis_delta_Vg_linear =
                fabs(backwardLinearVg - forwardLinearVg);
    }

    string dLine, dLog;
    _fet_format_delta_v(result.hysteresis_delta_Vg_linear, dLine);
    _fet_format_delta_v(result.hysteresis_delta_Vg, dLog);
    string hystReport;
    hystReport.Format("\\b([+/-])\n\\g(D)V\\-(line)\t%s\n\\g(D)V\\-(log)\t%s",
                      dLine, dLog);
    _fet_delete_graph_object(leftLayer, FET_HYST_SUMMARY);
    string labelScript;
    labelScript.Format("label -p 3 65 -n %s placeholder;", FET_HYST_SUMMARY);
    leftLayer.LT_execute(labelScript);
    GraphObject summaryLabel = leftLayer.GraphObjects(FET_HYST_SUMMARY);
    if (summaryLabel)
    {
        summaryLabel.Text = hystReport;
        summaryLabel.Attach = 0;
        string tableStyle;
        tableStyle.Format("%s.fsize=10;%s.fillcolor=color(255,255,255);%s.transparency=12;",
                          FET_HYST_SUMMARY, FET_HYST_SUMMARY, FET_HYST_SUMMARY);
        leftLayer.LT_execute(tableStyle);
    }

    return haveLog;
}

static void _fet_apply_ioff_cursor_override(GraphLayer& gl,
                                            FETResult& result,
                                            bool backward)
{
    string cursorName = backward ? "FET_OFF_MINUS" : "FET_OFF_PLUS";
    double logOff = NANUM;
    if (!_fet_get_graph_object_y(gl, cursorName, logOff))
        return;
    if (!_fet_valid_number(logOff) || logOff < -30 || logOff > 5)
        return;

    double ioff = pow(10.0, logOff);
    if (!_fet_valid_number(ioff) || ioff <= 0)
        return;

    double sign = result.ioff_signed_A < 0 ? -1.0 : 1.0;
    result.ioff_A = ioff;
    result.ioff_signed_A = sign * ioff;
    result.ion_ioff = result.ioff_A > 0 ? result.ion_A / result.ioff_A : NANUM;
}

static void _fet_expand_log_axis_for_horizontal_refs(GraphLayer& gl,
                                                     double y1, double y2)
{
    if (!gl || !_fet_valid_number(y1) || !_fet_valid_number(y2))
        return;

    set_active_layer(gl);
    gl.LT_execute("__FET_Y_FROM=layer.y.from;__FET_Y_TO=layer.y.to;");
    double axisFrom = NANUM;
    double axisTo = NANUM;
    LT_get_var("__FET_Y_FROM", &axisFrom);
    LT_get_var("__FET_Y_TO", &axisTo);

    double low = min(y1, y2);
    double high = max(y1, y2);
    if (!_fet_valid_number(axisFrom) || !_fet_valid_number(axisTo)
        || axisTo <= axisFrom)
    {
        axisFrom = low;
        axisTo = high;
    }
    double span = max(0.5, axisTo - axisFrom);
    double pad = max(0.15, span * 0.04);
    axisFrom = min(axisFrom, low - pad);
    axisTo = max(axisTo, high + pad);

    string script;
    script.Format("layer.y.from=%.15g;layer.y.to=%.15g;", axisFrom, axisTo);
    gl.LT_execute(script);
}

static bool _fet_add_graph_output(GraphLayer& gl, const XYRange& input,
                                  const XYRange& ssRange,
                                  const XYRange& vthRange,
                                  const FETOptions& options,
                                  const FETResult& result,
                                  bool backward = false,
                                  bool clearExisting = true)
{
    GraphPage graphPage = gl.GetPage();
    GraphLayer leftLayer = graphPage && graphPage.Layers.Count() > 0
                         ? graphPage.Layers(0) : gl;
    GraphLayer rightLayer = graphPage && graphPage.Layers.Count() > 1
                          ? graphPage.Layers(1) : gl;
    if (!leftLayer || !rightLayer)
        return false;

    if (clearExisting)
        _fet_clear_graph_output(leftLayer, rightLayer);

    FETDialogOptions dialogOptions;
    _fet_get_effective_dialog_options(dialogOptions);

    double ssMin, ssMax, ssSignY;
    double vthMin, vthMax, unused;
    _fet_range_x_limits(ssRange, ssMin, ssMax, ssSignY);
    _fet_range_x_limits(vthRange, vthMin, vthMax, unused);
    double inputMin, inputMax, inputSign;
    _fet_range_x_limits(input, inputMin, inputMax, inputSign);
    double ssSpan = max(1e-12, ssMax - ssMin);
    double gmSpan = max(1e-12, vthMax - vthMin);
    double ssFitMin = max(inputMin, ssMin - FET_FIT_EXTENSION * ssSpan);
    double ssFitMax = min(inputMax, ssMax + FET_FIT_EXTENSION * ssSpan);
    double gmFitMin = max(inputMin, vthMin - FET_FIT_EXTENSION * gmSpan);
    double gmFitMax = min(inputMax, vthMax + FET_FIT_EXTENSION * gmSpan);
    if (result.vth_V >= inputMin && result.vth_V <= inputMax)
    {
        gmFitMin = min(gmFitMin, result.vth_V);
        gmFitMax = max(gmFitMax, result.vth_V);
    }

    int cOffset = 40 + (backward ? 10 : 0);
    Worksheet wks = _fet_fit_sheet(clearExisting);
    if (!wks)
        return false;
    if (wks.GetNumCols() < cOffset + 10)
        wks.SetSize(max(wks.GetNumRows(), 3), cOffset + 10);
    if (wks.GetNumRows() < 3)
        wks.SetSize(3, max(wks.GetNumCols(), cOffset + 10));

    for (int cc = cOffset; cc < cOffset + 10; cc += 2)
    {
        wks.Columns(cc).SetType(OKDATAOBJ_DESIGNATION_X);
        wks.Columns(cc + 1).SetType(OKDATAOBJ_DESIGNATION_Y);
    }
    string prefix = backward ? "Backward " : "Forward ";
    string colName;
    colName = prefix + "SS Fit X";
    wks.Columns(cOffset + 0).SetLongName(colName);
    colName = prefix + "SS Fit logAbsId";
    wks.Columns(cOffset + 1).SetLongName(colName);
    colName = prefix + "Vth Fit X";
    wks.Columns(cOffset + 2).SetLongName(colName);
    colName = prefix + "Vth Fit Id";
    wks.Columns(cOffset + 3).SetLongName(colName);
    colName = prefix + "Vth Marker X";
    wks.Columns(cOffset + 4).SetLongName(colName);
    colName = prefix + "Vth Marker Id";
    wks.Columns(cOffset + 5).SetLongName(colName);
    colName = prefix + "On/Off X";
    wks.Columns(cOffset + 6).SetLongName(colName);
    colName = prefix + "On/Off logAbsId";
    wks.Columns(cOffset + 7).SetLongName(colName);
    colName = prefix + "gm Marker X";
    wks.Columns(cOffset + 8).SetLongName(colName);
    colName = prefix + "gm Marker Id";
    wks.Columns(cOffset + 9).SetLongName(colName);

    wks.SetCell(0, cOffset + 0, ssFitMin);
    wks.SetCell(1, cOffset + 0, ssFitMax);
    wks.SetCell(0, cOffset + 1, result.ss_slope_dec_V * ssFitMin + result.ss_intercept);
    wks.SetCell(1, cOffset + 1, result.ss_slope_dec_V * ssFitMax + result.ss_intercept);

    wks.SetCell(0, cOffset + 2, gmFitMin);
    wks.SetCell(1, cOffset + 2, gmFitMax);
    wks.SetCell(0, cOffset + 3, result.vth_slope_A_V * gmFitMin + result.vth_intercept_A);
    wks.SetCell(1, cOffset + 3, result.vth_slope_A_V * gmFitMax + result.vth_intercept_A);

    wks.SetCell(0, cOffset + 4, result.vth_V);
    wks.SetCell(0, cOffset + 5, 0.0);
    wks.SetCell(0, cOffset + 6, result.ion_Vg_V);
    wks.SetCell(0, cOffset + 7, log10(result.ion_A));
    wks.SetCell(1, cOffset + 6, result.ioff_Vg_V);
    wks.SetCell(1, cOffset + 7, log10(result.ioff_A));
    wks.SetCell(0, cOffset + 8, result.gm_Vg_V);
    wks.SetCell(0, cOffset + 9, result.gm_Id_A);

    if (dialogOptions.showFitLines)
    {
        int ssFitColor = _fet_blend_color(dialogOptions.logCurveColor,
                                           SYSCOLOR_BLACK, 0.46);
        int gmFitColor = _fet_blend_color(dialogOptions.linearCurveColor,
                                           SYSCOLOR_BLACK, 0.46);
        DataRange ssPlotRange;
        ssPlotRange.Add(wks, cOffset + 0, "X", cOffset + 0, 0, 1);
        ssPlotRange.Add(wks, cOffset + 1, "Y", cOffset + 1, 0, 1);
        int ssPlotIndex = leftLayer.AddPlot(ssPlotRange, IDM_PLOT_LINE);
        DataPlot ssPlot = leftLayer.DataPlots(ssPlotIndex);
        _fet_style_plot(ssPlot, ssFitColor, false, LINE_STYLE_SHORT_DASH);
        _fet_attach_plot_to_axis(ssPlot, FET_AXIS_LEFT);

        DataRange vthPlotRange;
        vthPlotRange.Add(wks, cOffset + 2, "X", cOffset + 2, 0, 1);
        vthPlotRange.Add(wks, cOffset + 3, "Y", cOffset + 3, 0, 1);
        int vthPlotIndex = rightLayer.AddPlot(vthPlotRange, IDM_PLOT_LINE);
        DataPlot vthPlot = rightLayer.DataPlots(vthPlotIndex);
        _fet_style_plot(vthPlot, gmFitColor, false, LINE_STYLE_SHORT_DASH);
        _fet_attach_plot_to_axis(vthPlot, FET_AXIS_RIGHT);
    }

    if (dialogOptions.showMarkers)
    {
        DataRange vthMarkerRange;
        vthMarkerRange.Add(wks, cOffset + 4, "X", cOffset + 4, 0, 0);
        vthMarkerRange.Add(wks, cOffset + 5, "Y", cOffset + 5, 0, 0);
        int vthMarkerIndex = rightLayer.AddPlot(vthMarkerRange, IDM_PLOT_SCATTER);
        DataPlot vthMarker = rightLayer.DataPlots(vthMarkerIndex);
        _fet_style_plot(vthMarker, dialogOptions.linearCurveColor, true);
        _fet_attach_plot_to_axis(vthMarker, FET_AXIS_RIGHT);

        DataRange onMarkerRange;
        onMarkerRange.Add(wks, cOffset + 6, "X", cOffset + 6, 0, 0);
        onMarkerRange.Add(wks, cOffset + 7, "Y", cOffset + 7, 0, 0);
        int onMarkerIndex = leftLayer.AddPlot(onMarkerRange, IDM_PLOT_SCATTER);
        DataPlot onMarker = leftLayer.DataPlots(onMarkerIndex);
        _fet_style_plot(onMarker, dialogOptions.markerColor, true);
        _fet_attach_plot_to_axis(onMarker, FET_AXIS_LEFT);

        DataRange gmMarkerRange;
        gmMarkerRange.Add(wks, cOffset + 8, "X", cOffset + 8, 0, 0);
        gmMarkerRange.Add(wks, cOffset + 9, "Y", cOffset + 9, 0, 0);
        int gmMarkerIndex = rightLayer.AddPlot(gmMarkerRange, IDM_PLOT_SCATTER);
        DataPlot gmMarker = rightLayer.DataPlots(gmMarkerIndex);
        _fet_style_plot(gmMarker, dialogOptions.markerColor, true);
        _fet_attach_plot_to_axis(gmMarker, FET_AXIS_RIGHT);
    }

    string suffix = backward ? "MINUS" : "PLUS";
    if (dialogOptions.showOnOffArrow)
    {
        double logOn = log10(result.ion_A);
        double logOff = log10(result.ioff_A);
        _fet_expand_log_axis_for_horizontal_refs(leftLayer, logOn, logOff);
        string onName = "FET_ON_" + suffix;
        string offName = "FET_OFF_" + suffix;
        _fet_add_scale_line(leftLayer, onName, inputMin, logOn, inputMax, logOn,
                            dialogOptions.markerColor, LINE_STYLE_SHORT_DASH);
        _fet_add_horizontal_cursor_line(leftLayer, offName, logOff,
                                        dialogOptions.markerColor,
                                        LINE_STYLE_SHORT_DASH, true);
    }

    string summaryName = "FET_SUMMARY_" + suffix;
    _fet_delete_graph_object(leftLayer, summaryName);
    string labelScript;
    labelScript.Format("label -p 3 %d -n %s placeholder;",
                       backward ? 34 : 3, summaryName);
    leftLayer.LT_execute(labelScript);
    GraphObject label = leftLayer.GraphObjects(summaryName);
    if (!label)
        return false;
    string report;
    double densityFactor = options.W_um > 0 ? 1.0e6 / options.W_um : 1.0e6;
    report.Format("\\b(%s)\nSS\t%.3g mV/dec\nV\\-(th)\t%.4g V\ng\\-(m)\t%.4g S\n\\g(m)\\-(FE)\t%.4g cm\\+(2)/(V\\x(00B7)s)\nI\\-(on)\t%.4g uA/um\nI\\-(off)\t%.4g uA/um\nI\\-(on)/I\\-(off)\t%.3g",
                  backward ? "[-]" : "[+]", result.ss_mV_dec,
                  result.vth_V, result.gm_S, result.mobility_cm2_Vs,
                  result.ion_A * densityFactor,
                  result.ioff_A * densityFactor, result.ion_ioff);
    label.Text = report;
    label.Attach = 0;
    string tableStyle;
    tableStyle.Format("%s.fsize=10;%s.fillcolor=color(255,255,255);%s.transparency=12;",
                      summaryName, summaryName, summaryName);
    leftLayer.LT_execute(tableStyle);

    graphPage.Refresh();
    return true;
}

static bool _fet_add_preview_output(GraphLayer& gl, const XYRange& ssRange,
                                    const XYRange& vthRange,
                                    const FETResult& result)
{
    _fet_remove_plots_from_page(gl, FET_PREVIEW_PAGE);

    double ssMin, ssMax, ssSignY;
    double vthMin, vthMax, unused;
    _fet_range_x_limits(ssRange, ssMin, ssMax, ssSignY);
    _fet_range_x_limits(vthRange, vthMin, vthMax, unused);
    double sign = ssSignY < 0 ? -1.0 : 1.0;

    WorksheetPage fitPage(FET_PREVIEW_PAGE);
    Worksheet wks;
    if (!fitPage)
    {
        fitPage.Create("Origin", CREATE_HIDDEN);
        fitPage.SetName(FET_PREVIEW_PAGE, OCD_ENUM_NEXT);
        wks = fitPage.Layers(0);
        wks.SetName("Preview");
    }
    else
    {
        wks = fitPage.Layers(0);
    }
    if (!wks)
        return false;

    wks.SetSize(2, 4);
    for (int cc = 0; cc < 4; cc += 2)
    {
        wks.Columns(cc).SetType(OKDATAOBJ_DESIGNATION_X);
        wks.Columns(cc + 1).SetType(OKDATAOBJ_DESIGNATION_Y);
    }
    wks.Columns(0).SetLongName("SS Preview X");
    wks.Columns(1).SetLongName("SS Preview abs(Id)");
    wks.Columns(2).SetLongName("Vth Preview X");
    wks.Columns(3).SetLongName("Vth Preview Id");

    wks.SetCell(0, 0, ssMin);
    wks.SetCell(1, 0, ssMax);
    wks.SetCell(0, 1, sign * pow(10.0, result.ss_slope_dec_V * ssMin + result.ss_intercept));
    wks.SetCell(1, 1, sign * pow(10.0, result.ss_slope_dec_V * ssMax + result.ss_intercept));

    wks.SetCell(0, 2, vthMin);
    wks.SetCell(1, 2, vthMax);
    wks.SetCell(0, 3, result.vth_slope_A_V * vthMin + result.vth_intercept_A);
    wks.SetCell(1, 3, result.vth_slope_A_V * vthMax + result.vth_intercept_A);

    DataRange ssPlotRange;
    ssPlotRange.Add(wks, 0, "X", 0, 0, 1);
    ssPlotRange.Add(wks, 1, "Y", 1, 0, 1);
    int ssPlotIndex = gl.AddPlot(ssPlotRange, IDM_PLOT_LINE);
    DataPlot ssPlot = gl.DataPlots(ssPlotIndex);
    _fet_style_plot(ssPlot, SYSCOLOR_RED, false);
    _fet_attach_plot_to_axis(ssPlot, FET_AXIS_LEFT);

    DataRange vthPlotRange;
    vthPlotRange.Add(wks, 2, "X", 2, 0, 1);
    vthPlotRange.Add(wks, 3, "Y", 3, 0, 1);
    int vthPlotIndex = gl.AddPlot(vthPlotRange, IDM_PLOT_LINE);
    DataPlot vthPlot = gl.DataPlots(vthPlotIndex);
    _fet_style_plot(vthPlot, SYSCOLOR_BLUE, false);
    _fet_attach_plot_to_axis(vthPlot, FET_AXIS_RIGHT);

    _fet_add_vertical_line(gl, "FET_PREVIEW_VTH", result.vth_V, SYSCOLOR_BLUE);
    gl.GetPage().Refresh();
    return true;
}

static Worksheet _fet_summary_sheet()
{
    Worksheet wks = _fet_graph_data_sheet("Extracted Parameters", 34, false);
    if (!wks)
        return wks;

    vector<string> names = {
        "Graph", "Source Range", "SS Range", "Vth Range", "SS", "SS R-Square",
        "Vth", "Vth R-Square", "gm", "gm Vg", "Mobility", "Ion", "Ioff",
        "Ion/Ioff", "L", "W", "Cox", "Cox Mode", "Oxide Thickness",
        "Kappa", "Vd", "Ioff Vg", "Ion Vg", "Hyst Level logA",
        "Hyst Forward Vg", "Hyst Backward Vg", "Hyst Delta Vg",
        "Hyst Delta Vg (linear)",
        "SS Slope", "SS Intercept", "Vth Slope", "Vth Intercept",
        "Gate Leakage", "Curve"
    };
    vector<string> units = {
        "", "", "", "", "mV/dec", "", "V", "", "S", "V",
        "cm^2/(V s)", "A", "A", "", "um", "um", "F/cm^2", "",
        "nm", "", "V", "V", "V", "", "V", "V", "V", "V",
        "dec/V", "", "A/V", "A", "", ""
    };
    for (int ii = 0; ii < names.GetSize(); ii++)
    {
        wks.Columns(ii).SetLongName(names[ii]);
        wks.Columns(ii).SetUnits(units[ii]);
    }
    return wks;
}

int fet_analyzer_get_summary_row_count_for_test()
{
    Worksheet wks = _fet_summary_sheet();
    return wks ? wks.GetNumRows() : -1;
}

// Finds the existing summary row already owned by curveKey (matched on the
// "Curve" column), so re-analyzing the same curve/direction -- e.g. while
// the user drags a cursor to fine-tune SS/Vth ranges, which re-triggers
// fet_analyzer_refresh_preview() on every drag -- updates that one row in
// place instead of appending a fresh duplicate row per drag.
//
// A curve/direction never seen before reuses the first blank row rather
// than always appending past GetNumRows(). This matters because Origin
// worksheets have a built-in minimum row count (observed: 14) that
// SetSize() cannot shrink below, so a brand-new "Extracted Parameters"
// sheet already has more rows than data. Without reusing those, every
// curve's results land far past the visible top of the sheet and it reads
// as empty. Once those blank rows are used up, new curves append normally.
static int _fet_summary_row_for_key(Worksheet& wks, LPCSTR curveKey)
{
    int rows = wks.GetNumRows();
    if (rows <= 0 || !curveKey || !curveKey[0])
        return rows;

    // GetStringArray() only returns entries up to the last row that ever had
    // a value explicitly set -- on a virgin "Curve" column it comes back
    // empty even though the sheet already has rows (see above). Treat any
    // index beyond what it returns as blank too, rather than bailing out.
    StringArray sa;
    wks.Columns(33).GetStringArray(sa);

    int firstBlank = -1;
    for (int rr = 0; rr < rows; rr++)
    {
        string cell = rr < sa.GetSize() ? sa[rr] : "";
        if (cell.CompareNoCase(curveKey) == 0)
            return rr;
        if (firstBlank < 0 && cell.IsEmpty())
            firstBlank = rr;
    }
    return firstBlank >= 0 ? firstBlank : rows;
}

static bool _fet_write_summary_row(GraphLayer& gl, const FETOptions& options,
                                   const FETResult& result, int row,
                                   LPCSTR curveLabel = "")
{
    Worksheet wks = _fet_summary_sheet();
    if (!wks || row < 0)
        return false;
    if (wks.GetNumRows() <= row)
        wks.SetSize(row + 1, max(wks.GetNumCols(), 34));

    wks.SetCell(row, 0, gl.GetPage().GetName());
    wks.SetCell(row, 1, result.source_range);
    wks.SetCell(row, 2, result.ss_range);
    wks.SetCell(row, 3, result.vth_range);
    wks.SetCell(row, 4, result.ss_mV_dec);
    wks.SetCell(row, 5, result.ss_r2);
    wks.SetCell(row, 6, result.vth_V);
    wks.SetCell(row, 7, result.vth_r2);
    wks.SetCell(row, 8, result.gm_S);
    wks.SetCell(row, 9, result.gm_Vg_V);
    wks.SetCell(row, 10, result.mobility_cm2_Vs);
    wks.SetCell(row, 11, result.ion_A);
    wks.SetCell(row, 12, result.ioff_A);
    wks.SetCell(row, 13, result.ion_ioff);
    wks.SetCell(row, 14, options.L_um);
    wks.SetCell(row, 15, options.W_um);
    wks.SetCell(row, 16, _fet_effective_cox(options));
    wks.SetCell(row, 17, options.coxMode);
    wks.SetCell(row, 18, options.oxideThickness_nm);
    wks.SetCell(row, 19, options.oxideKappa);
    wks.SetCell(row, 20, options.Vd_V);
    wks.SetCell(row, 21, result.ioff_Vg_V);
    wks.SetCell(row, 22, result.ion_Vg_V);
    wks.SetCell(row, 23, result.hysteresis_level_logA);
    wks.SetCell(row, 24, result.hysteresis_forward_Vg);
    wks.SetCell(row, 25, result.hysteresis_backward_Vg);
    wks.SetCell(row, 26, result.hysteresis_delta_Vg);
    wks.SetCell(row, 27, result.hysteresis_delta_Vg_linear);
    wks.SetCell(row, 28, result.ss_slope_dec_V);
    wks.SetCell(row, 29, result.ss_intercept);
    wks.SetCell(row, 30, result.vth_slope_A_V);
    wks.SetCell(row, 31, result.vth_intercept_A);
    wks.SetCell(row, 32, result.gate_warning);
    wks.SetCell(row, 33, curveLabel);
    wks.AutoSize();
    return true;
}

static bool _fet_append_summary(GraphLayer& gl, const FETOptions& options,
                                const FETResult& result)
{
    Worksheet wks = _fet_summary_sheet();
    if (!wks)
        return false;
    DataPlot activePlot = gl.DataPlots();
    string curveLabel = activePlot ? activePlot.GetDatasetName() : "";
    return _fet_write_summary_row(gl, options, result, wks.GetNumRows(),
                                  curveLabel);
}

static int _fet_analyze(const XYRange& input, const XYRange& ssRange,
                        const XYRange& vthRange, const FETOptions& options,
                        bool annotate, bool output, FETResult& result)
{
    result.hysteresis_level_logA = NANUM;
    result.hysteresis_forward_Vg = NANUM;
    result.hysteresis_backward_Vg = NANUM;
    result.hysteresis_delta_Vg = NANUM;
    result.hysteresis_delta_Vg_linear = NANUM;
    double effectiveCox = _fet_effective_cox(options);
    if (options.L_um <= 0 || options.W_um <= 0
        || !_fet_valid_number(effectiveCox) || effectiveCox <= 0
        || fabs(options.Vd_V) < 1e-30)
        return 10;
    if (!_fet_extract_ss(ssRange, result))
        return 20;
    if (!_fet_extract_vth(vthRange, result))
        return 30;
    if (!_fet_extract_curve_metrics(input, options, result))
        return 40;

    XYRange ssCopy(ssRange), vthCopy(vthRange);
    ssCopy.GetRangeString(result.ss_range, NTYPE_NO_CHECK_PLOT);
    vthCopy.GetRangeString(result.vth_range, NTYPE_NO_CHECK_PLOT);

    GraphLayer gl;
    gl = Project.ActiveLayer();
    if (annotate && gl)
        _fet_add_graph_output(gl, input, ssRange, vthRange, options, result);
    if (output && gl)
        _fet_append_summary(gl, options, result);
    return 0;
}

int fet_analyze_xf_core(const XYRange& input, const XYRange& ssRange,
                        const XYRange& vthRange, double L_um, double W_um,
                        double Cox, double Vd, bool annotate, bool output,
                        TreeNode& resultTree)
{
    FETOptions options;
    _fet_default_device_options(options);
    options.L_um = L_um;
    options.W_um = W_um;
    options.Cox_F_cm2 = Cox;
    options.coxMode = FET_COX_DIRECT;
    options.Vd_V = Vd;

    FETResult result;
    int nErr = _fet_analyze(input, ssRange, vthRange, options, annotate, output, result);
    if (nErr != 0)
        return nErr;

    resultTree.SS.dVal = result.ss_mV_dec;
    resultTree.SS_RSquare.dVal = result.ss_r2;
    resultTree.Vth.dVal = result.vth_V;
    resultTree.Vth_RSquare.dVal = result.vth_r2;
    resultTree.gm.dVal = result.gm_S;
    resultTree.gm_Vg.dVal = result.gm_Vg_V;
    resultTree.Mobility.dVal = result.mobility_cm2_Vs;
    resultTree.Ion.dVal = result.ion_A;
    resultTree.Ioff.dVal = result.ioff_A;
    resultTree.IonIoff.dVal = result.ion_ioff;
    resultTree.GateLeakage.strVal = result.gate_warning;
    return 0;
}

// ---- Multi-file batch statistics -----------------------------------------

static int g_fet_multi_direction = 0;  // 0 = forward (+), 1 = backward (-)
static int g_fet_hist_bins = 0;        // 0 = auto (sqrt rule)

static bool _fet_vector_stats(const vector& vals, double& mean, double& sd,
                              double& median, double& vmin, double& vmax)
{
    int n = vals.GetSize();
    if (n < 1)
        return false;

    double sum = 0;
    vmin = vals[0];
    vmax = vals[0];
    int ii;
    for (ii = 0; ii < n; ii++)
    {
        sum += vals[ii];
        if (vals[ii] < vmin) vmin = vals[ii];
        if (vals[ii] > vmax) vmax = vals[ii];
    }
    mean = sum / n;

    double squaredSum = 0;
    for (ii = 0; ii < n; ii++)
        squaredSum += (vals[ii] - mean) * (vals[ii] - mean);
    sd = n > 1 ? sqrt(squaredSum / (n - 1)) : 0.0;

    // vector's copy constructor does not compile from a const reference in
    // Origin C; default-construct and assign instead.
    vector sorted;
    sorted = vals;
    sorted.Sort();
    median = (n % 2) ? sorted[n / 2]
                     : 0.5 * (sorted[n / 2 - 1] + sorted[n / 2]);
    return true;
}

// Bins a parameter's values and, when the spread allows, samples the matching
// normal distribution scaled to the histogram (N * binWidth * pdf) so the
// curve overlays the counts directly.
static void _fet_stats_histogram(const vector& vals, int requestedBins,
                                 double mean, double sd,
                                 vector& centers, vector& counts,
                                 vector& curveX, vector& curveY)
{
    centers.SetSize(0);
    counts.SetSize(0);
    curveX.SetSize(0);
    curveY.SetSize(0);

    int n = vals.GetSize();
    if (n < 1)
        return;

    double vmin = vals[0];
    double vmax = vals[0];
    int ii;
    for (ii = 1; ii < n; ii++)
    {
        if (vals[ii] < vmin) vmin = vals[ii];
        if (vals[ii] > vmax) vmax = vals[ii];
    }
    if (vmax - vmin <= 0)
    {
        double pad = max(fabs(vmax) * 0.05, 1e-12);
        vmin -= pad;
        vmax += pad;
    }

    int bins = requestedBins;
    if (bins < 1)
        bins = _fet_clamp_int((int)ceil(sqrt((double)n)), 4, 15);
    if (bins > 60)
        bins = 60;

    double width = (vmax - vmin) / bins;
    centers.SetSize(bins);
    counts.SetSize(bins);
    for (ii = 0; ii < bins; ii++)
    {
        centers[ii] = vmin + (ii + 0.5) * width;
        counts[ii] = 0;
    }
    for (ii = 0; ii < n; ii++)
    {
        int bin = _fet_clamp_int((int)((vals[ii] - vmin) / width), 0, bins - 1);
        counts[bin] = counts[bin] + 1;
    }

    if (sd > 0)
    {
        int points = 121;
        double x0 = vmin - width;
        double x1 = vmax + width;
        double step = (x1 - x0) / (points - 1);
        double amplitude = n * width / (sd * 2.5066282746310002);
        curveX.SetSize(points);
        curveY.SetSize(points);
        for (ii = 0; ii < points; ii++)
        {
            double x = x0 + step * ii;
            double z = (x - mean) / sd;
            curveX[ii] = x;
            curveY[ii] = amplitude * exp(-0.5 * z * z);
        }
    }
}

static void _fet_style_column_plot(DataPlot& plot, int red, int green, int blue)
{
    if (!plot)
        return;
    int border = _fet_rgb_color(red, green, blue);
    int fill = _fet_blend_color(border, SYSCOLOR_WHITE, 0.55);

    // Theme tag naming for column plots differs across Origin versions;
    // whichever variant validates via UpdateThemeIDs gets applied.
    Tree patternFormat;
    patternFormat.Root.Pattern.Border.Color.nVal = border;
    patternFormat.Root.Pattern.Border.Width.dVal = 1.5;
    patternFormat.Root.Pattern.Fill.FillColor.nVal = fill;
    if (0 == plot.UpdateThemeIDs(patternFormat.Root))
        plot.ApplyFormat(patternFormat, true, true);

    Tree fillFormat;
    fillFormat.Root.Border.Color.nVal = border;
    fillFormat.Root.Border.Width.dVal = 1.5;
    fillFormat.Root.Fill.FillColor.nVal = fill;
    if (0 == plot.UpdateThemeIDs(fillFormat.Root))
        plot.ApplyFormat(fillFormat, true, true);
}

// Fixed name/unit/axis-title metadata for the six batch parameters, shared
// by the data-computation step and the single-panel renderer so both stay
// in sync on ordering.
static void _fet_stats_param_meta(int paramIndex, string& name, string& unit,
                                  string& axisTitle)
{
    vector<string> names = {
        "SS", "Vth", "gm", "Mobility", "Ion", "log10(Ion/Ioff)"
    };
    vector<string> units = {
        "mV/dec", "V", "uS", "cm^2/(V s)", "uA/um", ""
    };
    vector<string> titles = {
        "SS (mV/dec)",
        "\\i(V)\\-(th) (V)",
        "\\i(g)\\-(m) (\\g(m)S)",
        "\\g(m)\\-(FE) (cm\\+(2)/(V\\x(00B7)s))",
        "\\i(I)\\-(on) (\\g(m)A/\\g(m)m)",
        "log\\-(10)(\\i(I)\\-(on)/\\i(I)\\-(off))"
    };
    int idx = paramIndex % names.GetSize();
    if (idx < 0)
        idx += names.GetSize();
    name = names[idx];
    unit = units[idx];
    axisTitle = titles[idx];
}

// Maps a 0-7 selection index (used by the scatter and correlation-matrix
// dialogs) to its column in [FETStatsData]Parameters and a display name.
// Columns 0/1 (Curve/Segment) are text, not selectable here.
static void _fet_batch_param_col_name(int paramIndex, int& col, string& name)
{
    vector<int> cols = { 2, 4, 6, 7, 8, 9, 10, 11 };
    vector<string> names = {
        "SS", "Vth", "gm", "Mobility", "Ion", "Ioff", "Ion/Ioff", "log10(Ion/Ioff)"
    };
    int idx = paramIndex % cols.GetSize();
    if (idx < 0)
        idx += cols.GetSize();
    col = cols[idx];
    name = names[idx];
}

// Reads paired (x, y) values from two [FETStatsData]Parameters columns,
// keeping only rows that are real analyzed curves (non-empty Curve name,
// column 0) with both values present -- skips the blank/NANUM padding rows
// Origin's minimum-row-count leaves behind, and keeps x/y index-aligned to
// the same curve.
static bool _fet_read_batch_param_pair(Worksheet& paramsWks, int colX, int colY,
                                       vector& outX, vector& outY)
{
    outX.SetSize(0);
    outY.SetSize(0);
    if (!paramsWks)
        return false;
    StringArray curveNames;
    paramsWks.Columns(0).GetStringArray(curveNames);
    int rows = paramsWks.GetNumRows();
    int rr;
    for (rr = 0; rr < rows; rr++)
    {
        string curveName = rr < curveNames.GetSize() ? curveNames[rr] : "";
        if (curveName.IsEmpty())
            continue;
        double xv = paramsWks.Cell(rr, colX);
        double yv = paramsWks.Cell(rr, colY);
        if (is_missing_value(xv) || is_missing_value(yv))
            continue;
        outX.Add(xv);
        outY.Add(yv);
    }
    return outX.GetSize() > 0;
}

static double _fet_pearson_correlation(const vector& a, const vector& b)
{
    int n = min(a.GetSize(), b.GetSize());
    if (n < 2)
        return NANUM;
    double sumA = 0, sumB = 0;
    int ii;
    for (ii = 0; ii < n; ii++)
    {
        sumA += a[ii];
        sumB += b[ii];
    }
    double meanA = sumA / n, meanB = sumB / n;
    double cov = 0, varA = 0, varB = 0;
    for (ii = 0; ii < n; ii++)
    {
        double da = a[ii] - meanA;
        double db = b[ii] - meanB;
        cov += da * db;
        varA += da * da;
        varB += db * db;
    }
    if (varA <= 0 || varB <= 0)
        return NANUM;
    return cov / sqrt(varA * varB);
}

// Computes one parameter's summary stats and histogram bins and writes them
// into the [FETStatsData]Statistics/Histogram sheets. Pure data -- draws
// nothing, so switching which parameter is displayed later never needs to
// re-run curve fitting, only re-read these two sheets.
static bool _fet_stats_compute(Worksheet& histWks, Worksheet& statsWks,
                               int paramIndex, LPCSTR paramName, LPCSTR unit,
                               const vector& vals, int requestedBins)
{
    int n = vals.GetSize();
    double mean = NANUM, sd = NANUM, median = NANUM;
    double vmin = NANUM, vmax = NANUM;
    bool haveStats = _fet_vector_stats(vals, mean, sd, median, vmin, vmax);

    if (statsWks)
    {
        if (statsWks.GetNumRows() <= paramIndex)
            statsWks.SetSize(paramIndex + 1, max(statsWks.GetNumCols(), 9));
        statsWks.SetCell(paramIndex, 0, paramName);
        statsWks.SetCell(paramIndex, 1, unit);
        statsWks.SetCell(paramIndex, 2, n);
        statsWks.SetCell(paramIndex, 3, mean);
        statsWks.SetCell(paramIndex, 4, sd);
        statsWks.SetCell(paramIndex, 5, median);
        statsWks.SetCell(paramIndex, 6, vmin);
        statsWks.SetCell(paramIndex, 7, vmax);
        statsWks.SetCell(paramIndex, 8,
                         haveStats && fabs(mean) > 1e-300
                             ? 100.0 * sd / fabs(mean) : NANUM);
    }

    vector centers, counts, curveX, curveY;
    _fet_stats_histogram(vals, requestedBins, mean, sd,
                         centers, counts, curveX, curveY);
    if (!histWks || centers.GetSize() < 1)
        return false;

    int c0 = paramIndex * 4;
    int neededRows = max(centers.GetSize(), curveX.GetSize());
    if (histWks.GetNumRows() < neededRows)
        histWks.SetSize(neededRows, histWks.GetNumCols());
    string colName;
    colName.Format("%s Bin Center", paramName);
    histWks.Columns(c0).SetLongName(colName);
    histWks.Columns(c0).SetUnits(unit);
    histWks.Columns(c0).SetType(OKDATAOBJ_DESIGNATION_X);
    colName.Format("%s Count", paramName);
    histWks.Columns(c0 + 1).SetLongName(colName);
    histWks.Columns(c0 + 1).SetType(OKDATAOBJ_DESIGNATION_Y);
    colName.Format("%s Fit X", paramName);
    histWks.Columns(c0 + 2).SetLongName(colName);
    histWks.Columns(c0 + 2).SetUnits(unit);
    histWks.Columns(c0 + 2).SetType(OKDATAOBJ_DESIGNATION_X);
    colName.Format("%s Fit Count", paramName);
    histWks.Columns(c0 + 3).SetLongName(colName);
    histWks.Columns(c0 + 3).SetType(OKDATAOBJ_DESIGNATION_Y);

    int ii;
    for (ii = 0; ii < histWks.GetNumRows(); ii++)
    {
        histWks.SetCell(ii, c0, ii < centers.GetSize() ? centers[ii] : NANUM);
        histWks.SetCell(ii, c0 + 1, ii < counts.GetSize() ? counts[ii] : NANUM);
        histWks.SetCell(ii, c0 + 2, ii < curveX.GetSize() ? curveX[ii] : NANUM);
        histWks.SetCell(ii, c0 + 3, ii < curveY.GetSize() ? curveY[ii] : NANUM);
    }
    return true;
}

static int g_fet_stats_current_param = 0;

// Draws exactly ONE parameter's histogram + Gaussian overlay + N/mean/SD
// label into a SINGLE layer, replacing whatever that layer showed before.
// This is deliberately not a multi-layer grid: six independently-positioned
// layers on one page kept rendering as a stale, overlapping mess even though
// every layer's own geometry read back correct after being set -- a symptom
// eventually traced to page.width/page.height (a separate, absolute-unit
// aspect-lock) never being set explicitly, leaving the page a mismatched
// size for whatever Resize() hint was given. _fet_set_graph_page_ratio now
// sets both together. A 3-layer graph (see _fet_build_scatter_hist_graph)
// still needs to add layers -- this is that helper.
static GraphLayer _fet_graph_add_layer(GraphPage& gp)
{
    GraphLayer emptyLayer;
    if (!gp)
        return emptyLayer;
    int before = gp.Layers.Count();
    int index = gp.AddLayer();
    if (index < 0 || gp.Layers.Count() <= before)
    {
        // Some Origin versions only add graph layers through LabTalk.
        GraphLayer lastLayer = gp.Layers(before - 1);
        set_active_layer(lastLayer);
        LT_execute("layadd type:=normal;");
        if (gp.Layers.Count() <= before)
            return emptyLayer;
        index = gp.Layers.Count() - 1;
    }
    return gp.Layers(index);
}

static bool _fet_render_stats_param(GraphLayer& gl, int paramIndex)
{
    if (!gl)
        return false;

    WorksheetPage statsBook(FET_STATS_DATA_PAGE);
    if (!statsBook)
        return false;
    Worksheet histWks = statsBook.Layers("Histogram");
    Worksheet statsWks = statsBook.Layers("Statistics");
    if (!histWks || !statsWks)
        return false;

    string paramName, unit, axisTitle;
    _fet_stats_param_meta(paramIndex, paramName, unit, axisTitle);

    int ii;
    for (ii = gl.DataPlots.Count() - 1; ii >= 0; ii--)
        gl.RemovePlot(ii);

    int c0 = paramIndex * 4;
    int rows = histWks.GetNumRows();
    int red, green, blue;
    _fet_multi_curve_rgb(paramIndex, red, green, blue);

    DataRange columnRange;
    columnRange.Add(histWks, c0, "X", c0, 0, rows - 1);
    columnRange.Add(histWks, c0 + 1, "Y", c0 + 1, 0, rows - 1);
    int columnIndex = gl.AddPlot(columnRange, IDM_PLOT_COLUMN);
    DataPlot columnPlot = gl.DataPlots(columnIndex);
    _fet_style_column_plot(columnPlot, red, green, blue);

    DataRange gaussRange;
    gaussRange.Add(histWks, c0 + 2, "X", c0 + 2, 0, rows - 1);
    gaussRange.Add(histWks, c0 + 3, "Y", c0 + 3, 0, rows - 1);
    int gaussIndex = gl.AddPlot(gaussRange, IDM_PLOT_LINE);
    DataPlot gaussPlot = gl.DataPlots(gaussIndex);
    int gaussColor = _fet_blend_color(_fet_rgb_color(red, green, blue),
                                      SYSCOLOR_BLACK, 0.30);
    _fet_style_plot(gaussPlot, gaussColor, false, LINE_STYLE_SOLID, 2.0);

    set_active_layer(gl);
    string script;
    script  = "layer.x.type=0;layer.y.type=0;";
    script += "layer.x.grid.show=0;layer.y.grid.show=0;";
    script += "layer -a;";
    script += "layer.x.label.pt=11;layer.y.label.pt=11;";
    gl.LT_execute(script);

    // Override the Y range to the histogram bars' own max count, not
    // whatever "layer -a" picked considering the Gaussian curve too. A
    // tightly-clustered parameter's Gaussian amplitude (inversely
    // proportional to its standard deviation) can be far taller than any
    // bar, autoscaling the actual bars down to invisible slivers -- this is
    // very likely why some parameters looked "stuck"/empty when cycling
    // with [Prev]/[Next] on real device data (which tends to cluster
    // tightly for some parameters and not others).
    double maxCount = 0;
    for (ii = 0; ii < rows; ii++)
    {
        double v = histWks.Cell(ii, c0 + 1);
        if (_fet_valid_number(v) && v > maxCount)
            maxCount = v;
    }
    double yTo = maxCount > 0 ? maxCount * 1.15 : 1.0;
    string yScript;
    yScript.Format("layer.y.from=0;layer.y.to=%.15g;", yTo);
    gl.LT_execute(yScript);

    string titleScript;
    titleScript.Format("label -xb \"%s\";XB.fsize=12;"
                       "label -yl \"Count\";YL.fsize=12;", axisTitle);
    gl.LT_execute(titleScript);

    double n = statsWks.Cell(paramIndex, 2);
    double mean = statsWks.Cell(paramIndex, 3);
    double sd = statsWks.Cell(paramIndex, 4);
    _fet_delete_graph_object(gl, "FET_HIST_STAT");
    gl.LT_execute("label -p 66 90 -j 0 -n FET_HIST_STAT placeholder;");
    GraphObject infoLabel = gl.GraphObjects("FET_HIST_STAT");
    if (infoLabel && !is_missing_value(n))
    {
        string info;
        info.Format("\\i(N) = %d\n\\g(m) = %.4g\n\\g(s) = %.4g",
                    (int)n, mean, sd);
        infoLabel.Text = info;
        gl.LT_execute("FET_HIST_STAT.fsize=11;FET_HIST_STAT.background=0;");
    }

    string headerText;
    headerText.Format("Parameter %d / %d: %s", paramIndex + 1,
                      FET_STATS_PARAM_COUNT, paramName);
    _fet_delete_graph_object(gl, "FET_STATS_TITLE");
    gl.LT_execute("label -p 15 97 -j 0 -n FET_STATS_TITLE placeholder;");
    GraphObject titleLabel = gl.GraphObjects("FET_STATS_TITLE");
    if (titleLabel)
    {
        titleLabel.Text = headerText;
        gl.LT_execute("FET_STATS_TITLE.fsize=12;FET_STATS_TITLE.background=0;"
                      "FET_STATS_TITLE.color=color(37,99,235);");
    }

    // Bottom corners, away from FET_MULTI (top-right, -p 88 97) -- Next used
    // to sit at that exact same spot and visibly collide with it.
    _fet_delete_graph_object(gl, "FET_STATS_PREV");
    gl.LT_execute("label -p 2 3 -j 0 -n FET_STATS_PREV [Prev];");
    GraphObject prevBtn = gl.GraphObjects("FET_STATS_PREV");
    if (prevBtn)
    {
        prevBtn.Text = "[Prev]";
        string prevScript;
        prevScript = "FET_STATS_PREV.fsize=16;FET_STATS_PREV.color=color(37,99,235);"
                    "FET_STATS_PREV.background=0;FET_STATS_PREV.show=1;"
                    "FET_STATS_PREV.script$=\"fet_analyzer_stats_prev_param();\";"
                    "FET_STATS_PREV.script=1;";
        gl.LT_execute(prevScript);
    }

    _fet_delete_graph_object(gl, "FET_STATS_NEXT");
    gl.LT_execute("label -p 88 3 -j 1 -n FET_STATS_NEXT [Next];");
    GraphObject nextBtn = gl.GraphObjects("FET_STATS_NEXT");
    if (nextBtn)
    {
        nextBtn.Text = "[Next]";
        string nextScript;
        nextScript = "FET_STATS_NEXT.fsize=16;FET_STATS_NEXT.color=color(37,99,235);"
                    "FET_STATS_NEXT.background=0;FET_STATS_NEXT.show=1;"
                    "FET_STATS_NEXT.script$=\"fet_analyzer_stats_next_param();\";"
                    "FET_STATS_NEXT.script=1;";
        gl.LT_execute(nextScript);
    }

    // Re-assert the same square-page/76%-frame proportions every render:
    // RemovePlot/AddPlot churn on every parameter switch is cheap insurance
    // against Origin quietly reflowing the layer back to a smaller default.
    // 5.00in page (3000 = 5in * 600 units/in), not the original 7.6in.
    _fet_set_graph_page_ratio(gl, 500, 3000);
    return true;
}

// Finds the imported "Curves" workbook a multi-file overlay graph was built
// from by matching plot dataset names against worksheet-page names (dataset
// names embed the page name; the longest matching page wins so e.g. "Book1"
// can't shadow "Book12"). Survives project save/reload, unlike any
// session-local bookkeeping.
static WorksheetPage _fet_find_import_book_for_graph(GraphPage& gp)
{
    WorksheetPage emptyPage;
    if (!gp)
        return emptyPage;

    int ll, pp;
    for (ll = 0; ll < gp.Layers.Count(); ll++)
    {
        GraphLayer gl = gp.Layers(ll);
        if (!gl)
            continue;
        for (pp = 0; pp < gl.DataPlots.Count(); pp++)
        {
            DataPlot plot = gl.DataPlots(pp);
            if (!plot || _fet_is_helper_plot(plot))
                continue;
            string dataset = plot.GetDatasetName();
            dataset.MakeLower();

            WorksheetPage best;
            int bestLength = 0;
            WorksheetPage wp;
            foreach (wp in Project.WorksheetPages)
            {
                string pageName = wp.GetName();
                pageName.MakeLower();
                if (pageName.IsEmpty() || dataset.Find(pageName) < 0
                    || pageName.GetLength() <= bestLength)
                    continue;
                Worksheet curvesSheet = wp.Layers("Curves");
                if (!curvesSheet
                    || curvesSheet.GetNumCols() < FET_IMPORT_COLS_PER_CURVE)
                    continue;
                best = wp;
                bestLength = pageName.GetLength();
            }
            if (best)
                return best;
        }
    }
    return emptyPage;
}

// Detects whether a Curves sheet uses the compact (Vg/Id only, 2 columns per
// curve) or full (6 columns per curve) layout. A sheet with fewer than 6
// columns total can only be compact. Otherwise, column index 2 disambiguates
// by content: it is curve #2's "Vg" in a compact sheet, or curve #1's "Ig" in
// a full sheet -- so its long name identifies the layout unambiguously.
static int _fet_detect_curve_layout(Worksheet& curves)
{
    if (!curves)
        return FET_IMPORT_COLS_PER_CURVE;
    if (curves.GetNumCols() < FET_IMPORT_COLS_PER_CURVE)
        return FET_IMPORT_COLS_PER_CURVE_COMPACT;
    string thirdName = curves.Columns(2).GetLongName();
    if (thirdName.CompareNoCase("Vg") == 0)
        return FET_IMPORT_COLS_PER_CURVE_COMPACT;
    return FET_IMPORT_COLS_PER_CURVE;
}

// Resolves the Curves workbook multi-curve analysis should read from: the
// active worksheet if it (or its page) already holds one, otherwise the
// source workbook behind the active overlay/statistics graph. This is the
// "start from whatever's already active, not a file prompt" entry point.
static WorksheetPage _fet_find_multi_source_book()
{
    Worksheet activeWks;
    activeWks = Project.ActiveLayer();
    if (activeWks)
    {
        WorksheetPage wp = activeWks.GetPage();
        if (wp && wp.Layers("Curves"))
            return wp;
    }

    GraphLayer activeGl;
    activeGl = Project.ActiveLayer();
    if (activeGl)
    {
        // Try the hidden source-book tag first (works for both the overlay
        // and statistics graphs, neither of which plots the original import
        // book directly). Fall back to dataset-name matching for graphs
        // built before this tag existed.
        string taggedName;
        if (_fet_read_source_book_tag(activeGl, taggedName))
        {
            WorksheetPage tagged(taggedName);
            if (tagged && tagged.Layers("Curves"))
                return tagged;
        }
        GraphPage gp = activeGl.GetPage();
        return _fet_find_import_book_for_graph(gp);
    }

    WorksheetPage emptyPage;
    return emptyPage;
}

// Derives a Vg/Id/absId/logAbsId sheet (4 columns per curve, always) from
// whichever import layout the source Curves sheet uses, purely to feed the
// overlay graph -- kept separate from the user-facing import table so that
// table can stay Vg/Id-only in compact mode. Rebuilt on every run, like the
// other FETGraphData/FETFitData helper sheets in this file.
static bool _fet_build_multi_graph_data(Worksheet& srcCurves, int colsPerCurve,
                                        int curveCount, Worksheet& outDerived)
{
    if (!srcCurves || curveCount < 1)
        return false;
    int rowsTotal = srcCurves.GetNumRows();
    if (rowsTotal < 1)
        return false;

    outDerived = _fet_named_page_sheet(FET_STATS_DATA_PAGE, "OverlayCurves",
                                       curveCount * 4, true);
    if (!outDerived)
        return false;
    outDerived.SetSize(rowsTotal, curveCount * 4);

    int cc;
    for (cc = 0; cc < curveCount; cc++)
    {
        int srcBase = cc * colsPerCurve;
        int dstBase = cc * 4;
        string label = srcCurves.Columns(srcBase).GetComments();

        XYRange full;
        full.Add(srcCurves, srcBase, "X", srcBase, 0, rowsTotal - 1);
        full.Add(srcCurves, srcBase + 1, "Y", srcBase + 1, 0, rowsTotal - 1);
        vector vx, vy;
        _fet_get_xy(full, vx, vy);

        outDerived.Columns(dstBase).SetLongName("Vg");
        outDerived.Columns(dstBase).SetUnits("V");
        outDerived.Columns(dstBase).SetType(OKDATAOBJ_DESIGNATION_X);
        outDerived.Columns(dstBase).SetComments(label);
        outDerived.Columns(dstBase + 1).SetLongName("Id");
        outDerived.Columns(dstBase + 1).SetUnits("A");
        outDerived.Columns(dstBase + 1).SetType(OKDATAOBJ_DESIGNATION_Y);
        outDerived.Columns(dstBase + 2).SetLongName("absId");
        outDerived.Columns(dstBase + 2).SetUnits("A");
        outDerived.Columns(dstBase + 2).SetType(OKDATAOBJ_DESIGNATION_Y);
        outDerived.Columns(dstBase + 3).SetLongName("logAbsId");
        outDerived.Columns(dstBase + 3).SetUnits("log10(A)");
        outDerived.Columns(dstBase + 3).SetType(OKDATAOBJ_DESIGNATION_Y);

        int rr;
        for (rr = 0; rr < rowsTotal; rr++)
        {
            if (rr < vx.GetSize())
            {
                double absVal = fabs(vy[rr]);
                outDerived.SetCell(rr, dstBase, vx[rr]);
                outDerived.SetCell(rr, dstBase + 1, vy[rr]);
                outDerived.SetCell(rr, dstBase + 2, absVal);
                outDerived.SetCell(rr, dstBase + 3,
                                   absVal > 0 ? log10(absVal) : NANUM);
            }
            else
            {
                outDerived.SetCell(rr, dstBase, NANUM);
                outDerived.SetCell(rr, dstBase + 1, NANUM);
                outDerived.SetCell(rr, dstBase + 2, NANUM);
                outDerived.SetCell(rr, dstBase + 3, NANUM);
            }
        }
    }
    outDerived.AutoSize();
    return true;
}

// Multi-curve overlay graph: every curve on the log (left) axis uses the
// same fixed indigo, every curve on the linear (right) axis uses the same
// fixed amber -- the classic single-curve palette, not a per-curve rainbow.
// Non-highlighted curves are drawn genuinely translucent (Line.Transparency,
// verified to round-trip through Origin's format Tree) rather than blended
// toward white, so hue and axis color stay intact. Only the curve at
// bestIndex (lowest SS, picked by the caller) is fully opaque and bold. No
// legend: the highlighted curve plus faded context is the whole point.
static bool _fet_build_multi_overlay_graph(Worksheet& derived, int curveCount,
                                           int bestIndex, LPCSTR sourceBookName,
                                           GraphPage& outGraph)
{
    if (!derived || curveCount < 1)
        return false;

    GraphPage oldGraph(FET_MULTI_OVERLAY_GRAPH_PAGE);
    if (oldGraph)
        oldGraph.Destroy();

    FETDialogOptions options;
    _fet_get_effective_dialog_options(options);

    GraphPage gp;
    gp.Create("DOUBLEY");
    gp.SetName(FET_MULTI_OVERLAY_GRAPH_PAGE, OCD_ENUM_NEXT);
    GraphLayer leftLayer = gp.Layers(0);
    GraphLayer rightLayer = gp.Layers(1);
    if (!leftLayer || !rightLayer)
        return false;

    int rowsTotal = derived.GetNumRows();
    int cc;
    for (cc = 0; cc < curveCount; cc++)
    {
        int baseCol = cc * 4;
        bool best = (cc == bestIndex);
        double lineWidth = best ? 2.5 : 1.0;
        int transparency = best ? 0 : 65;

        XYRange vgRange;
        vgRange.Add(derived, baseCol, "X", baseCol, 0, rowsTotal - 1);
        vgRange.Add(derived, baseCol + 1, "Y", baseCol + 1, 0, rowsTotal - 1);
        vector vx, vy;
        if (!_fet_get_xy(vgRange, vx, vy))
            continue;

        _fet_add_segmented_visible_plots(leftLayer, derived, baseCol,
                                         baseCol + 3, vx.GetSize(), vx,
                                         options.logCurveColor, FET_AXIS_LEFT,
                                         lineWidth, lineWidth, transparency);
        _fet_add_segmented_visible_plots(rightLayer, derived, baseCol,
                                         baseCol + 1, vx.GetSize(), vx,
                                         options.linearCurveColor, FET_AXIS_RIGHT,
                                         lineWidth, lineWidth, transparency);
    }

    _fet_configure_transfer_graph(leftLayer);
    _fet_configure_linear_transfer_graph(rightLayer);
    _fet_set_y_axis_color(leftLayer, false, options.logCurveColor);
    _fet_set_y_axis_color(rightLayer, true, options.linearCurveColor);
    _fet_add_multi_button(leftLayer);
    _fet_tag_source_book(leftLayer, sourceBookName);
    gp.SetShow(PAGE_ACTIVATE);
    set_active_layer(leftLayer);
    outGraph = gp;
    return true;
}

// Unified multi-curve analysis core: batch-extracts parameters from every
// curve in the source Curves sheet (whichever import layout it uses), builds
// the [FETStatsData]Parameters/Statistics/Histogram tables and the 2 x 3
// histogram panel graph, then builds the overlay graph highlighting the
// lowest-SS curve. Runs both graphs from one call, matching a single "active
// data in, both graphs out" entry point rather than two separate flows.
static int _fet_multi_analyze_core(WorksheetPage& book, string& summaryText)
{
    summaryText = "";
    if (!book)
        return 1;
    Worksheet curves = book.Layers("Curves");
    if (!curves)
        return 1;
    int colsPerCurve = _fet_detect_curve_layout(curves);
    int curveCount = curves.GetNumCols() / colsPerCurve;
    int rowsTotal = curves.GetNumRows();
    if (curveCount < 1 || rowsTotal < FET_MIN_POINTS)
        return 2;

    Worksheet derivedCurves;
    _fet_build_multi_graph_data(curves, colsPerCurve, curveCount, derivedCurves);

    FETDialogOptions options;
    _fet_get_effective_dialog_options(options);
    bool backward = g_fet_multi_direction == 1;
    double densityFactor = options.device.W_um > 0
                         ? 1.0e6 / options.device.W_um : 1.0e6;

    Worksheet paramWks = _fet_named_page_sheet(FET_STATS_DATA_PAGE,
                                               "Parameters", 12, true);
    if (!paramWks)
        return 1;
    vector<string> paramColNames = {
        "Curve", "Segment", "SS", "SS R-Square", "Vth", "Vth R-Square",
        "gm", "Mobility", "Ion", "Ioff", "Ion/Ioff", "log10(Ion/Ioff)"
    };
    vector<string> paramColUnits = {
        "", "", "mV/dec", "", "V", "", "uS", "cm^2/(V s)",
        "uA/um", "uA/um", "", ""
    };
    int cc;
    for (cc = 0; cc < paramColNames.GetSize(); cc++)
    {
        paramWks.Columns(cc).SetLongName(paramColNames[cc]);
        paramWks.Columns(cc).SetUnits(paramColUnits[cc]);
    }

    // The statistics graph is a single layer (see _fet_render_stats_param):
    // it's created up front so that one layer can host the progress readout
    // while the (potentially slow) per-curve fitting loop runs, then gets
    // overwritten with the first parameter's real histogram content
    // afterward.
    GraphPage oldStatsGraph(FET_STATS_GRAPH_PAGE);
    if (oldStatsGraph)
        oldStatsGraph.Destroy();
    GraphPage statsGraph;
    statsGraph.Create("Origin");
    statsGraph.SetName(FET_STATS_GRAPH_PAGE, OCD_ENUM_NEXT);
    GraphLayer progressLayer = statsGraph.Layers(0);
    // Same square-page, 76%-frame proportions as every single-curve graph
    // (_fet_set_graph_page_ratio), just a 5.00in page (3000 = 5in * 600
    // units/in) instead of the usual 7.6in -- page.width/height, not just
    // Resize(), has to change too, or the page stays 7.6in regardless.
    _fet_set_graph_page_ratio(progressLayer, 500, 3000);
    statsGraph.SetShow(PAGE_ACTIVATE);

    vector ssVals, vthVals, gmVals, mobVals, ionVals, ratioVals;
    vector<string> failedCurves;
    int analyzed = 0;
    int bestCurveIndex = -1;
    double bestSS = 1e300;
    progressBox pb("Analyzing curves...");
    pb.SetRange(0, curveCount);
    for (cc = 0; cc < curveCount; cc++)
    {
        int baseCol = cc * colsPerCurve;
        string label = curves.Columns(baseCol).GetComments();
        if (label.IsEmpty())
            label.Format("Curve %d", cc + 1);
        if (!pb.Set(cc))
            break;  // user clicked Cancel -- keep whatever analyzed so far
        _fet_update_progress_label(progressLayer, "Analyzing curves", cc + 1,
                                   curveCount, label);
        _fet_report_progress_status("Analyzing", cc + 1, curveCount, label);

        XYRange full;
        full.Add(curves, baseCol, "X", baseCol, 0, rowsTotal - 1);
        full.Add(curves, baseCol + 1, "Y", baseCol + 1, 0, rowsTotal - 1);
        vector vx, vy;
        if (!_fet_get_xy(full, vx, vy))
        {
            failedCurves.Add(label);
            continue;
        }

        int turnIndex = -1;
        bool hasBackward = _fet_detect_scan_turn(vx, turnIndex);
        int segBegin = 0;
        int segEnd = vx.GetSize() - 1;
        if (backward)
        {
            if (!hasBackward)
            {
                failedCurves.Add(label);
                continue;
            }
            segBegin = turnIndex;
        }
        else if (hasBackward)
        {
            segEnd = turnIndex;
        }

        FETRangeIndices indices;
        if (!_fet_auto_pick_segment_indices(vx, vy, segBegin, segEnd,
                                            backward, indices))
        {
            failedCurves.Add(label);
            continue;
        }

        // The imported Curves sheet holds only valid Vg/Id pairs from row 0
        // upward (trailing rows are NANUM and get cleaned), so cleaned-vector
        // indices map 1:1 onto worksheet rows here.
        XYRange input, ssRange, vthRange;
        input.Add(curves, baseCol, "X", baseCol, segBegin, segEnd);
        input.Add(curves, baseCol + 1, "Y", baseCol + 1, segBegin, segEnd);
        ssRange.Add(curves, baseCol, "X", baseCol,
                    indices.ssBegin, indices.ssEnd);
        ssRange.Add(curves, baseCol + 1, "Y", baseCol + 1,
                    indices.ssBegin, indices.ssEnd);
        vthRange.Add(curves, baseCol, "X", baseCol,
                     indices.vthBegin, indices.vthEnd);
        vthRange.Add(curves, baseCol + 1, "Y", baseCol + 1,
                     indices.vthBegin, indices.vthEnd);

        FETResult result;
        if (0 != _fet_analyze(input, ssRange, vthRange, options.device,
                              false, false, result))
        {
            failedCurves.Add(label);
            continue;
        }

        int row = analyzed;
        if (paramWks.GetNumRows() <= row)
            paramWks.SetSize(row + 1, max(paramWks.GetNumCols(), 12));
        double gm_uS = result.gm_S * 1.0e6;
        double ionDensity = result.ion_A * densityFactor;
        double ioffDensity = result.ioff_A * densityFactor;
        double ratioLog = result.ion_ioff > 0 ? log10(result.ion_ioff) : NANUM;
        paramWks.SetCell(row, 0, label);
        paramWks.SetCell(row, 1, backward ? "-" : "+");
        paramWks.SetCell(row, 2, result.ss_mV_dec);
        paramWks.SetCell(row, 3, result.ss_r2);
        paramWks.SetCell(row, 4, result.vth_V);
        paramWks.SetCell(row, 5, result.vth_r2);
        paramWks.SetCell(row, 6, gm_uS);
        paramWks.SetCell(row, 7, result.mobility_cm2_Vs);
        paramWks.SetCell(row, 8, ionDensity);
        paramWks.SetCell(row, 9, ioffDensity);
        paramWks.SetCell(row, 10, result.ion_ioff);
        paramWks.SetCell(row, 11, ratioLog);

        ssVals.Add(result.ss_mV_dec);
        vthVals.Add(result.vth_V);
        gmVals.Add(gm_uS);
        mobVals.Add(result.mobility_cm2_Vs);
        ionVals.Add(ionDensity);
        if (_fet_valid_number(ratioLog))
            ratioVals.Add(ratioLog);
        if (result.ss_mV_dec < bestSS)
        {
            bestSS = result.ss_mV_dec;
            bestCurveIndex = cc;
        }
        analyzed++;
    }
    pb.Set(curveCount);
    _fet_remove_progress_label(progressLayer);
    LT_execute("type -s \"Analysis complete.\";");

    // SetSize(0, ...) cannot shrink below Origin's minimum row count, so a
    // re-run over fewer curves would otherwise leave the previous run's
    // rows visible below the fresh ones.
    int staleRow, staleCol;
    for (staleRow = analyzed; staleRow < paramWks.GetNumRows(); staleRow++)
    {
        paramWks.SetCell(staleRow, 0, "");
        paramWks.SetCell(staleRow, 1, "");
        for (staleCol = 2; staleCol < 12; staleCol++)
            paramWks.SetCell(staleRow, staleCol, NANUM);
    }
    paramWks.AutoSize();

    string bookName = book.GetName();
    GraphPage overlayGraph;
    bool overlayBuilt = derivedCurves
        && _fet_build_multi_overlay_graph(derivedCurves, curveCount,
                                          bestCurveIndex, bookName,
                                          overlayGraph);

    if (analyzed < 1)
    {
        summaryText.Format(
            "No curve could be analyzed with the current fitting settings "
            "[%s segment]. Try a larger smoothing window or a lower minimum "
            "R-Square.%s",
            backward ? "backward" : "forward",
            overlayBuilt ? "\n\nThe overlay graph was still built." : "");
        return overlayBuilt ? 5 : 3;
    }

    Worksheet statsWks = _fet_named_page_sheet(FET_STATS_DATA_PAGE,
                                               "Statistics", 9, true);
    Worksheet histWks = _fet_named_page_sheet(FET_STATS_DATA_PAGE,
                                              "Histogram",
                                              FET_STATS_PARAM_COUNT * 4, true);
    if (!statsWks || !histWks)
        return 4;
    vector<string> statsColNames = {
        "Parameter", "Unit", "N", "Mean", "SD", "Median", "Min", "Max", "CV"
    };
    for (cc = 0; cc < statsColNames.GetSize(); cc++)
    {
        statsWks.Columns(cc).SetLongName(statsColNames[cc]);
        statsWks.Columns(cc).SetUnits(cc == 8 ? "%" : "");
    }

    int pp;
    for (pp = 0; pp < FET_STATS_PARAM_COUNT; pp++)
    {
        vector vals;
        if (pp == 0) vals = ssVals;
        else if (pp == 1) vals = vthVals;
        else if (pp == 2) vals = gmVals;
        else if (pp == 3) vals = mobVals;
        else if (pp == 4) vals = ionVals;
        else vals = ratioVals;

        string paramName, paramUnit, axisTitle;
        _fet_stats_param_meta(pp, paramName, paramUnit, axisTitle);
        _fet_stats_compute(histWks, statsWks, pp, paramName, paramUnit, vals,
                          g_fet_hist_bins);
    }
    statsWks.AutoSize();
    histWks.AutoSize();

    // One layer, showing one parameter at a time (see _fet_render_stats_param
    // for why this replaced an earlier six-layer-per-page grid). Starts on
    // whichever parameter the settings dialog's "Show parameter" picked
    // (default SS, index 0, for a first-ever run).
    if (g_fet_stats_current_param < 0 || g_fet_stats_current_param >= FET_STATS_PARAM_COUNT)
        g_fet_stats_current_param = 0;
    GraphLayer statsLayer = statsGraph.Layers(0);
    _fet_render_stats_param(statsLayer, g_fet_stats_current_param);
    _fet_add_multi_button(statsLayer);
    _fet_tag_source_book(statsLayer, bookName);
    set_active_layer(statsLayer);
    statsGraph.Refresh();

    string statsPageName = paramWks.GetPage().GetName();
    string statsGraphName = statsGraph.GetName();
    string overlayGraphName = "";
    if (overlayBuilt)
        overlayGraphName = overlayGraph.GetName();
    string bestSuffix = "";
    if (bestCurveIndex >= 0)
    {
        string bestLabel;
        bestLabel = curves.Columns(bestCurveIndex * colsPerCurve).GetComments();
        bestSuffix.Format(" (highlighted: lowest SS = %s)", bestLabel);
    }
    summaryText.Format(
        "Analyzed %d of %d curve(s) [%s segment].\n\n"
        "Overlay graph: \"%s\"%s\n"
        "Distribution histograms: graph \"%s\"\n"
        "Parameter table: [%s]Parameters\n"
        "Statistics summary: [%s]Statistics",
        analyzed, curveCount, backward ? "backward" : "forward",
        overlayGraphName, bestSuffix,
        statsGraphName, statsPageName, statsPageName);
    if (failedCurves.GetSize() > 0)
    {
        summaryText += "\n\nSkipped curves:";
        for (cc = 0; cc < failedCurves.GetSize() && cc < 8; cc++)
        {
            summaryText += "\n  ";
            summaryText += failedCurves[cc];
        }
        if (failedCurves.GetSize() > 8)
            summaryText += "\n  ...";
    }
    return 0;
}

// Fitting-parameter settings for multi-curve analysis. Shares the
// device/extraction options with the single-curve settings dialog (so tuning
// them in one place tunes both) and adds the analyzed sweep segment and the
// histogram bin count.
static bool _fet_get_multi_options(LPCSTR title = "FET Multi-Curve Analysis")
{
    FETDialogOptions options;
    _fet_get_effective_dialog_options(options);

    GETN_BOX(settings)
    GETN_BEGIN_BRANCH(device, "Device Parameters") GETN_OPTION_BRANCH(GETNBRANCH_OPEN)
        GETN_NUM(L_um, "Channel Length L (um)", options.device.L_um)
        GETN_NUM(W_um, "Channel Width W (um)", options.device.W_um)
        GETN_LIST(CoxMode, "Cox input", options.device.coxMode, "Manual Cox|HfOx|AlOx|SiOx|Manual kappa")
        GETN_NUM(OxideThickness, "Oxide thickness (nm)", options.device.oxideThickness_nm)
        GETN_NUM(Kappa, "Manual kappa", options.device.oxideKappa)
        GETN_NUM(Cox, "Manual Cox (F/cm^2)", options.device.Cox_F_cm2)
        GETN_NUM(Vd, "Drain Voltage Vd (V)", options.device.Vd_V)
    GETN_END_BRANCH(device)
    GETN_BEGIN_BRANCH(extract, "Fitting") GETN_OPTION_BRANCH(GETNBRANCH_OPEN)
        GETN_LIST(Direction, "Analyzed segment", g_fet_multi_direction, "Forward (+)|Backward (-)")
        GETN_NUM(SmoothingWindow, "Smoothing window (odd points)", options.smoothingWindow)
        GETN_NUM(SSWindowPoints, "Automatic SS window (0 = auto)", options.ssWindowPoints)
        GETN_NUM(VthWindowPoints, "Automatic Vth window (0 = auto)", options.vthWindowPoints)
        GETN_NUM(MinFitR2, "Minimum automatic fit R-Square", options.minFitR2)
    GETN_END_BRANCH(extract)
    GETN_BEGIN_BRANCH(stats, "Statistics")
        GETN_NUM(Bins, "Histogram bins (0 = auto)", g_fet_hist_bins)
        GETN_LIST(ShowParam, "Show parameter", g_fet_stats_current_param, "SS|Vth|gm|Mobility|Ion|log10(Ion/Ioff)")
    GETN_END_BRANCH(stats)

    settings.device.OxideThickness.Enable =
        options.device.coxMode == FET_COX_DIRECT ? 0 : 1;
    settings.device.Kappa.Enable =
        options.device.coxMode == FET_COX_MANUAL_KAPPA ? 1 : 0;
    settings.device.Cox.Enable =
        options.device.coxMode == FET_COX_DIRECT ? 1 : 0;

    if (!GetNBox(settings, title))
        return false;

    options.device.L_um = settings.device.L_um.dVal;
    options.device.W_um = settings.device.W_um.dVal;
    options.device.coxMode = settings.device.CoxMode.nVal;
    options.device.oxideThickness_nm = settings.device.OxideThickness.dVal;
    options.device.oxideKappa = settings.device.Kappa.dVal;
    options.device.Cox_F_cm2 = settings.device.Cox.dVal;
    options.device.Vd_V = settings.device.Vd.dVal;
    options.smoothingWindow = (int)settings.extract.SmoothingWindow.dVal;
    options.ssWindowPoints = (int)settings.extract.SSWindowPoints.dVal;
    options.vthWindowPoints = (int)settings.extract.VthWindowPoints.dVal;
    options.minFitR2 = settings.extract.MinFitR2.dVal;
    if (options.smoothingWindow < 1)
        options.smoothingWindow = 1;
    if (options.smoothingWindow % 2 == 0)
        options.smoothingWindow++;
    if (options.ssWindowPoints < 0)
        options.ssWindowPoints = 0;
    if (options.vthWindowPoints < 0)
        options.vthWindowPoints = 0;
    if (options.minFitR2 < 0)
        options.minFitR2 = 0;
    if (options.minFitR2 > 1)
        options.minFitR2 = 1;

    g_fet_multi_direction = settings.extract.Direction.nVal == 1 ? 1 : 0;
    g_fet_hist_bins = (int)settings.stats.Bins.dVal;
    if (g_fet_hist_bins < 0)
        g_fet_hist_bins = 0;
    if (g_fet_hist_bins > 60)
        g_fet_hist_bins = 60;
    g_fet_stats_current_param = settings.stats.ShowParam.nVal;

    g_fet_dialog_options = options;
    g_fet_dialog_options_initialized = true;
    return true;
}

// ---- Scatter + marginal histograms and correlation matrix ----------------
//
// Both read from an already-computed [FETStatsData]Parameters table (i.e.
// multi-curve analysis must have run at least once) rather than re-fitting
// curves, since they're cross-parameter analyses over results that already
// exist.

static int g_fet_scatter_x_param = 4;  // Ion
static int g_fet_scatter_y_param = 0;  // SS

static bool _fet_get_scatter_options()
{
    GETN_BOX(settings)
    GETN_LIST(XParam, "X-axis parameter", g_fet_scatter_x_param, "SS|Vth|gm|Mobility|Ion|Ioff|Ion/Ioff|log10(Ion/Ioff)")
    GETN_LIST(YParam, "Y-axis parameter", g_fet_scatter_y_param, "SS|Vth|gm|Mobility|Ion|Ioff|Ion/Ioff|log10(Ion/Ioff)")
    if (!GetNBox(settings, "FET Scatter + Histograms"))
        return false;
    g_fet_scatter_x_param = settings.XParam.nVal;
    g_fet_scatter_y_param = settings.YParam.nVal;
    return true;
}

// Builds a 3-layer graph: a main scatter (bottom-left) plus one marginal
// histogram along each axis (top for X, right for Y), both aligned to the
// main layer's own autoscaled range so the three visually line up. Bars use
// the already-proven IDM_PLOT_COLUMN/IDM_PLOT_BAR types (Column for
// vertical, Bar for horizontal), matching the histogram styling already
// used by the statistics graph.
static bool _fet_string_list_contains(const vector<string>& names, LPCSTR s)
{
    int ii;
    for (ii = 0; ii < names.GetSize(); ii++)
        if (names[ii].CompareNoCase(s) == 0)
            return true;
    return false;
}

// Snapshots the names of all current GraphPages, for use with
// _fet_pick_new_graph_page below. Needed because neither LT_execute()'s own
// return value NOR Project.ActiveLayer() reliably indicates whether an
// X-Function (plot_marginal/plotmatrix) actually built the graph we asked
// for: headless testing this session showed LT_execute() can return FALSE
// even when a correct multi-layer graph was built, AND return TRUE while
// only producing junk single-layer pages -- so the only trustworthy check
// is to look at what pages actually exist before/after and inspect their
// shape directly.
static void _fet_snapshot_graph_page_names(vector<string>& names)
{
    names.SetSize(0);
    GraphPage gp;
    foreach (gp in Project.GraphPages)
        names.Add(gp.GetName());
}

// Finds the best-shaped GraphPage created since `before` was snapshotted
// (highest Layers.Count()), destroys every OTHER newly-created page (so a
// failed/degenerate X-Function attempt -- e.g. plotmatrix silently emitting
// several throwaway single-layer pages instead of one real matrix grid,
// confirmed via headless probing -- doesn't leave clutter behind), and
// returns whether the best candidate meets minLayers. On failure, ALL new
// pages are destroyed and false is returned.
static bool _fet_pick_new_graph_page(const vector<string>& before, int minLayers,
                                     string& graphName)
{
    graphName = "";
    vector<string> newNames;
    string bestName;
    int bestLayers = -1;
    GraphPage gp;
    foreach (gp in Project.GraphPages)
    {
        if (_fet_string_list_contains(before, gp.GetName()))
            continue;
        newNames.Add(gp.GetName());
        int lc = gp.Layers.Count();
        if (lc > bestLayers)
        {
            bestLayers = lc;
            bestName = gp.GetName();
        }
    }
    bool ok = bestLayers >= minLayers;
    int ii;
    for (ii = 0; ii < newNames.GetSize(); ii++)
    {
        if (ok && newNames[ii].CompareNoCase(bestName) == 0)
            continue; // keep the winner
        GraphPage toKill(newNames[ii]);
        if (toKill)
            toKill.Destroy();
    }
    if (!ok)
        return false;
    graphName = bestName;
    return true;
}

// Renames whatever auto-generated page name an X-Function chose (e.g.
// "Graph3") to a fixed, predictable page name, destroying any stale page
// already occupying that name first -- so downstream code (the [FET Multi]
// re-run button's source-book tag lookup, re-running the same
// scatter/correlation operation twice, and tests) can find "the" scatter or
// correlation graph by a fixed name regardless of whether the native
// template or the hand-built fallback produced it.
static void _fet_rename_graph_page_to(string& graphName, LPCSTR fixedName)
{
    if (graphName.CompareNoCase(fixedName) == 0)
        return;
    GraphPage target(graphName);
    if (!target)
        return;
    GraphPage stale(fixedName);
    if (stale && stale.GetName().CompareNoCase(graphName) != 0)
        stale.Destroy();
    target.SetName(fixedName, OCD_ENUM_NEXT);
    graphName = target.GetName();
}

// Tries Origin's built-in "Marginal Histograms" graph -- the plot_marginal
// X-Function, the same template reachable from Origin's own graph gallery --
// before falling back to a hand-built 3-layer graph. Confirmed via headless
// probing this session that plot_marginal CAN build a correct 3-layer
// marginal-histogram graph even while LT_execute() itself reports failure,
// so success/failure here is judged purely by what page shows up afterward
// (a real marginal-histogram graph needs a main layer plus 2 margins = 3
// layers), not by LT_execute()'s return value.
// srcWks must be a plain 2-column (X, Y) worksheet.
static bool _fet_try_native_marginal_plot(Worksheet& srcWks, string& graphName)
{
    graphName = "";
    if (!srcWks)
        return false;
    string bookName = srcWks.GetPage().GetName();
    string wksName = srcWks.GetName();
    vector<string> before;
    _fet_snapshot_graph_page_names(before);
    string script;
    script.Format("plot_marginal iy:=[%s]%s!(1,2) type:=0 top:=0 right:=0;",
                  bookName, wksName);
    LT_execute(script);
    return _fet_pick_new_graph_page(before, 3, graphName);
}

// Hand-built 3-layer fallback: a main scatter (bottom-left) plus one
// marginal histogram along each axis (top for X, right for Y), both aligned
// to the main layer's own autoscaled range so the three visually line up.
// Bars use the already-proven IDM_PLOT_COLUMN/IDM_PLOT_BAR types.
static bool _fet_build_scatter_hist_graph_fallback(Worksheet& derived,
                                                    const vector& vx,
                                                    const vector& vy,
                                                    LPCSTR nameX, LPCSTR nameY,
                                                    int rows, string& summaryText)
{
    double meanX = NANUM, sdX = NANUM, medianX = NANUM, minX = NANUM, maxX = NANUM;
    _fet_vector_stats(vx, meanX, sdX, medianX, minX, maxX);
    double meanY = NANUM, sdY = NANUM, medianY = NANUM, minY = NANUM, maxY = NANUM;
    _fet_vector_stats(vy, meanY, sdY, medianY, minY, maxY);

    vector centersX, countsX, curveXX, curveYX;
    _fet_stats_histogram(vx, 0, meanX, sdX, centersX, countsX, curveXX, curveYX);
    vector centersY, countsY, curveXY, curveYY;
    _fet_stats_histogram(vy, 0, meanY, sdY, centersY, countsY, curveXY, curveYY);

    int histRows = max(centersX.GetSize(), centersY.GetSize());
    if (derived.GetNumCols() < 6 || derived.GetNumRows() < histRows)
        derived.SetSize(max(rows, histRows), 6);
    derived.Columns(2).SetLongName("X Bin Center");
    derived.Columns(2).SetType(OKDATAOBJ_DESIGNATION_X);
    derived.Columns(3).SetLongName("X Bin Count");
    derived.Columns(3).SetType(OKDATAOBJ_DESIGNATION_Y);
    derived.Columns(4).SetLongName("Y Bin Count");
    derived.Columns(4).SetType(OKDATAOBJ_DESIGNATION_X);
    derived.Columns(5).SetLongName("Y Bin Center");
    derived.Columns(5).SetType(OKDATAOBJ_DESIGNATION_Y);

    int ii;
    for (ii = 0; ii < derived.GetNumRows(); ii++)
    {
        derived.SetCell(ii, 2, ii < centersX.GetSize() ? centersX[ii] : NANUM);
        derived.SetCell(ii, 3, ii < countsX.GetSize() ? countsX[ii] : NANUM);
        derived.SetCell(ii, 4, ii < countsY.GetSize() ? countsY[ii] : NANUM);
        derived.SetCell(ii, 5, ii < centersY.GetSize() ? centersY[ii] : NANUM);
    }
    derived.AutoSize();

    FETDialogOptions options;
    _fet_get_effective_dialog_options(options);
    COLORREF markerRgb = okutil_convert_ocolor_to_RGB(options.markerColor);
    int markerRed = GetRValue(markerRgb);
    int markerGreen = GetGValue(markerRgb);
    int markerBlue = GetBValue(markerRgb);

    GraphPage oldGraph(FET_SCATTER_GRAPH_PAGE);
    if (oldGraph)
        oldGraph.Destroy();
    GraphPage gp;
    gp.Create("Origin");
    gp.SetName(FET_SCATTER_GRAPH_PAGE, OCD_ENUM_NEXT);
    GraphLayer mainLayer = gp.Layers(0);
    if (!mainLayer)
        return false;
    _fet_set_graph_page_ratio(mainLayer, 500, 3600);

    set_active_layer(mainLayer);
    mainLayer.LT_execute("layer.unit=0;layer.left=16;layer.top=15;layer.width=60;layer.height=60;");

    DataRange scatterRange;
    scatterRange.Add(derived, 0, "X", 0, 0, rows - 1);
    scatterRange.Add(derived, 1, "Y", 1, 0, rows - 1);
    int scatterIndex = mainLayer.AddPlot(scatterRange, IDM_PLOT_SCATTER);
    DataPlot scatterPlot = mainLayer.DataPlots(scatterIndex);
    _fet_style_plot(scatterPlot, options.logCurveColor, true);

    string mainScript;
    mainScript.Format("layer -a;layer.x.grid.show=0;layer.y.grid.show=0;"
                      "label -xb \"%s\";label -yl \"%s\";"
                      "XB.fsize=12;YL.fsize=12;layer.x.label.pt=10;layer.y.label.pt=10;",
                      nameX, nameY);
    mainLayer.LT_execute(mainScript);

    mainLayer.LT_execute("__FET_SCX_FROM=layer.x.from;__FET_SCX_TO=layer.x.to;"
                         "__FET_SCY_FROM=layer.y.from;__FET_SCY_TO=layer.y.to;");
    double xFrom = 0, xTo = 0, yFrom = 0, yTo = 0;
    LT_get_var("__FET_SCX_FROM", &xFrom);
    LT_get_var("__FET_SCX_TO", &xTo);
    LT_get_var("__FET_SCY_FROM", &yFrom);
    LT_get_var("__FET_SCY_TO", &yTo);

    GraphLayer topLayer = _fet_graph_add_layer(gp);
    if (topLayer)
    {
        set_active_layer(topLayer);
        topLayer.LT_execute("layer.unit=0;layer.left=16;layer.top=78;layer.width=60;layer.height=16;");
        DataRange topRange;
        topRange.Add(derived, 2, "X", 2, 0, centersX.GetSize() - 1);
        topRange.Add(derived, 3, "Y", 3, 0, centersX.GetSize() - 1);
        int topIndex = topLayer.AddPlot(topRange, IDM_PLOT_COLUMN);
        DataPlot topPlot = topLayer.DataPlots(topIndex);
        _fet_style_column_plot(topPlot, markerRed, markerGreen, markerBlue);
        string topScript;
        topScript.Format("layer.x.from=%.15g;layer.x.to=%.15g;layer -a;"
                         "layer.x.from=%.15g;layer.x.to=%.15g;"
                         "layer.x.showAxes=0;layer.y.showAxes=1;"
                         "layer.x.grid.show=0;layer.y.grid.show=0;"
                         "label -yl \"Count\";YL.fsize=10;",
                         xFrom, xTo, xFrom, xTo);
        topLayer.LT_execute(topScript);
    }

    GraphLayer rightLayer = _fet_graph_add_layer(gp);
    if (rightLayer)
    {
        set_active_layer(rightLayer);
        rightLayer.LT_execute("layer.unit=0;layer.left=78;layer.top=15;layer.width=16;layer.height=60;");
        DataRange rightRange;
        rightRange.Add(derived, 4, "X", 4, 0, centersY.GetSize() - 1);
        rightRange.Add(derived, 5, "Y", 5, 0, centersY.GetSize() - 1);
        int rightIndex = rightLayer.AddPlot(rightRange, IDM_PLOT_BAR);
        DataPlot rightPlot = rightLayer.DataPlots(rightIndex);
        _fet_style_column_plot(rightPlot, markerRed, markerGreen, markerBlue);
        string rightScript;
        rightScript.Format("layer.y.from=%.15g;layer.y.to=%.15g;layer -a;"
                           "layer.y.from=%.15g;layer.y.to=%.15g;"
                           "layer.y.showAxes=0;layer.x.showAxes=1;"
                           "layer.x.grid.show=0;layer.y.grid.show=0;"
                           "label -xb \"Count\";XB.fsize=10;",
                           yFrom, yTo, yFrom, yTo);
        rightLayer.LT_execute(rightScript);
    }

    gp.SetShow(PAGE_ACTIVATE);
    set_active_layer(mainLayer);

    string graphName = gp.GetName();
    summaryText.Format("Scatter + marginal histograms: \"%s\" (N = %d).\n\n"
                       "X: %s   Y: %s",
                       graphName, rows, nameX, nameY);
    return true;
}

static bool _fet_build_scatter_hist_graph(int xParamIdx, int yParamIdx,
                                          string& summaryText)
{
    summaryText = "";
    WorksheetPage statsBook(FET_STATS_DATA_PAGE);
    if (!statsBook)
        return false;
    Worksheet paramsWks = statsBook.Layers("Parameters");
    if (!paramsWks)
        return false;

    int colX, colY;
    string nameX, nameY;
    _fet_batch_param_col_name(xParamIdx, colX, nameX);
    _fet_batch_param_col_name(yParamIdx, colY, nameY);

    vector vx, vy;
    if (!_fet_read_batch_param_pair(paramsWks, colX, colY, vx, vy) || vx.GetSize() < 2)
        return false;
    int rows = vx.GetSize();

    Worksheet derived = _fet_named_page_sheet(FET_SCATTER_DATA_PAGE, "Data", 6, true);
    if (!derived)
        return false;
    derived.SetSize(rows, 6);
    derived.Columns(0).SetLongName(nameX);
    derived.Columns(0).SetType(OKDATAOBJ_DESIGNATION_X);
    derived.Columns(1).SetLongName(nameY);
    derived.Columns(1).SetType(OKDATAOBJ_DESIGNATION_Y);
    int ii;
    for (ii = 0; ii < rows; ii++)
    {
        derived.SetCell(ii, 0, vx[ii]);
        derived.SetCell(ii, 1, vy[ii]);
    }
    derived.AutoSize();

    string graphName;
    if (_fet_try_native_marginal_plot(derived, graphName))
    {
        _fet_rename_graph_page_to(graphName, FET_SCATTER_GRAPH_PAGE);
        summaryText.Format(
            "Scatter + marginal histograms (Origin's native Marginal "
            "Histograms template): \"%s\" (N = %d).\n\nX: %s   Y: %s",
            graphName, rows, nameX, nameY);
        return true;
    }

    return _fet_build_scatter_hist_graph_fallback(derived, vx, vy, nameX, nameY,
                                                  rows, summaryText);
}

static bool _fet_get_correlation_options(vector<int>& selected)
{
    bool checkSS = true, checkVth = true, checkGm = false, checkMob = true,
         checkIon = true, checkIoff = false, checkRatio = false, checkLogRatio = true;

    GETN_BOX(settings)
    GETN_CHECK(SS, "SS", checkSS)
    GETN_CHECK(Vth, "Vth", checkVth)
    GETN_CHECK(Gm, "gm", checkGm)
    GETN_CHECK(Mobility, "Mobility", checkMob)
    GETN_CHECK(Ion, "Ion", checkIon)
    GETN_CHECK(Ioff, "Ioff", checkIoff)
    GETN_CHECK(Ratio, "Ion/Ioff", checkRatio)
    GETN_CHECK(LogRatio, "log10(Ion/Ioff)", checkLogRatio)
    if (!GetNBox(settings, "FET Correlation Matrix"))
        return false;

    selected.SetSize(0);
    if (settings.SS.nVal) selected.Add(0);
    if (settings.Vth.nVal) selected.Add(1);
    if (settings.Gm.nVal) selected.Add(2);
    if (settings.Mobility.nVal) selected.Add(3);
    if (settings.Ion.nVal) selected.Add(4);
    if (settings.Ioff.nVal) selected.Add(5);
    if (settings.Ratio.nVal) selected.Add(6);
    if (settings.LogRatio.nVal) selected.Add(7);
    return true;
}

// Pairwise Pearson correlation over the selected parameters, written as a
// plain coefficient table (not a colored heatmap image -- Origin C's
// worksheet cell/conditional-formatting API isn't exercised anywhere else
// in this file, so a table using only already-proven cell-write calls was
// the safer deliverable here).
// Tries Origin's built-in Scatter Matrix graph -- the plotmatrix X-Function,
// reachable from the same gallery as "Correlation Plot" -- before falling
// back to a hand-computed coefficient table. Headless probing this session
// showed a failed/degenerate plotmatrix call can still report LT_execute()
// success while actually emitting several throwaway single-layer pages
// instead of one real matrix grid, so (as with _fet_try_native_marginal_plot)
// success is judged by the shape of whatever page(s) appear afterward, not
// by LT_execute()'s return value: a real scatter-matrix grid for M selected
// parameters needs well more than one layer, so require at least M layers
// as a conservative floor. irng accepts non-contiguous column numbers
// directly (`(2,3,5)`-style), so this reads straight from
// [FETStatsData]Parameters, no derived worksheet needed.
static bool _fet_try_native_scatter_matrix(Worksheet& paramsWks,
                                           const vector<int>& selected,
                                           string& graphName)
{
    graphName = "";
    if (!paramsWks)
        return false;
    string bookName = paramsWks.GetPage().GetName();
    string wksName = paramsWks.GetName();

    string colList;
    int ii;
    for (ii = 0; ii < selected.GetSize(); ii++)
    {
        int col;
        string name;
        _fet_batch_param_col_name(selected[ii], col, name);
        string token;
        if (ii == 0)
            token.Format("%d", col + 1);
        else
            token.Format(",%d", col + 1);
        colList += token;
    }

    vector<string> before;
    _fet_snapshot_graph_page_names(before);
    string script;
    script.Format("plotmatrix irng:=[%s]%s!(%s) ellipse:=1 fit:=1;",
                  bookName, wksName, colList);
    LT_execute(script);
    int minLayers = selected.GetSize() > 2 ? selected.GetSize() : 2;
    return _fet_pick_new_graph_page(before, minLayers, graphName);
}

static int _fet_build_correlation_matrix(const vector<int>& selected,
                                         string& summaryText)
{
    summaryText = "";
    WorksheetPage statsBook(FET_STATS_DATA_PAGE);
    if (!statsBook)
        return 1;
    Worksheet paramsWks = statsBook.Layers("Parameters");
    if (!paramsWks)
        return 1;

    int m = selected.GetSize();
    if (m < 2)
        return 2;

    string nativeGraphName;
    if (_fet_try_native_scatter_matrix(paramsWks, selected, nativeGraphName))
    {
        _fet_rename_graph_page_to(nativeGraphName, FET_CORRELATION_GRAPH_PAGE);
        summaryText.Format(
            "Correlation matrix (Origin's native Scatter Matrix template): "
            "\"%s\" (%d parameters).", nativeGraphName, m);
        return 0;
    }

    Worksheet outWks = _fet_named_page_sheet(FET_STATS_DATA_PAGE, "Correlation",
                                             m + 1, true);
    if (!outWks)
        return 3;
    outWks.SetSize(m, m + 1);
    outWks.Columns(0).SetLongName("Parameter");

    int ii, jj;
    for (ii = 0; ii < m; ii++)
    {
        int colI;
        string nameI;
        _fet_batch_param_col_name(selected[ii], colI, nameI);
        outWks.Columns(ii + 1).SetLongName(nameI);
        outWks.SetCell(ii, 0, nameI);
    }

    for (ii = 0; ii < m; ii++)
    {
        int colI;
        string nameI;
        _fet_batch_param_col_name(selected[ii], colI, nameI);
        for (jj = 0; jj < m; jj++)
        {
            if (ii == jj)
            {
                outWks.SetCell(ii, jj + 1, 1.0);
                continue;
            }
            int colJ;
            string nameJ;
            _fet_batch_param_col_name(selected[jj], colJ, nameJ);
            vector pairX, pairY;
            _fet_read_batch_param_pair(paramsWks, colI, colJ, pairX, pairY);
            outWks.SetCell(ii, jj + 1, _fet_pearson_correlation(pairX, pairY));
        }
    }
    outWks.AutoSize();

    string sheetPage = outWks.GetPage().GetName();
    summaryText.Format("Correlation matrix (coefficient table -- Origin's "
                       "native Scatter Matrix graph was unavailable) "
                       "written to [%s]Correlation (%d parameters).",
                       sheetPage, m);
    return 0;
}

// Shows/hides (never deletes) the visible forward/backward segment plots
// for the curve currently being analyzed, so choosing "Forward" in Settings
// actually hides the backward curve on the graph instead of just skipping
// its analysis. The hidden full-range source plot always has as many
// points as the whole curve (vx.GetSize()), so any visible candidate with
// fewer points is a segment, not the source -- and among segments, the one
// starting at vx[0] is forward, the one starting at vx[turnIndex] is
// backward. Restricting to candidates whose overall X span matches this
// curve's avoids touching a different curve's segments in a multi-curve
// graph.
static void _fet_set_curve_segment_visibility(GraphLayer& gl, const vector& vx,
                                              int turnIndex, bool hasBackward,
                                              bool showForward, bool showBackward)
{
    if (!gl || !hasBackward || vx.GetSize() == 0)
        return;

    double curveMin = vx[0], curveMax = vx[0];
    int ii;
    for (ii = 1; ii < vx.GetSize(); ii++)
    {
        if (vx[ii] < curveMin) curveMin = vx[ii];
        if (vx[ii] > curveMax) curveMax = vx[ii];
    }
    double forwardStartX = vx[0];
    double backwardStartX = vx[turnIndex];

    set_active_layer(gl);
    for (ii = 0; ii < gl.DataPlots.Count(); ii++)
    {
        DataPlot candidate = gl.DataPlots(ii);
        if (!candidate || _fet_is_helper_plot(candidate))
            continue;

        XYRange candidateRange;
        candidate.GetDataRange(candidateRange, 0, -1);
        vector cx, cy;
        if (!_fet_get_xy(candidateRange, cx, cy) || cx.GetSize() == 0
            || cx.GetSize() >= vx.GetSize())
            continue;

        double cMin = cx[0], cMax = cx[0];
        int jj;
        for (jj = 1; jj < cx.GetSize(); jj++)
        {
            if (cx[jj] < cMin) cMin = cx[jj];
            if (cx[jj] > cMax) cMax = cx[jj];
        }
        if (fabs(cMin - curveMin) > 1e-9 || fabs(cMax - curveMax) > 1e-9)
            continue;

        bool isForward = fabs(cx[0] - forwardStartX) < 1e-9;
        bool isBackward = !isForward && fabs(cx[0] - backwardStartX) < 1e-9;
        if (!isForward && !isBackward)
            continue;
        bool wantShow = isForward ? showForward : showBackward;
        if (wantShow == candidate.IsShow())
            continue;
        string script;
        script.Format("layer -hp %d %d;", wantShow ? 0 : 1, ii + 1);
        gl.LT_execute(script);
    }
}

int fet_analyzer_refresh_preview()
{
    GraphLayer gl;
    gl = Project.ActiveLayer();
    if (!gl)
        return 1;

    GraphPage graphPage = gl.GetPage();
    GraphLayer leftLayer = graphPage && graphPage.Layers.Count() > 0
                         ? graphPage.Layers(0) : gl;
    DataPlot activePlot = _fet_get_analysis_plot_for_graph_layer(gl);
    if (!activePlot)
        return 2;

    XYRange fullInput;
    activePlot.GetDataRange(fullInput, 0, -1);
    vector vx, vy;
    if (!_fet_get_xy(fullInput, vx, vy))
        return 3;

    FETDialogOptions dialogOptions;
    _fet_get_effective_dialog_options(dialogOptions);
    int turnIndex = -1;
    bool hasBackward = _fet_detect_scan_turn(vx, turnIndex);
    bool useForward = dialogOptions.scanMode != FET_SCAN_BACKWARD;
    bool useBackward = hasBackward
                    && (dialogOptions.scanMode == FET_SCAN_AUTO
                        || dialogOptions.scanMode == FET_SCAN_BACKWARD
                        || dialogOptions.scanMode == FET_SCAN_BOTH);
    // Only drop the backward range cursors when the curve genuinely has no
    // backward segment (a data fact). Previously this also fired whenever
    // the user merely chose "Forward" in settings, deleting the user's
    // tuned backward cursors -- so switching scanMode back to Both/Backward
    // afterward had nothing left to re-use and looked broken. The
    // hysteresis marker only means something when backward is actively
    // analyzed, so it still gets hidden (and freshly redrawn) with scanMode.
    if (!hasBackward)
        _fet_remove_backward_range_cursors(leftLayer);
    if (!useBackward)
    {
        _fet_delete_graph_object(leftLayer, FET_HYST_CURSOR);
        _fet_delete_graph_object(leftLayer, FET_HYST_SUMMARY);
    }
    _fet_write_graph_curves_data(vx, vy, turnIndex, hasBackward);

    GraphLayer rightLayer = graphPage && graphPage.Layers.Count() > 1
                          ? graphPage.Layers(1) : gl;
    _fet_set_curve_segment_visibility(leftLayer, vx, turnIndex, hasBackward,
                                      useForward, useBackward);
    _fet_set_curve_segment_visibility(rightLayer, vx, turnIndex, hasBackward,
                                      useForward, useBackward);

    string curveLabel = activePlot.GetDatasetName();
    string forwardKey = curveLabel + " [+]";
    string backwardKey = curveLabel + " [-]";
    Worksheet summarySheet = _fet_summary_sheet();
    bool outputAdded = false;
    if (useForward)
    {
        int segmentEnd = hasBackward ? turnIndex : vx.GetSize() - 1;
        XYRange input, ssRange, vthRange;
        activePlot.GetDataRange(input, 0, segmentEnd);
        if (!_fet_ranges_from_named_or_auto(
                leftLayer, activePlot,
                FET_CURSOR_SS_START, FET_CURSOR_SS_END,
                FET_CURSOR_VTH_START, FET_CURSOR_VTH_END,
                vx, vy, 0, segmentEnd, false, ssRange, vthRange))
            return 4;

        FETResult result;
        int nErr = _fet_analyze(input, ssRange, vthRange,
                                dialogOptions.device, false, false, result);
        if (nErr != 0)
            return 100 + nErr;
        _fet_apply_ioff_cursor_override(leftLayer, result, false);
        if (!_fet_add_graph_output(leftLayer, input, ssRange, vthRange,
                                   dialogOptions.device, result, false, true))
            return 5;
        if (dialogOptions.appendSummary && summarySheet)
        {
            int row = _fet_summary_row_for_key(summarySheet, forwardKey);
            _fet_write_summary_row(leftLayer, dialogOptions.device, result,
                                   row, forwardKey);
        }
        outputAdded = true;
    }

    if (useBackward)
    {
        XYRange input, ssRange, vthRange;
        activePlot.GetDataRange(input, turnIndex, vx.GetSize() - 1);
        if (!_fet_ranges_from_named_or_auto(
                leftLayer, activePlot,
                FET_CURSOR_BWD_SS_START, FET_CURSOR_BWD_SS_END,
                FET_CURSOR_BWD_VTH_START, FET_CURSOR_BWD_VTH_END,
                vx, vy, turnIndex, vx.GetSize() - 1, true,
                ssRange, vthRange))
            return 6;

        FETResult result;
        int nErr = _fet_analyze(input, ssRange, vthRange,
                                dialogOptions.device, false, false, result);
        if (nErr != 0)
            return 200 + nErr;
        _fet_apply_ioff_cursor_override(leftLayer, result, true);
        if (!_fet_add_graph_output(leftLayer, input, ssRange, vthRange,
                                   dialogOptions.device, result, true,
                                   !outputAdded))
            return 7;
        if (dialogOptions.appendSummary && summarySheet)
        {
            int row = _fet_summary_row_for_key(summarySheet, backwardKey);
            _fet_write_summary_row(leftLayer, dialogOptions.device, result,
                                   row, backwardKey);
        }
        outputAdded = true;
    }

    // Runs after both _fet_add_graph_output calls above so the hysteresis
    // cursor's axis-center default reflects the final Y range -- Ion/Ioff
    // reference lines can expand it (_fet_expand_log_axis_for_horizontal_refs),
    // and centering against a pre-expansion range wouldn't match what the
    // user actually sees.
    if (hasBackward && useBackward)
    {
        FETResult hystResult;
        _fet_update_hysteresis_cursor(leftLayer, rightLayer, vx, vy,
                                      turnIndex, dialogOptions, hystResult);
        if (dialogOptions.appendSummary && summarySheet && useForward)
        {
            int row = _fet_summary_row_for_key(summarySheet, forwardKey);
            if (row >= 0 && row < summarySheet.GetNumRows())
            {
                summarySheet.SetCell(row, 23, hystResult.hysteresis_level_logA);
                summarySheet.SetCell(row, 24, hystResult.hysteresis_forward_Vg);
                summarySheet.SetCell(row, 25, hystResult.hysteresis_backward_Vg);
                summarySheet.SetCell(row, 26, hystResult.hysteresis_delta_Vg);
                summarySheet.SetCell(row, 27, hystResult.hysteresis_delta_Vg_linear);
            }
        }
    }

    if (!outputAdded)
        return hasBackward ? 8 : 9;
    graphPage.Refresh();
    return 0;
}

static void _fet_default_dialog_options(FETDialogOptions& options)
{
    _fet_default_device_options(options.device);
    options.annotate = true;
    options.appendSummary = true;
    options.refreshGraphStyle = true;
    options.recreateCursors = false;
    options.showFitLines = true;
    options.showMarkers = true;
    options.showOnOffArrow = true;
    options.showHysteresisCursor = true;
    options.scanMode = FET_SCAN_AUTO;
    options.smoothingWindow = 1;
    options.ssWindowPoints = 0;
    options.vthWindowPoints = 0;
    options.minFitR2 = 0.90;
    options.logCurveColor = _fet_rgb_color(79, 70, 229);
    options.linearCurveColor = _fet_rgb_color(217, 119, 6);
    options.ssColor = options.logCurveColor;
    options.vthColor = options.linearCurveColor;
    options.markerColor = _fet_rgb_color(194, 65, 61);
}

static bool _fet_get_dialog_options(FETDialogOptions& options,
                                    LPCSTR title = "FET Gadget Settings")
{
    GETN_BOX(settings)
    GETN_BEGIN_BRANCH(device, "Device Parameters") GETN_OPTION_BRANCH(GETNBRANCH_OPEN)
        GETN_NUM(L_um, "Channel Length L (um)", options.device.L_um)
        GETN_NUM(W_um, "Channel Width W (um)", options.device.W_um)
        GETN_LIST(CoxMode, "Cox input", options.device.coxMode, "Manual Cox|HfOx|AlOx|SiOx|Manual kappa")
        GETN_NUM(OxideThickness, "Oxide thickness (nm)", options.device.oxideThickness_nm)
        GETN_NUM(Kappa, "Manual kappa", options.device.oxideKappa)
        GETN_NUM(Cox, "Manual Cox (F/cm^2)", options.device.Cox_F_cm2)
        GETN_NUM(Vd, "Drain Voltage Vd (V)", options.device.Vd_V)
    GETN_END_BRANCH(device)
    GETN_BEGIN_BRANCH(extract, "Extraction Methods")
        GETN_LIST(ScanMode, "Scan direction", options.scanMode, "Auto|Forward (+)|Backward (-)|Both (+/-)")
        GETN_NUM(SmoothingWindow, "Smoothing window (odd points)", options.smoothingWindow)
        GETN_NUM(SSWindowPoints, "Automatic SS window (0 = auto)", options.ssWindowPoints)
        GETN_NUM(VthWindowPoints, "Automatic Vth window (0 = auto)", options.vthWindowPoints)
        GETN_NUM(MinFitR2, "Minimum automatic fit R-Square", options.minFitR2)
        GETN_STR(SSMethod, "SS method", "log10(abs(Id)) linear fit")
        GETN_STR(VthMethod, "Vth method", "Id linear extrapolation")
        GETN_STR(IonMethod, "Ion/Ioff method", "Ion=max abs(Id); Ioff=off-region mean / cursor")
    GETN_END_BRANCH(extract)
    GETN_BEGIN_BRANCH(cursors, "Range Cursors")
        GETN_CHECK(Recreate, "Recreate automatic cursors", options.recreateCursors)
        GETN_STR(SSRange, "SS range", "lightened log-curve cursor pair")
        GETN_STR(VthRange, "gm range", "lightened linear-curve cursor pair")
    GETN_END_BRANCH(cursors)
    // Curve/axis/marker colors used to be user-configurable here (manual
    // color pickers plus presets). Removed by request -- colors now stay
    // fixed at the values in _fet_default_dialog_options rather than being
    // settable per-graph.
    GETN_BEGIN_BRANCH(graph, "Graph")
        GETN_CHECK(RefreshGraphStyle, "Apply logAbsId/Id layer formatting", options.refreshGraphStyle)
        GETN_CHECK(ShowFitLines, "Show extended SS and gm-range fit lines", options.showFitLines)
        GETN_CHECK(ShowMarkers, "Show circular Vth, Ion and gm markers", options.showMarkers)
        GETN_CHECK(ShowOnOffArrow, "Show Ion reference and movable Ioff cursor", options.showOnOffArrow)
        GETN_CHECK(ShowHysteresisCursor, "Show hysteresis opening cursor", options.showHysteresisCursor)
        GETN_STR(LeftAxis, "Left Y axis", "|Id| in uA/um; scientific tick labels")
        GETN_STR(RightAxis, "Right Y axis", "Id in uA/um; starts at zero")
    GETN_END_BRANCH(graph)
    GETN_BEGIN_BRANCH(output, "Output")
        GETN_CHECK(Annotate, "Annotate active graph", options.annotate)
        GETN_CHECK(AppendSummary, "Append [FETGraphData]Extracted Parameters", options.appendSummary)
    GETN_END_BRANCH(output)

    settings.extract.SSMethod.Enable = 0;
    settings.extract.VthMethod.Enable = 0;
    settings.extract.IonMethod.Enable = 0;
    settings.cursors.SSRange.Enable = 0;
    settings.cursors.VthRange.Enable = 0;
    settings.device.OxideThickness.Enable =
        options.device.coxMode == FET_COX_DIRECT ? 0 : 1;
    settings.device.Kappa.Enable =
        options.device.coxMode == FET_COX_MANUAL_KAPPA ? 1 : 0;
    settings.device.Cox.Enable =
        options.device.coxMode == FET_COX_DIRECT ? 1 : 0;
    settings.graph.LeftAxis.Enable = 0;
    settings.graph.RightAxis.Enable = 0;

    if (!GetNBox(settings, title))
        return false;

    options.device.L_um = settings.device.L_um.dVal;
    options.device.W_um = settings.device.W_um.dVal;
    options.device.coxMode = settings.device.CoxMode.nVal;
    options.device.oxideThickness_nm = settings.device.OxideThickness.dVal;
    options.device.oxideKappa = settings.device.Kappa.dVal;
    options.device.Cox_F_cm2 = settings.device.Cox.dVal;
    options.device.Vd_V = settings.device.Vd.dVal;
    options.scanMode = settings.extract.ScanMode.nVal;
    options.smoothingWindow = (int)settings.extract.SmoothingWindow.dVal;
    options.ssWindowPoints = (int)settings.extract.SSWindowPoints.dVal;
    options.vthWindowPoints = (int)settings.extract.VthWindowPoints.dVal;
    options.minFitR2 = settings.extract.MinFitR2.dVal;
    if (options.smoothingWindow < 1)
        options.smoothingWindow = 1;
    if (options.smoothingWindow % 2 == 0)
        options.smoothingWindow++;
    if (options.ssWindowPoints < 0)
        options.ssWindowPoints = 0;
    if (options.vthWindowPoints < 0)
        options.vthWindowPoints = 0;
    if (options.minFitR2 < 0)
        options.minFitR2 = 0;
    if (options.minFitR2 > 1)
        options.minFitR2 = 1;
    options.recreateCursors = settings.cursors.Recreate.nVal != 0;
    options.refreshGraphStyle = settings.graph.RefreshGraphStyle.nVal != 0;
    options.showFitLines = settings.graph.ShowFitLines.nVal != 0;
    options.showMarkers = settings.graph.ShowMarkers.nVal != 0;
    options.showOnOffArrow = settings.graph.ShowOnOffArrow.nVal != 0;
    options.showHysteresisCursor = settings.graph.ShowHysteresisCursor.nVal != 0;
    options.annotate = settings.output.Annotate.nVal != 0;
    options.appendSummary = settings.output.AppendSummary.nVal != 0;
    return true;
}

void fet_analyzer_show_settings()
{
    GraphLayer gl;
    gl = Project.ActiveLayer();
    if (!gl)
    {
        _fet_message("Activate a FET graph before opening FET Gadget settings.",
                     MB_OK | MB_ICONEXCLAMATION);
        return;
    }

    GraphPage graphPage = gl.GetPage();
    GraphLayer leftLayer = graphPage && graphPage.Layers.Count() > 0
                         ? graphPage.Layers(0) : gl;
    GraphLayer rightLayer = graphPage && graphPage.Layers.Count() > 1
                          ? graphPage.Layers(1) : gl;
    DataPlot activePlot = _fet_get_analysis_plot_for_graph_layer(leftLayer);
    FETDialogOptions options;
    _fet_get_effective_dialog_options(options);
    int oldScanMode = options.scanMode;
    if (!_fet_get_dialog_options(options))
        return;

    g_fet_dialog_options = options;
    g_fet_dialog_options_initialized = true;
    if (options.refreshGraphStyle)
    {
        _fet_configure_transfer_graph(leftLayer);
        _fet_configure_linear_transfer_graph(rightLayer);
        _fet_style_data_curves(leftLayer, options.logCurveColor);
        _fet_style_data_curves(rightLayer, options.linearCurveColor);
        _fet_set_y_axis_color(leftLayer, false, options.logCurveColor);
        _fet_set_y_axis_color(rightLayer, true, options.linearCurveColor);
    }
    if ((options.recreateCursors || oldScanMode != options.scanMode)
        && activePlot)
        _fet_add_free_range_cursors(leftLayer, activePlot);
    else
        fet_analyzer_refresh_preview();
    _fet_add_config_button(leftLayer);
    graphPage.Refresh();
    set_active_layer(leftLayer);
}

int fet_analyzer_reapply_graph_style_for_test()
{
    GraphLayer gl;
    gl = Project.ActiveLayer();
    if (!gl)
        return 1;
    GraphPage graphPage = gl.GetPage();
    GraphLayer leftLayer = graphPage && graphPage.Layers.Count() > 0
                         ? graphPage.Layers(0) : gl;
    GraphLayer rightLayer = graphPage && graphPage.Layers.Count() > 1
                          ? graphPage.Layers(1) : gl;
    _fet_configure_transfer_graph(leftLayer);
    _fet_configure_linear_transfer_graph(rightLayer);
    _fet_add_config_button(leftLayer);
    _fet_add_config_button(leftLayer);

    int buttonCount = 0;
    GraphObject object;
    foreach (object in leftLayer.GraphObjects)
    {
        if (object.GetName().CompareNoCase(FET_CONFIG_BUTTON) == 0)
            buttonCount++;
    }
    GraphObject button = leftLayer.GraphObjects(FET_CONFIG_BUTTON);
    if (!button || buttonCount != 1)
        return 2;
    if (button.Text.CompareNoCase("[FET Gadget]") != 0)
        return 4;
    leftLayer.LT_execute("__FET_CONFIG_BACKGROUND=FET_CONFIG.BACKGROUND;__FET_CONFIG_SHOW=FET_CONFIG.SHOW;");
    double buttonBackground = 0, buttonShow = 0;
    if (!LT_get_var("__FET_CONFIG_BACKGROUND", &buttonBackground)
        || !LT_get_var("__FET_CONFIG_SHOW", &buttonShow)
        || fabs(buttonBackground) > 0.5
        || buttonShow < 0.5)
        return 11;

    Axis axis = leftLayer.YAxis;
    Tree format;
    format = axis.GetFormat(FPB_ALL, FOB_AXIS_LABELS, true, true);
    TreeNode labels = tree_get_node_by_tagname(format, "LeftLabels", true);
    if (!labels || !labels.GetNode("Formula")
        || labels.GetNode("Formula").strVal.CompareNoCase("x+6") != 0)
        return 5;
    if (leftLayer.GraphObjects("Legend")
        || rightLayer.GraphObjects("Legend"))
        return 6;
    leftLayer.LT_execute("__FET_ASPECT=layer.width/layer.height;");
    double aspect = 0;
    if (!LT_get_var("__FET_ASPECT", &aspect) || fabs(aspect - 1.0) > 0.01)
        return 7;
    leftLayer.LT_execute("__FET_PAGE_ASPECT=page.dvwidth/page.dvheight;");
    double pageAspect = 0;
    if (!LT_get_var("__FET_PAGE_ASPECT", &pageAspect)
        || fabs(pageAspect - 1.0) > 0.01)
        return 10;
    rightLayer.LT_execute("__FET_RIGHT_FROM=layer.y.from;");
    double rightFrom = NANUM;
    if (!LT_get_var("__FET_RIGHT_FROM", &rightFrom) || fabs(rightFrom) > 1e-15)
        return 8;
    return 0;
}

int fet_analyzer_prepare_active_graph_for_test()
{
    GraphLayer gl;
    gl = Project.ActiveLayer();
    if (!gl)
        return 1;

    gl = _fet_prepare_graph_for_analysis(gl);
    DataPlot activePlot = _fet_get_analysis_plot_for_graph_layer(gl);
    if (!activePlot)
        return 2;

    XYRange input;
    activePlot.GetDataRange(input, 0, -1);
    vector vx, vy;
    if (!_fet_get_xy(input, vx, vy))
        return 3;

    int turnIndex = -1;
    return _fet_detect_scan_turn(vx, turnIndex) ? 0 : 4;
}

// Core of the "Analyze" button click, message-box-free so it can be driven
// from automated tests. Returns 0 on success; negative codes mean "nothing
// to analyze yet" (caller should show a guidance message, not an error);
// positive codes are hard failures. curveLabel is filled with the analyzed
// curve's dataset name for the caller's confirmation message.
static int _fet_analyzer_analyze_active_core(string& curveLabel)
{
    GraphLayer gl;
    gl = Project.ActiveLayer();
    if (!gl)
        return -1;

    gl = _fet_prepare_graph_for_analysis(gl);
    DataPlot activePlot = _fet_get_analysis_plot_for_graph_layer(gl);
    if (!activePlot)
        return -2;
    curveLabel = activePlot.GetDatasetName();

    FETDialogOptions dialogOptions;
    _fet_get_effective_dialog_options(dialogOptions);
    bool hasForwardCursors = gl.GraphObjects(FET_CURSOR_SS_START)
                          && gl.GraphObjects(FET_CURSOR_SS_END)
                          && gl.GraphObjects(FET_CURSOR_VTH_START)
                          && gl.GraphObjects(FET_CURSOR_VTH_END);
    bool needsBackward = dialogOptions.scanMode == FET_SCAN_AUTO
                      || dialogOptions.scanMode == FET_SCAN_BACKWARD
                      || dialogOptions.scanMode == FET_SCAN_BOTH;
    bool hasBackwardCursors = gl.GraphObjects(FET_CURSOR_BWD_SS_START)
                           && gl.GraphObjects(FET_CURSOR_BWD_SS_END)
                           && gl.GraphObjects(FET_CURSOR_BWD_VTH_START)
                           && gl.GraphObjects(FET_CURSOR_BWD_VTH_END);
    // _fet_add_free_range_cursors() already runs a full
    // fet_analyzer_refresh_preview() once it places cursors. Calling it again
    // right after would append a second, duplicate set of summary rows for
    // the same click, so only call it explicitly when cursors already
    // existed and no analysis has happened yet this click.
    bool addedCursors = false;
    if (!hasForwardCursors || (needsBackward && !hasBackwardCursors))
    {
        if (!_fet_add_free_range_cursors(gl, activePlot))
            return -3;
        addedCursors = true;
    }

    return addedCursors ? 0 : fet_analyzer_refresh_preview();
}

int fet_analyzer_analyze_active_for_test()
{
    string curveLabel;
    return _fet_analyzer_analyze_active_core(curveLabel);
}

void fet_analyzer_analyze_active()
{
    string curveLabel;
    int nErr = _fet_analyzer_analyze_active_core(curveLabel);
    if (nErr == -1)
    {
        _fet_message("Activate a 2D Id-Vg graph before starting FET Gadget.",
                     MB_OK | MB_ICONEXCLAMATION);
        return;
    }
    if (nErr == -2)
    {
        _fet_message("Select an Id-Vg curve in the active graph first.",
                     MB_OK | MB_ICONEXCLAMATION);
        return;
    }
    if (nErr == -3)
    {
        _fet_message("FET Gadget could not auto-detect SS/Vth ranges on the active curve.",
                     MB_OK | MB_ICONSTOP);
        return;
    }
    if (nErr != 0)
    {
        string error;
        error.Format("Graph analysis failed (code %d). Check the active curve and cursor ranges.",
                     nErr);
        _fet_message(error, MB_OK | MB_ICONSTOP);
        return;
    }

    string doneMessage;
    doneMessage.Format(
        "Analysis complete for curve \"%s\".\n\n"
        "[+] is the forward acquisition segment and [-] is the backward segment.\n"
        "Solid forward cursors and dashed backward cursors remain draggable; "
        "moving them updates fits and parameters.",
        curveLabel);
    _fet_message(doneMessage);
}

static bool _fet_prompt_transfer_csv_files(string& fileList)
{
    StringArray files;
    int nFiles = GetMultiOpenBox(files, "[CSV Files (*.csv)] *.csv", NULL, NULL,
                                 "Select FET Transfer CSV File(s)", true);
    if (nFiles <= 0 || files.GetSize() <= 0)
        return false;

    fileList = "";
    for (int ii = 0; ii < files.GetSize(); ii++)
    {
        if (ii > 0)
            fileList += "|";
        fileList += files[ii];
    }
    return true;
}

static void _fet_show_import_failure(TreeNode& result)
{
    string error;
    error.Format("CSV import failed. Imported curves: %d; failed files: %d.\n\n"
                 "Expected instrument format:\n"
                 "DataName, Vg, Vd, Ig, Id, absId\n"
                 "DataValue, ...\n\n"
                 "Also supported: ordinary tables with Vg/Id columns.\n\n"
                 "%s",
                 result.Curves.nVal, result.FailedFiles.nVal,
                 result.FailedDetails.strVal);
    _fet_message(error, MB_OK | MB_ICONSTOP);
}

// Entry: import CSV file(s) into a worksheet. Selecting more than one file
// switches to the compact Vg/Id-only, multi-curve layout (no graph -- run
// multi-curve analysis on the resulting worksheet next); a single file keeps
// the full 6-column layout and also builds the usual single-curve graph.
void fet_analyzer_import_csv()
{
    string fileList;
    if (!_fet_prompt_transfer_csv_files(fileList))
        return;

    vector<string> paths;
    fileList.GetTokens(paths, '|');
    bool multi = paths.GetSize() > 1;

    Tree result;
    int nErr = fet_import_transfer_csv_files_ex(fileList, result, !multi, multi);
    if (nErr != 0)
    {
        _fet_show_import_failure(result);
        return;
    }

    string done;
    if (multi)
        done.Format("Imported %d FET curve(s) into a compact Vg/Id workbook "
                    "(\"%s\").\n\nRun Multi-curve analysis next -- it reads "
                    "this active worksheet directly.",
                    result.Curves.nVal, result.Workbook.strVal);
    else
        done.Format("Imported %d FET curve(s).\n\n"
                    "A workbook and double-Y Id-Vg graph were created.",
                    result.Curves.nVal);
    _fet_message(done);
}

// Entry: unified multi-curve analysis. Reads whichever worksheet or graph is
// currently active (never prompts for files), batch-extracts parameters from
// every curve, and builds the overlay graph and the histogram/statistics
// graph together in one pass.
void fet_analyzer_multi_analyze()
{
    WorksheetPage book = _fet_find_multi_source_book();
    if (!book)
    {
        _fet_message("Activate the imported multi-curve worksheet (or its "
                     "overlay/statistics graph) before running multi-curve "
                     "analysis.", MB_OK | MB_ICONEXCLAMATION);
        return;
    }
    if (!_fet_get_multi_options())
        return;

    string summary;
    int nErr = _fet_multi_analyze_core(book, summary);
    if (nErr != 0 && nErr != 5)
    {
        if (nErr == 3)
            _fet_message(summary, MB_OK | MB_ICONEXCLAMATION);
        else
        {
            string error;
            error.Format("Multi-curve analysis failed (code %d).", nErr);
            _fet_message(error, MB_OK | MB_ICONSTOP);
        }
        return;
    }
    _fet_message(summary);
}

int fet_analyzer_multi_analyze_for_test(LPCSTR lpcszBook)
{
    WorksheetPage book(lpcszBook);
    string summary;
    return _fet_multi_analyze_core(book, summary);
}

// Exercises _fet_find_multi_source_book() against whatever graph layer is
// currently active, the same call path the [FET Multi] button uses -- this
// is what proves the hidden source-book tag (not dataset-name matching)
// resolves the original import workbook from a graph that plots derived
// data. Returns the resolved workbook's curve count, or -1 if none found.
int fet_analyzer_find_multi_source_book_curve_count_for_test()
{
    WorksheetPage book = _fet_find_multi_source_book();
    if (!book)
        return -1;
    Worksheet curves = book.Layers("Curves");
    if (!curves)
        return -1;
    int colsPerCurve = _fet_detect_curve_layout(curves);
    return curves.GetNumCols() / colsPerCurve;
}

// Test hooks for the scatter+histograms and correlation-matrix builders,
// bypassing their settings dialogs (GetNBox blocks for interactive input,
// which a headless smoke test can't drive).
int fet_analyzer_scatter_hist_for_test(int xParamIdx, int yParamIdx)
{
    string summary;
    return _fet_build_scatter_hist_graph(xParamIdx, yParamIdx, summary) ? 0 : 1;
}

int fet_analyzer_correlation_matrix_for_test()
{
    vector<int> selected = { 0, 1, 3 };
    string summary;
    return _fet_build_correlation_matrix(selected, summary);
}

// [Prev]/[Next] button handlers on the statistics graph: only re-render from
// the already-computed [FETStatsData]Statistics/Histogram sheets, never
// re-run curve fitting.
void fet_analyzer_stats_prev_param()
{
    GraphLayer gl;
    gl = Project.ActiveLayer();
    if (!gl)
        return;
    g_fet_stats_current_param = (g_fet_stats_current_param + FET_STATS_PARAM_COUNT - 1)
                               % FET_STATS_PARAM_COUNT;
    _fet_render_stats_param(gl, g_fet_stats_current_param);
    gl.GetPage().Refresh();
}

void fet_analyzer_stats_next_param()
{
    GraphLayer gl;
    gl = Project.ActiveLayer();
    if (!gl)
        return;
    g_fet_stats_current_param = (g_fet_stats_current_param + 1) % FET_STATS_PARAM_COUNT;
    _fet_render_stats_param(gl, g_fet_stats_current_param);
    gl.GetPage().Refresh();
}

// Entry: scatter plot of two batch-result parameters with marginal
// histograms on each axis. Reads [FETStatsData]Parameters -- run Multi-Curve
// Analysis at least once first.
void fet_analyzer_scatter_hist()
{
    WorksheetPage statsBook(FET_STATS_DATA_PAGE);
    Worksheet paramsWks;
    if (statsBook)
        paramsWks = statsBook.Layers("Parameters");
    if (!paramsWks || paramsWks.GetNumRows() < 1)
    {
        _fet_message("Run Multi-Curve Analysis first -- this reads its "
                     "[FETStatsData]Parameters table.",
                     MB_OK | MB_ICONEXCLAMATION);
        return;
    }
    if (!_fet_get_scatter_options())
        return;

    string summary;
    if (!_fet_build_scatter_hist_graph(g_fet_scatter_x_param, g_fet_scatter_y_param,
                                       summary))
    {
        _fet_message("Not enough curves with both parameters present to plot "
                     "(need at least 2).", MB_OK | MB_ICONEXCLAMATION);
        return;
    }
    _fet_message(summary);
}

// Entry: pairwise Pearson correlation table over selected batch-result
// parameters. Also reads [FETStatsData]Parameters.
void fet_analyzer_correlation_matrix()
{
    WorksheetPage statsBook(FET_STATS_DATA_PAGE);
    Worksheet paramsWks;
    if (statsBook)
        paramsWks = statsBook.Layers("Parameters");
    if (!paramsWks || paramsWks.GetNumRows() < 1)
    {
        _fet_message("Run Multi-Curve Analysis first -- this reads its "
                     "[FETStatsData]Parameters table.",
                     MB_OK | MB_ICONEXCLAMATION);
        return;
    }

    vector<int> selected;
    if (!_fet_get_correlation_options(selected))
        return;
    if (selected.GetSize() < 2)
    {
        _fet_message("Select at least two parameters.", MB_OK | MB_ICONEXCLAMATION);
        return;
    }

    string summary;
    int nErr = _fet_build_correlation_matrix(selected, summary);
    if (nErr != 0)
    {
        string error;
        error.Format("Correlation matrix failed (code %d).", nErr);
        _fet_message(error, MB_OK | MB_ICONSTOP);
        return;
    }
    _fet_message(summary);
}

int fet_analyzer_get_launch_mode()
{
    Worksheet wks;
    wks = Project.ActiveLayer();
    if (wks)
        return FET_LAUNCH_IMPORT_CSV;

    GraphLayer gl;
    gl = Project.ActiveLayer();
    if (gl)
        return FET_LAUNCH_GRAPH_ANALYSIS;

    return FET_LAUNCH_IMPORT_CSV;
}

// Real GetNBox dialogs (ordinary Windows dialog windows, not a graph page --
// an earlier version drew the menu as buttons on a hidden-axes graph, which
// read as "the App just opened a plot" rather than a menu and was reverted;
// a later version used a GETN_LIST dropdown instead of buttons, which wasn't
// what was wanted either). GetNBox's custom-button mechanism tops out at 4
// extra buttons beyond OK/Cancel (GETNE_ON_CUSTOM_BUTTON5 does not exist,
// confirmed by probe) -- one short of this App's 5 operations -- so the 2
// newer, less-common operations sit behind a "More..." button that opens a
// second small button dialog, keeping every level real buttons.
static int s_fet_start_action = 0;

static int _fet_more_dialog_event(TreeNode& tr, int nRow, int nEvent,
                                  DWORD& dwEnables, LPCSTR lpcszNodeName,
                                  WndContainer& getNContainer,
                                  string& strAux, string& strErrMsg)
{
    if (nEvent == GETNE_ON_CUSTOM_BUTTON1)
    {
        s_fet_start_action = FET_LAUNCH_SCATTER_HIST;
        return PEVENT_GETN_RET_TO_CLOSE;
    }
    if (nEvent == GETNE_ON_CUSTOM_BUTTON2)
    {
        s_fet_start_action = FET_LAUNCH_CORRELATION_MATRIX;
        return PEVENT_GETN_RET_TO_CLOSE;
    }
    return 0;
}

static int _fet_show_more_dialog()
{
    GETN_BOX(menu)
    GETN_STR(Hint, "Choose an operation:", "") GETN_HINT
    GETN_CUSTOM_BUTTON("OK=|Cancel=Back|Scatter + Histograms|Correlation Matrix")
    GetNBox(menu, _fet_more_dialog_event, FET_APP_TITLE, "");
    return s_fet_start_action;
}

static int _fet_start_dialog_event(TreeNode& tr, int nRow, int nEvent,
                                   DWORD& dwEnables, LPCSTR lpcszNodeName,
                                   WndContainer& getNContainer,
                                   string& strAux, string& strErrMsg)
{
    if (nEvent == GETNE_ON_CUSTOM_BUTTON1)
    {
        s_fet_start_action = FET_LAUNCH_IMPORT_CSV;
        return PEVENT_GETN_RET_TO_CLOSE;
    }
    if (nEvent == GETNE_ON_CUSTOM_BUTTON2)
    {
        s_fet_start_action = FET_LAUNCH_GRAPH_ANALYSIS;
        return PEVENT_GETN_RET_TO_CLOSE;
    }
    if (nEvent == GETNE_ON_CUSTOM_BUTTON3)
    {
        s_fet_start_action = FET_LAUNCH_MULTI_ANALYZE;
        return PEVENT_GETN_RET_TO_CLOSE;
    }
    if (nEvent == GETNE_ON_CUSTOM_BUTTON4)
    {
        s_fet_start_action = -1;  // "More..." -- open the sub-dialog
        return PEVENT_GETN_RET_TO_CLOSE;
    }
    return 0;
}

static int _fet_show_start_dialog()
{
    s_fet_start_action = 0;
    GETN_BOX(menu)
    GETN_STR(Hint, "Choose an operation:", "") GETN_HINT
    GETN_CUSTOM_BUTTON("OK=|Cancel=Close|Import|Single-Curve Analysis|Multi-Curve Analysis|More...")
    GetNBox(menu, _fet_start_dialog_event, FET_APP_TITLE, "");
    if (s_fet_start_action == -1)
        return _fet_show_more_dialog();
    return s_fet_start_action;
}

void fet_analyzer_start()
{
    int mode = _fet_show_start_dialog();
    if (mode == FET_LAUNCH_IMPORT_CSV)
        fet_analyzer_import_csv();
    else if (mode == FET_LAUNCH_GRAPH_ANALYSIS)
        fet_analyzer_analyze_active();
    else if (mode == FET_LAUNCH_MULTI_ANALYZE)
        fet_analyzer_multi_analyze();
    else if (mode == FET_LAUNCH_SCATTER_HIST)
        fet_analyzer_scatter_hist();
    else if (mode == FET_LAUNCH_CORRELATION_MATRIX)
        fet_analyzer_correlation_matrix();
}
