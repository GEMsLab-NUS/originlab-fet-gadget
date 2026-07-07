/*
 * FET Gadget MVP for Origin 2022+ / Origin 10.x.
 * Core runtime: Origin C. LabTalk is used only for App launch, graph object
 * cursor glue, and small graph-formatting operations.
 */

#include <Origin.h>
#include <GetNBox.h>
#include <xfutils.h>
#include <sys_utils.h>

#define FET_APP_TITLE "FET Gadget v0.6"
#define FET_MIN_POINTS 3
#define FET_IMPORT_COLS_PER_CURVE 6
#define FET_LAUNCH_IMPORT_CSV 1
#define FET_LAUNCH_GRAPH_ANALYSIS 2
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
#define FET_FIT_EXTENSION 0.15
#define FET_HYST_CURSOR "FET_HYST"
#define FET_GRAPH_DATA_PAGE "FETGraphData"

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
    int logCurveColorPreset;
    int linearCurveColorPreset;
    int markerColorPreset;
} FETDialogOptions;

static void _fet_add_config_button(GraphLayer& gl);
static int _fet_rgb_color(int red, int green, int blue);
static bool _fet_is_helper_plot(DataPlot& plot);
static void _fet_style_plot(DataPlot& plot, int color, bool symbols,
                            int lineStyle = LINE_STYLE_SOLID,
                            bool doubleCompound = false);
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

static int _fet_color_preset(int group, int preset, int currentColor)
{
    if (preset <= 0)
        return currentColor;

    if (group == 0)
    {
        if (preset == 1)
            return _fet_rgb_color(67, 56, 202);
        if (preset == 2)
            return _fet_rgb_color(8, 145, 178);
        if (preset == 3)
            return _fet_rgb_color(37, 99, 235);
        if (preset == 4)
            return _fet_rgb_color(126, 34, 206);
    }
    if (group == 1)
    {
        if (preset == 1)
            return _fet_rgb_color(217, 119, 6);
        if (preset == 2)
            return _fet_rgb_color(225, 29, 72);
        if (preset == 3)
            return _fet_rgb_color(202, 138, 4);
        if (preset == 4)
            return _fet_rgb_color(22, 163, 74);
    }
    if (group == 2)
    {
        if (preset == 1)
            return _fet_rgb_color(194, 65, 61);
        if (preset == 2)
            return _fet_rgb_color(79, 70, 229);
        if (preset == 3)
            return _fet_rgb_color(245, 158, 11);
        if (preset == 4)
            return _fet_rgb_color(75, 85, 99);
    }
    return currentColor;
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
        "Vg (V)", "Vg[V]", "SMU4 V", "SMU4 Voltage"
    };
    return _fet_find_any_column(names, aliases, startIndex);
}

static int _fet_find_vd_column(const vector<string>& names, int startIndex = 0)
{
    vector<string> aliases = {
        "Vd", "Vd_V", "VD", "DrainV", "Drain_V", "DrainVoltage",
        "Drain Voltage", "Vdrain", "Vdrain_V", "Drain Voltage (V)",
        "Drain Voltage[V]", "Vd (V)", "Vd[V]", "SMU2 V", "SMU2 Voltage"
    };
    return _fet_find_any_column(names, aliases, startIndex);
}

static int _fet_find_ig_column(const vector<string>& names, int startIndex = 0)
{
    vector<string> aliases = {
        "Ig", "Ig_A", "IG", "GateI", "Gate_I", "GateCurrent",
        "Gate Current", "Igate", "Igate_A", "Gate Current (A)",
        "Gate Current[A]", "Ig (A)", "Ig[A]", "SMU4 I", "SMU4 Current"
    };
    return _fet_find_any_column(names, aliases, startIndex);
}

static int _fet_find_id_column(const vector<string>& names, int startIndex = 0)
{
    vector<string> aliases = {
        "Id", "Id_A", "ID", "DrainI", "Drain_I", "DrainCurrent",
        "Drain Current", "Idrain", "Idrain_A", "Drain Current (A)",
        "Drain Current[A]", "Id (A)", "Id[A]", "SMU2 I", "SMU2 Current"
    };
    return _fet_find_any_column(names, aliases, startIndex);
}

static int _fet_find_absid_column(const vector<string>& names, int startIndex = 0)
{
    vector<string> aliases = {
        "absId", "absId_A", "AbsId", "AbsId_A", "Abs(Id)", "Abs Id",
        "|Id|", "absDrainCurrent", "abs(Id) (A)", "absId (A)", "absId[A]"
    };
    return _fet_find_any_column(names, aliases, startIndex);
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

static void _fet_import_error_text(int code, string& text)
{
    if (code == 0)
        text = "no Vg/Id data rows were found";
    else if (code == -1)
        text = "file could not be opened";
    else if (code == -2)
        text = "DataName/header row did not contain recognized Vg and Id columns";
    else
        text.Format("error %d", code);
}

static bool _fet_prepare_import_context(FETImportContext& ctx, bool makeGraph)
{
    WorksheetPage page;
    page.Create("Origin");
    page.SetName("FETImportedData", OCD_ENUM_NEXT);
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

    if (makeGraph)
    {
        GraphPage gp;
        gp.Create("DOUBLEY");
        gp.SetName("FETImportedGraph", OCD_ENUM_NEXT);
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
                                          LPCSTR label)
{
    vector<string> names = { "Vg", "Id", "Ig", "absId", "Vd", "logAbsId" };
    vector<string> units = { "V", "A", "A", "A", "V", "log10(A)" };
    for (int ii = 0; ii < FET_IMPORT_COLS_PER_CURVE; ii++)
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

static void _fet_set_graph_page_ratio(GraphLayer& gl)
{
    if (!gl)
        return;
    GraphPage gp = gl.GetPage();
    if (gp)
        gp.Resize(420, 420, 101);
    gl.LT_execute("page.kar=0;page.width=4560;page.height=4560;layer.unit=0;layer.left=14;layer.top=10;layer.width=76;layer.height=76;");
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
                if (!firstHiddenSource)
                    firstHiddenSource = candidate;
                if (!selectedDataset.IsEmpty()
                    && candidateDataset.CompareNoCase(selectedDataset) == 0)
                    return candidate;
            }
            else if (!firstVisibleSource)
            {
                firstVisibleSource = candidate;
            }
        }
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
            if (firstHiddenSource)
                return firstHiddenSource;
            if (largestSource)
                return largestSource;
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

static int _fet_add_segmented_visible_plots(GraphLayer& gl, Worksheet& wks,
                                            int xColumn, int yColumn,
                                            int rowCount, const vector& vx,
                                            int color, int axis)
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
    _fet_style_plot(forwardPlot, color, false, LINE_STYLE_SOLID);
    _fet_attach_plot_to_axis(forwardPlot, axis);
    int added = forwardPlot ? 1 : 0;

    if (hasBackward)
    {
        DataRange backwardRange;
        backwardRange.Add(wks, xColumn, "X", xColumn, turnIndex, rowCount - 1);
        backwardRange.Add(wks, yColumn, "Y", yColumn, turnIndex, rowCount - 1);
        int backwardIndex = gl.AddPlot(backwardRange, IDM_PLOT_LINE);
        DataPlot backwardPlot = gl.DataPlots(backwardIndex);
        _fet_style_plot(backwardPlot, color, false, LINE_STYLE_SOLID, true);
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

    FETDialogOptions graphOptions;
    _fet_get_effective_dialog_options(graphOptions);
    _fet_add_segmented_visible_plots(leftLayer, wks, 8, 11, vx.GetSize(), vx,
                                     graphOptions.logCurveColor, FET_AXIS_LEFT);
    _fet_add_segmented_visible_plots(rightLayer, wks, 8, 9, vx.GetSize(), vx,
                                     graphOptions.linearCurveColor, FET_AXIS_RIGHT);
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
    int baseCol = curveIndex * FET_IMPORT_COLS_PER_CURVE;
    if (nRows > ctx.maxRows)
        ctx.maxRows = nRows;
    ctx.curves.SetSize(ctx.maxRows, baseCol + FET_IMPORT_COLS_PER_CURVE);

    string label = GetFileName(filePath, true);
    if (blockIndex > 1)
    {
        string suffix;
        suffix.Format(" #%d", blockIndex);
        label += suffix;
    }
    _fet_set_import_column_labels(ctx.curves, baseCol, label);

    for (int rr = 0; rr < nRows; rr++)
    {
        ctx.curves.SetCell(rr, baseCol, vVg[rr]);
        ctx.curves.SetCell(rr, baseCol + 1, vId[rr]);
        ctx.curves.SetCell(rr, baseCol + 2, rr < vIg.GetSize() ? vIg[rr] : NANUM);
        ctx.curves.SetCell(rr, baseCol + 3, rr < vAbsId.GetSize() ? vAbsId[rr] : fabs(vId[rr]));
        ctx.curves.SetCell(rr, baseCol + 4, rr < vVd.GetSize() ? vVd[rr] : NANUM);
        double absValue = rr < vAbsId.GetSize() ? vAbsId[rr] : fabs(vId[rr]);
        ctx.curves.SetCell(rr, baseCol + 5, absValue > 0 ? log10(absValue) : NANUM);
    }

    if (ctx.makeGraph && ctx.graphLayer && ctx.linearGraphLayer)
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
            blockIndex++;
            inData = true;
            requireDataValue = true;
            continue;
        }

        if (!inData)
        {
            vector<string> headerTokens;
            _fet_split_csv_line(line, headerTokens);
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
            if (splitScanColumns)
            {
                idxVg = idxFwdVg;
                idxId = idxFwdId;
                idxAbsId = _fet_find_directional_absid_column(headerTokens, "Forward");
            }
            if (idxVg >= 0 && idxId >= 0)
            {
                blockIndex++;
                inData = true;
                requireDataValue = false;
                continue;
            }

            _fet_append_import_meta(ctx, filePath, lineNumber, line);
            continue;
        }

        if (!requireDataValue || _fet_starts_with_token(line, "DataValue"))
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

    if (inData)
        _fet_flush_import_scan_block(ctx, filePath, blockIndex,
                                     vVg, vVd, vIg, vId, vAbsId,
                                     vBwdVg, vBwdVd, vBwdIg,
                                     vBwdId, vBwdAbsId);
    return ctx.curveCount - curvesBefore;
}

int fet_import_transfer_csv_files(LPCSTR lpcszFiles, TreeNode& resultTree,
                                  bool makeGraph = true)
{
    string files(lpcszFiles);
    if (files.IsEmpty())
        return 1;

    vector<string> paths;
    files.GetTokens(paths, '|');
    if (paths.GetSize() < 1)
        return 1;

    FETImportContext ctx;
    if (!_fet_prepare_import_context(ctx, makeGraph))
        return 2;

    int failed = 0;
    string failedDetails;
    for (int ii = 0; ii < paths.GetSize(); ii++)
    {
        _fet_trim_string(paths[ii]);
        if (paths[ii].IsEmpty())
            continue;
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
        gl.LT_execute("label -p 88 12 -j 1 -n FET_CONFIG [FET Gadget];");
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
                            int lineStyle, bool doubleCompound)
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
        tr.Root.Line.Width.dVal = 2.0;
        tr.Root.Line.Style.nVal = lineStyle;
        if (doubleCompound)
            tr.Root.Line.Compound.nVal = 1;
    }
    if (0 == plot.UpdateThemeIDs(tr.Root))
        plot.ApplyFormat(tr, true, true);
    if (!symbols)
    {
        plot.SetLineType(lineStyle);
        if (doubleCompound)
        {
            int compound = 1;
            plot.LabTalk("line.compound", &compound, true);
        }
    }
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

static Worksheet _fet_graph_data_sheet(LPCSTR sheetName, int minCols,
                                       bool clearExisting)
{
    WorksheetPage page(FET_GRAPH_DATA_PAGE);
    bool created = false;
    if (!page)
    {
        page.Create("Origin");
        page.SetName(FET_GRAPH_DATA_PAGE, OCD_ENUM_NEXT);
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

static bool _fet_hysteresis_default_level(const vector& vy,
                                          int turnIndex,
                                          double& targetLog)
{
    double forwardMin = 1e300, forwardMax = -1e300;
    double backwardMin = 1e300, backwardMax = -1e300;
    int ii;
    for (ii = 0; ii <= turnIndex; ii++)
    {
        double value = fabs(vy[ii]);
        if (value > 0)
        {
            double lv = log10(value);
            forwardMin = min(forwardMin, lv);
            forwardMax = max(forwardMax, lv);
        }
    }
    for (ii = turnIndex; ii < vy.GetSize(); ii++)
    {
        double value = fabs(vy[ii]);
        if (value > 0)
        {
            double lv = log10(value);
            backwardMin = min(backwardMin, lv);
            backwardMax = max(backwardMax, lv);
        }
    }
    double low = max(forwardMin, backwardMin);
    double high = min(forwardMax, backwardMax);
    if (!_fet_valid_number(low) || !_fet_valid_number(high) || high <= low)
        return false;
    targetLog = (low + high) / 2.0;
    return true;
}

static bool _fet_update_hysteresis_cursor(GraphLayer& gl,
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

    if (!options.showHysteresisCursor || turnIndex < FET_MIN_POINTS)
    {
        _fet_delete_graph_object(gl, FET_HYST_CURSOR);
        return false;
    }

    double targetLog = NANUM;
    if (!_fet_hysteresis_default_level(vy, turnIndex, targetLog))
        return false;

    double inputMin = vx[0];
    double inputMax = vx[0];
    for (int ii = 1; ii < vx.GetSize(); ii++)
    {
        inputMin = min(inputMin, vx[ii]);
        inputMax = max(inputMax, vx[ii]);
    }
    _fet_add_scale_line(gl, FET_HYST_CURSOR, inputMin, targetLog,
                        inputMax, targetLog, options.markerColor,
                        LINE_STYLE_SHORT_DASH, false, false, true, true);

    double forwardVg, backwardVg;
    if (!_fet_find_vg_at_log_current(vx, vy, 0, turnIndex,
                                     targetLog, forwardVg))
        return false;
    if (!_fet_find_vg_at_log_current(vx, vy, turnIndex, vx.GetSize() - 1,
                                     targetLog, backwardVg))
        return false;

    result.hysteresis_level_logA = targetLog;
    result.hysteresis_forward_Vg = forwardVg;
    result.hysteresis_backward_Vg = backwardVg;
    result.hysteresis_delta_Vg = fabs(backwardVg - forwardVg);
    return true;
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
    Worksheet wks = _fet_graph_data_sheet("Extracted Parameters", 33, false);
    if (!wks)
        return wks;

    vector<string> names = {
        "Graph", "Source Range", "SS Range", "Vth Range", "SS", "SS R-Square",
        "Vth", "Vth R-Square", "gm", "gm Vg", "Mobility", "Ion", "Ioff",
        "Ion/Ioff", "L", "W", "Cox", "Cox Mode", "Oxide Thickness",
        "Kappa", "Vd", "Ioff Vg", "Ion Vg", "Hyst Level logA",
        "Hyst Forward Vg", "Hyst Backward Vg", "Hyst Delta Vg",
        "SS Slope", "SS Intercept", "Vth Slope", "Vth Intercept",
        "Gate Leakage", "Curve"
    };
    vector<string> units = {
        "", "", "", "", "mV/dec", "", "V", "", "S", "V",
        "cm^2/(V s)", "A", "A", "", "um", "um", "F/cm^2", "",
        "nm", "", "V", "V", "V", "", "V", "V", "V", "dec/V",
        "", "A/V", "A", "", ""
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
    wks.Columns(32).GetStringArray(sa);

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
        wks.SetSize(row + 1, max(wks.GetNumCols(), 33));

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
    wks.SetCell(row, 27, result.ss_slope_dec_V);
    wks.SetCell(row, 28, result.ss_intercept);
    wks.SetCell(row, 29, result.vth_slope_A_V);
    wks.SetCell(row, 30, result.vth_intercept_A);
    wks.SetCell(row, 31, result.gate_warning);
    wks.SetCell(row, 32, curveLabel);
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
        _fet_delete_graph_object(leftLayer, FET_HYST_CURSOR);
    _fet_write_graph_curves_data(vx, vy, turnIndex, hasBackward);

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
        if (hasBackward && useBackward)
            _fet_update_hysteresis_cursor(leftLayer, vx, vy, turnIndex,
                                          dialogOptions, result);
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
    options.logCurveColorPreset = 0;
    options.linearCurveColorPreset = 0;
    options.markerColorPreset = 0;
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
    GETN_BEGIN_BRANCH(graph, "Graph")
        GETN_CHECK(RefreshGraphStyle, "Apply logAbsId/Id layer formatting", options.refreshGraphStyle)
        GETN_CHECK(ShowFitLines, "Show extended SS and gm-range fit lines", options.showFitLines)
        GETN_CHECK(ShowMarkers, "Show circular Vth, Ion and gm markers", options.showMarkers)
        GETN_CHECK(ShowOnOffArrow, "Show Ion reference and movable Ioff cursor", options.showOnOffArrow)
        GETN_CHECK(ShowHysteresisCursor, "Show hysteresis opening cursor", options.showHysteresisCursor)
        GETN_LIST(LogCurveColorPreset, "logAbsId preset", options.logCurveColorPreset, "Manual below|Indigo|Cyan|Blue|Purple")
        GETN_COLOR(LogCurveColor, "logAbsId curve / left axis", options.logCurveColor)
        GETN_LIST(LinearCurveColorPreset, "Id preset", options.linearCurveColorPreset, "Manual below|Amber|Rose|Gold|Green")
        GETN_COLOR(LinearCurveColor, "Id curve / right axis", options.linearCurveColor)
        GETN_LIST(MarkerColorPreset, "Marker/cursor preset", options.markerColorPreset, "Manual below|Red clay|Indigo|Amber|Slate")
        GETN_COLOR(MarkerColor, "Ion/Ioff cursor and gm markers", options.markerColor)
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
    options.logCurveColorPreset = settings.graph.LogCurveColorPreset.nVal;
    options.linearCurveColorPreset = settings.graph.LinearCurveColorPreset.nVal;
    options.markerColorPreset = settings.graph.MarkerColorPreset.nVal;
    options.logCurveColor = settings.graph.LogCurveColor.nVal;
    options.linearCurveColor = settings.graph.LinearCurveColor.nVal;
    options.markerColor = settings.graph.MarkerColor.nVal;
    options.logCurveColor = _fet_color_preset(0, options.logCurveColorPreset,
                                              options.logCurveColor);
    options.linearCurveColor = _fet_color_preset(1, options.linearCurveColorPreset,
                                                 options.linearCurveColor);
    options.markerColor = _fet_color_preset(2, options.markerColorPreset,
                                            options.markerColor);
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

static void _fet_analyzer_import_csv(bool makeGraph)
{
    StringArray files;
    int nFiles = GetMultiOpenBox(files, "[CSV Files (*.csv)] *.csv", NULL, NULL,
                                 "Select FET Transfer CSV File(s)", true);
    if (nFiles <= 0 || files.GetSize() <= 0)
        return;

    string fileList;
    for (int ii = 0; ii < files.GetSize(); ii++)
    {
        if (ii > 0)
            fileList += "|";
        fileList += files[ii];
    }

    Tree result;
    int nErr = fet_import_transfer_csv_files(fileList, result, makeGraph);
    if (nErr != 0)
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
        return;
    }

    string done;
    if (makeGraph)
        done.Format("Imported %d FET curve(s).\n\n"
                    "A workbook and double-Y Id-Vg graph were created.",
                    result.Curves.nVal);
    else
        done.Format("Imported %d FET curve(s).\n\n"
                    "A workbook was created. No graph or analysis was generated.",
                    result.Curves.nVal);
    _fet_message(done);
}

void fet_analyzer_import_csv()
{
    _fet_analyzer_import_csv(false);
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

static int s_fet_start_action = 0;

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
    return 0;
}

static int _fet_show_start_dialog()
{
    s_fet_start_action = 0;
    GETN_BOX(menu)
    GETN_STR(ChooseAction, "Choose one operation.", "") GETN_HINT
    GETN_CUSTOM_BUTTON("OK=|Cancel=Close|Import Data|Analyze Graph")
    GetNBox(menu, _fet_start_dialog_event, FET_APP_TITLE,
            "Import Data creates worksheets only. Analyze Graph requires an active graph.");
    return s_fet_start_action;
}

void fet_analyzer_start()
{
    int mode = _fet_show_start_dialog();
    if (mode == FET_LAUNCH_GRAPH_ANALYSIS)
        fet_analyzer_analyze_active();
    else if (mode == FET_LAUNCH_IMPORT_CSV)
        fet_analyzer_import_csv();
}
