#include <Origin.h>

int fet_analyze_xf_core(const XYRange& input, const XYRange& ssRange,
                        const XYRange& vthRange, double L_um, double W_um,
                        double Cox, double Vd, bool annotate, bool output,
                        TreeNode& resultTree);
int fet_import_transfer_csv_files(LPCSTR lpcszFiles, TreeNode& resultTree,
                                  bool makeGraph = true);
int fet_analyzer_get_launch_mode();
int fet_analyzer_auto_ranges_for_plot(DataPlot& plot, XYRange& ssRange,
                                      XYRange& vthRange);
int fet_analyzer_place_free_cursors_for_plot(DataPlot& plot);
int fet_analyzer_ranges_from_free_cursors(DataPlot& plot, XYRange& ssRange,
                                          XYRange& vthRange);
int fet_analyzer_refresh_preview();
int fet_analyzer_detect_scan_for_plot(DataPlot& plot, TreeNode& resultTree);
int fet_analyzer_reapply_graph_style_for_test();
int fet_analyzer_prepare_active_graph_for_test();
void fet_analyzer_reset_options_for_test();
void fet_analyzer_set_scan_mode_for_test(int scanMode);
int fet_analyzer_analyze_active_for_test();

static int _fet_smoke_check_double_y(GraphLayer& leftLayer,
                                     GraphLayer& rightLayer,
                                     int leftPlotIndex,
                                     int rightPlotIndex)
{
    if (!leftLayer || !rightLayer)
        return 1;

    DataPlot leftPlot = leftLayer.DataPlots(leftPlotIndex);
    DataPlot rightPlot = rightLayer.DataPlots(rightPlotIndex);
    if (!leftPlot || !rightPlot)
        return 4;
    if (leftPlot.GetAttachedAxis(true) != 0)
        return 5;
    if (rightPlot.GetAttachedAxis(true) != 1)
        return 6;
    return 0;
}

static int _fet_smoke_check_log_curve(GraphLayer& leftLayer,
                                      GraphLayer& rightLayer,
                                      int leftPlotIndex,
                                      int rightPlotIndex)
{
    DataPlot leftPlot = leftLayer.DataPlots(leftPlotIndex);
    DataPlot rightPlot = rightLayer.DataPlots(rightPlotIndex);
    if (!leftPlot || !rightPlot)
        return 1;
    XYRange leftRange, rightRange;
    leftPlot.GetDataRange(leftRange, 0, -1);
    rightPlot.GetDataRange(rightRange, 0, -1);
    vector leftX, leftY, rightX, rightY;
    if (!leftRange.GetData(leftY, leftX)
        || !rightRange.GetData(rightY, rightX))
        return 2;
    int n = min(leftY.GetSize(), rightY.GetSize());
    for (int ii = 0; ii < n; ii++)
    {
        double current = fabs(rightY[ii]);
        if (current > 0 && !is_missing_value(leftY[ii]))
            return fabs(leftY[ii] - log10(current)) < 1e-9 ? 0 : 3;
    }
    return 4;
}

static bool _fet_smoke_column_has_value(Worksheet& wks, int colIndex)
{
    int rows = wks.GetNumRows();
    if (rows <= 0)
        return false;
    XYRange range;
    range.Add(wks, colIndex, "X", colIndex, 0, rows - 1);
    range.Add(wks, colIndex, "Y", colIndex, 0, rows - 1);
    vector vx, vy;
    if (!range.GetData(vy, vx))
        return false;
    for (int rr = 0; rr < vy.GetSize(); rr++)
    {
        if (!is_missing_value(vy[rr]))
            return true;
    }
    return false;
}

static int _fet_smoke_find_summary_row(Worksheet& wks, LPCSTR key)
{
    int rows = wks.GetNumRows();
    if (rows <= 0)
        return -1;
    StringArray sa;
    wks.Columns(32).GetStringArray(sa);
    for (int rr = 0; rr < rows; rr++)
    {
        string cell = rr < sa.GetSize() ? sa[rr] : "";
        if (cell.CompareNoCase(key) == 0)
            return rr;
    }
    return -1;
}

string g_fet_smoke_csv_path;
string g_fet_smoke_table_csv_path;
string g_fet_smoke_double_csv_path;
string g_fet_smoke_split_csv_path;
string g_fet_smoke_actual_csv_path_1;
string g_fet_smoke_actual_csv_path_2;
string g_fet_smoke_actual_csv_path_3;
string g_fet_smoke_actual_idvg_csv_path;

void fet_analyzer_smoke_set_csv(LPCSTR lpcszPath)
{
    g_fet_smoke_csv_path = lpcszPath;
}

void fet_analyzer_smoke_set_table_csv(LPCSTR lpcszPath)
{
    g_fet_smoke_table_csv_path = lpcszPath;
}

void fet_analyzer_smoke_set_actual_csv_1(LPCSTR lpcszPath)
{
    g_fet_smoke_actual_csv_path_1 = lpcszPath;
}

void fet_analyzer_smoke_set_actual_csv_2(LPCSTR lpcszPath)
{
    g_fet_smoke_actual_csv_path_2 = lpcszPath;
}

void fet_analyzer_smoke_set_actual_csv_3(LPCSTR lpcszPath)
{
    g_fet_smoke_actual_csv_path_3 = lpcszPath;
}

int fet_analyzer_runtime_smoke()
{
    fet_analyzer_reset_options_for_test();

    char smokeCsvPath[4096], smokeTableCsvPath[4096];
    LT_get_str("__FET_SMOKE_CSV$", smokeCsvPath, 4096);
    LT_get_str("__FET_SMOKE_TABLE_CSV$", smokeTableCsvPath, 4096);
    if (strlen(smokeCsvPath) > 0)
        g_fet_smoke_csv_path = smokeCsvPath;
    if (strlen(smokeTableCsvPath) > 0)
        g_fet_smoke_table_csv_path = smokeTableCsvPath;
    char smokeDoubleCsvPath[4096], smokeSplitCsvPath[4096];
    LT_get_str("__FET_SMOKE_DOUBLE_CSV$", smokeDoubleCsvPath, 4096);
    LT_get_str("__FET_SMOKE_SPLIT_CSV$", smokeSplitCsvPath, 4096);
    if (strlen(smokeDoubleCsvPath) > 0)
        g_fet_smoke_double_csv_path = smokeDoubleCsvPath;
    if (strlen(smokeSplitCsvPath) > 0)
        g_fet_smoke_split_csv_path = smokeSplitCsvPath;

    char actualCsvPath1[4096], actualCsvPath2[4096], actualCsvPath3[4096];
    LT_get_str("__FET_ACTUAL_CSV_1$", actualCsvPath1, 4096);
    LT_get_str("__FET_ACTUAL_CSV_2$", actualCsvPath2, 4096);
    LT_get_str("__FET_ACTUAL_CSV_3$", actualCsvPath3, 4096);
    if (strlen(actualCsvPath1) > 0)
        g_fet_smoke_actual_csv_path_1 = actualCsvPath1;
    if (strlen(actualCsvPath2) > 0)
        g_fet_smoke_actual_csv_path_2 = actualCsvPath2;
    if (strlen(actualCsvPath3) > 0)
        g_fet_smoke_actual_csv_path_3 = actualCsvPath3;
    char actualIdVgCsvPath[4096];
    LT_get_str("__FET_ACTUAL_IDVG_CSV$", actualIdVgCsvPath, 4096);
    if (strlen(actualIdVgCsvPath) > 0)
        g_fet_smoke_actual_idvg_csv_path = actualIdVgCsvPath;

    vector vx = {
        -3.00, -2.75, -2.50, -2.25, -2.00, -1.75, -1.50, -1.25, -1.00,
        -0.75, -0.50, -0.25, 0.00, 0.25, 0.50, 0.75, 1.00, 1.25,
        1.50, 1.75, 2.00, 2.25, 2.50, 2.75, 3.00
    };
    vector vy = {
        1.00e-12, 1.78e-12, 3.16e-12, 5.62e-12, 1.00e-11,
        1.78e-11, 3.16e-11, 5.62e-11, 1.00e-10, 1.78e-10,
        3.16e-10, 5.62e-10, 1.00e-9, 3.00e-7, 1.10e-6,
        2.00e-6, 3.05e-6, 4.05e-6, 5.10e-6, 6.05e-6,
        7.10e-6, 8.05e-6, 9.10e-6, 1.01e-5, 1.11e-5
    };

    WorksheetPage wp;
    wp.Create("Origin");
    wp.SetName("FETSmokeData", OCD_ENUM_NEXT);
    Worksheet wks;
    wks = wp.Layers(0);
    wks.SetSize(vx.GetSize(), 2);
    wks.Columns(0).SetType(OKDATAOBJ_DESIGNATION_X);
    wks.Columns(1).SetType(OKDATAOBJ_DESIGNATION_Y);
    wks.Columns(0).SetLongName("Vg");
    wks.Columns(1).SetLongName("Id");
    set_active_layer(wks);
    if (fet_analyzer_get_launch_mode() != 1)
        return 401;

    XYRange input, ss, vth;
    input.Add(wks, 0, "X", 0, 0, 24);
    input.Add(wks, 1, "Y", 1, 0, 24);
    input.SetData(&vy, &vx);
    ss.Add(wks, 0, "X", 0, 0, 12);
    ss.Add(wks, 1, "Y", 1, 0, 12);
    vth.Add(wks, 0, "X", 0, 14, 24);
    vth.Add(wks, 1, "Y", 1, 14, 24);

    GraphPage gp;
    gp.Create("Origin");
    gp.SetName("FETSmokeGraph", OCD_ENUM_NEXT);
    GraphLayer gl;
    gl = gp.Layers(0);
    int smokePlotIndex = gl.AddPlot(input, IDM_PLOT_LINE);
    DataPlot smokePlot = gl.DataPlots(smokePlotIndex);
    gp.SetShow(PAGE_ACTIVATE);
    if (fet_analyzer_get_launch_mode() != 2)
        return 402;

    XYRange autoSS, autoVth;
    int autoErr = fet_analyzer_auto_ranges_for_plot(smokePlot, autoSS, autoVth);
    if (autoErr != 0)
        return 430 + autoErr;
    Tree autoResult;
    autoErr = fet_analyze_xf_core(input, autoSS, autoVth, 10.0, 1000.0,
                                  1.15e-8, 0.1, false, false, autoResult);
    if (autoErr != 0)
        return 440 + autoErr;
    autoErr = fet_analyzer_place_free_cursors_for_plot(smokePlot);
    if (autoErr != 0)
        return 450 + autoErr;
    XYRange cursorSS, cursorVth;
    autoErr = fet_analyzer_ranges_from_free_cursors(smokePlot, cursorSS, cursorVth);
    if (autoErr != 0)
        return 460 + autoErr;
    gl.LT_execute("draw -n FET_BWD_SS_START -lm -v -2;draw -n FET_BWD_SS_END -lm -v -1;draw -n FET_BWD_VTH_START -lm -v 1;draw -n FET_BWD_VTH_END -lm -v 2;");
    autoErr = fet_analyzer_refresh_preview();
    if (autoErr != 0)
        return 480 + autoErr;
    if (gl.GraphObjects("FET_BWD_SS_START")
        || gl.GraphObjects("FET_BWD_SS_END")
        || gl.GraphObjects("FET_BWD_VTH_START")
        || gl.GraphObjects("FET_BWD_VTH_END"))
        return 481;
    if (gl.GraphObjects("FET_VTH"))
        return 489;
    if (!gl.GraphObjects("FET_SUMMARY_PLUS")
        || !gl.GraphObjects("FET_ON_PLUS")
        || !gl.GraphObjects("FET_OFF_PLUS")
        || gl.GraphObjects("FET_RATIO_PLUS"))
        return 490;
    gl.LT_execute("__FET_OFF_VMOVE=FET_OFF_PLUS.VMOVE;");
    double offVmove = 0;
    if (!LT_get_var("__FET_OFF_VMOVE", &offVmove) || offVmove < 0.5)
        return 491;
    gl.LT_execute("__FET_OFF_HMOVE=FET_OFF_PLUS.HMOVE;__FET_OFF_ATTACH=FET_OFF_PLUS.ATTACH;");
    double offHmove = 0, offAttach = 0;
    if (!LT_get_var("__FET_OFF_HMOVE", &offHmove)
        || !LT_get_var("__FET_OFF_ATTACH", &offAttach)
        || offHmove > 0.5 || fabs(offAttach - 2) > 0.5)
        return 492;
    Tree cursorResult;
    autoErr = fet_analyze_xf_core(input, cursorSS, cursorVth, 10.0, 1000.0,
                                  1.15e-8, 0.1, false, false, cursorResult);
    if (autoErr != 0)
        return 470 + autoErr;

    Tree result;
    int err = fet_analyze_xf_core(input, ss, vth, 10.0, 1000.0,
                                  1.15e-8, 0.1, true, true, result);
    if (err != 0)
        return 100 + err;
    if (fabs(result.SS.dVal - 1000.045) > 1.0)
        return 201;
    if (fabs(result.Vth.dVal - 0.23937) > 0.01)
        return 202;
    if (fabs(result.Mobility.dVal - 35.6522) > 0.1)
        return 203;
    if (!gl.GraphObjects("FET_SUMMARY"))
    {
        if (!gl.GraphObjects("FET_SUMMARY_PLUS"))
            return 204;
    }

    WorksheetPage results("FETGraphData");
    if (!results || !results.Layers("Curves")
        || !results.Layers("Extracted Parameters"))
        return 205;

    if (!g_fet_smoke_csv_path.IsEmpty())
    {
        FILE* fp = fopen(g_fet_smoke_csv_path, "r");
        if (!fp)
            return 309;
        fclose(fp);

        Tree worksheetOnlyResult;
        int worksheetOnlyErr = fet_import_transfer_csv_files(g_fet_smoke_csv_path,
                                                             worksheetOnlyResult, false);
        if (worksheetOnlyErr != 0)
            return 290 + worksheetOnlyErr;
        if (worksheetOnlyResult.Curves.nVal != 2)
            return 296;
        if (!worksheetOnlyResult.Graph.strVal.IsEmpty())
            return 297;

        Tree importResult;
        int importErr = fet_import_transfer_csv_files(g_fet_smoke_csv_path, importResult, true);
        if (importErr != 0)
            return 300 + importErr;
        if (importResult.Curves.nVal != 2)
            return 306;
        WorksheetPage imported(importResult.Workbook.strVal);
        if (!imported || !imported.Layers("Curves") || !imported.Layers("RawMeta"))
            return 307;
        GraphPage importedGraph(importResult.Graph.strVal);
        if (!importedGraph)
            return 308;
        GraphLayer importedLayer = importedGraph.Layers(0);
        GraphLayer importedRightLayer = importedGraph.Layers(1);
        if (!importedLayer || !importedRightLayer)
            return 318;
        if (importedLayer.DataPlots.Count() != 2
            || importedRightLayer.DataPlots.Count() != 4)
            return 319;
        if (!importedLayer.GraphObjects("FET_CONFIG"))
            return 320;
        if (importedLayer.GraphObjects("Legend")
            || importedRightLayer.GraphObjects("Legend"))
            return 321;
        int axisErr = _fet_smoke_check_double_y(importedLayer, importedRightLayer, 0, 0);
        if (axisErr != 0)
            return 330 + axisErr;
        int logErr = _fet_smoke_check_log_curve(importedLayer, importedRightLayer, 0, 0);
        if (logErr != 0)
            return 340 + logErr;
        importedGraph.SetShow(PAGE_ACTIVATE);
        set_active_layer(importedLayer);
        int styleErr = fet_analyzer_reapply_graph_style_for_test();
        if (styleErr != 0)
            return 350 + styleErr;
    }
    if (!g_fet_smoke_table_csv_path.IsEmpty())
    {
        FILE* fpTable = fopen(g_fet_smoke_table_csv_path, "r");
        if (!fpTable)
            return 409;
        fclose(fpTable);

        Tree tableImportResult;
        int tableImportErr = fet_import_transfer_csv_files(g_fet_smoke_table_csv_path,
                                                           tableImportResult, true);
        if (tableImportErr != 0)
            return 410 + tableImportErr;
        if (tableImportResult.Curves.nVal != 1)
            return 416;
        WorksheetPage tableImported(tableImportResult.Workbook.strVal);
        if (!tableImported || !tableImported.Layers("Curves") || !tableImported.Layers("RawMeta"))
            return 417;
        GraphPage tableImportedGraph(tableImportResult.Graph.strVal);
        if (!tableImportedGraph)
            return 418;
        GraphLayer tableImportedLayer = tableImportedGraph.Layers(0);
        GraphLayer tableImportedRightLayer = tableImportedGraph.Layers(1);
        if (!tableImportedLayer || !tableImportedRightLayer)
            return 418;
        if (tableImportedLayer.DataPlots.Count() != 1
            || tableImportedRightLayer.DataPlots.Count() != 2)
            return 419;
        if (!tableImportedLayer.GraphObjects("FET_CONFIG"))
            return 420;
        int tableAxisErr = _fet_smoke_check_double_y(tableImportedLayer, tableImportedRightLayer, 0, 0);
        if (tableAxisErr != 0)
            return 430 + tableAxisErr;
        int tableLogErr = _fet_smoke_check_log_curve(tableImportedLayer, tableImportedRightLayer, 0, 0);
        if (tableLogErr != 0)
            return 440 + tableLogErr;
    }
    if (!g_fet_smoke_double_csv_path.IsEmpty())
    {
        FILE* fpDouble = fopen(g_fet_smoke_double_csv_path, "r");
        if (!fpDouble)
            return 449;
        fclose(fpDouble);

        Tree doubleImportResult;
        int doubleImportErr = fet_import_transfer_csv_files(g_fet_smoke_double_csv_path,
                                                            doubleImportResult, true);
        if (doubleImportErr != 0)
            return 450 + doubleImportErr;
        if (doubleImportResult.Curves.nVal != 1)
            return 456;
        GraphPage doubleImportedGraph(doubleImportResult.Graph.strVal);
        if (!doubleImportedGraph)
            return 457;
        GraphLayer doubleImportedLayer = doubleImportedGraph.Layers(0);
        GraphLayer doubleImportedRightLayer = doubleImportedGraph.Layers(1);
        if (!doubleImportedLayer || !doubleImportedRightLayer)
            return 458;
        if (doubleImportedLayer.DataPlots.Count() != 2
            || doubleImportedRightLayer.DataPlots.Count() != 3)
            return 459;
        doubleImportedGraph.SetShow(PAGE_ACTIVATE);
        set_active_layer(doubleImportedLayer);
        int doubleImportedRefresh = fet_analyzer_refresh_preview();
        if (doubleImportedRefresh != 0)
            return 460 + doubleImportedRefresh;
        WorksheetPage doubleImportedResults("FETGraphData");
        Worksheet doubleImportedCurves;
        if (doubleImportedResults)
            doubleImportedCurves = doubleImportedResults.Layers("Curves");
        if (!doubleImportedCurves
            || !_fet_smoke_column_has_value(doubleImportedCurves, 4)
            || !_fet_smoke_column_has_value(doubleImportedCurves, 5))
            return 489;
    }
    if (!g_fet_smoke_split_csv_path.IsEmpty())
    {
        FILE* fpSplit = fopen(g_fet_smoke_split_csv_path, "r");
        if (!fpSplit)
            return 649;
        fclose(fpSplit);

        Tree splitImportResult;
        int splitImportErr = fet_import_transfer_csv_files(g_fet_smoke_split_csv_path,
                                                           splitImportResult, true);
        if (splitImportErr != 0)
            return 650 + splitImportErr;
        if (splitImportResult.Curves.nVal != 1)
            return 656;
        GraphPage splitImportedGraph(splitImportResult.Graph.strVal);
        if (!splitImportedGraph)
            return 657;
        GraphLayer splitImportedLayer = splitImportedGraph.Layers(0);
        GraphLayer splitImportedRightLayer = splitImportedGraph.Layers(1);
        if (!splitImportedLayer || !splitImportedRightLayer)
            return 658;
        if (splitImportedLayer.DataPlots.Count() != 2
            || splitImportedRightLayer.DataPlots.Count() != 3)
            return 659;

        DataPlot splitSource;
        for (int pp = 0; pp < splitImportedRightLayer.DataPlots.Count(); pp++)
        {
            DataPlot candidate = splitImportedRightLayer.DataPlots(pp);
            if (candidate && !candidate.IsShow())
            {
                splitSource = candidate;
                break;
            }
        }
        if (!splitSource)
            return 660;

        Tree splitScanInfo;
        int splitScanErr = fet_analyzer_detect_scan_for_plot(splitSource,
                                                             splitScanInfo);
        if (splitScanErr != 0 || splitScanInfo.IsDouble.nVal != 1
            || splitScanInfo.TurnIndex.nVal < 20)
            return 661 + splitScanErr;
    }
    if (!g_fet_smoke_actual_csv_path_1.IsEmpty()
        && !g_fet_smoke_actual_csv_path_2.IsEmpty()
        && !g_fet_smoke_actual_csv_path_3.IsEmpty())
    {
        string actualPaths = g_fet_smoke_actual_csv_path_1 + "|"
                           + g_fet_smoke_actual_csv_path_2 + "|"
                           + g_fet_smoke_actual_csv_path_3;
        Tree actualImportResult;
        int actualImportErr = fet_import_transfer_csv_files(actualPaths,
                                                            actualImportResult, true);
        if (actualImportErr != 0)
            return 500 + actualImportErr;
        if (actualImportResult.Curves.nVal != 10)
            return 506;
        if (actualImportResult.FailedFiles.nVal != 0)
            return 507;
        WorksheetPage actualImported(actualImportResult.Workbook.strVal);
        if (!actualImported || !actualImported.Layers("Curves") || !actualImported.Layers("RawMeta"))
            return 508;
        GraphPage actualImportedGraph(actualImportResult.Graph.strVal);
        if (!actualImportedGraph)
            return 509;
        GraphLayer actualImportedLayer = actualImportedGraph.Layers(0);
        GraphLayer actualImportedRightLayer = actualImportedGraph.Layers(1);
        if (!actualImportedLayer || !actualImportedRightLayer)
            return 509;
        if (actualImportedLayer.DataPlots.Count() < 10
            || actualImportedLayer.DataPlots.Count() > 20
            || actualImportedRightLayer.DataPlots.Count()
               != actualImportedLayer.DataPlots.Count() + 10)
            return 510;
        if (!actualImportedLayer.GraphObjects("FET_CONFIG"))
            return 511;
        int actualAxisErr = _fet_smoke_check_double_y(actualImportedLayer, actualImportedRightLayer, 0, 0);
        if (actualAxisErr != 0)
            return 520 + actualAxisErr;
        int actualLogErr = _fet_smoke_check_log_curve(actualImportedLayer, actualImportedRightLayer, 0, 0);
        if (actualLogErr != 0)
            return 530 + actualLogErr;

        // Bug-2 regression guard: in this 10-curve graph, whichever plot is
        // active must be the curve that actually gets analyzed -- not just
        // whichever curve elsewhere in the layer happens to have the most
        // backward-scan points.
        DataPlot activeCandidate = actualImportedRightLayer.DataPlots();
        if (activeCandidate)
        {
            actualImportedGraph.SetShow(PAGE_ACTIVATE);
            set_active_layer(actualImportedRightLayer);
            string expectedDataset = activeCandidate.GetDatasetName();
            int actualRefreshErr = fet_analyzer_refresh_preview();
            if (actualRefreshErr != 0)
                return 540 + actualRefreshErr;
            WorksheetPage extractedPage("FETGraphData");
            Worksheet extractedWks;
            if (extractedPage)
                extractedWks = extractedPage.Layers("Extracted Parameters");
            if (!extractedWks)
                return 545;
            string expectedForwardKey = expectedDataset + " [+]";
            if (_fet_smoke_find_summary_row(extractedWks, expectedForwardKey) < 0)
                return 546;
        }
    }
    if (!g_fet_smoke_actual_idvg_csv_path.IsEmpty())
    {
        FILE* fpActualIdVg = fopen(g_fet_smoke_actual_idvg_csv_path, "r");
        if (!fpActualIdVg)
            return 709;
        fclose(fpActualIdVg);

        Tree actualIdVgImportResult;
        int actualIdVgImportErr = fet_import_transfer_csv_files(
            g_fet_smoke_actual_idvg_csv_path, actualIdVgImportResult, true);
        if (actualIdVgImportErr != 0)
            return 710 + actualIdVgImportErr;
        if (actualIdVgImportResult.Curves.nVal != 1)
            return 716;

        GraphPage actualIdVgImportedGraph(actualIdVgImportResult.Graph.strVal);
        if (!actualIdVgImportedGraph)
            return 717;
        GraphLayer actualIdVgImportedLayer = actualIdVgImportedGraph.Layers(0);
        GraphLayer actualIdVgImportedRightLayer = actualIdVgImportedGraph.Layers(1);
        if (!actualIdVgImportedLayer || !actualIdVgImportedRightLayer)
            return 718;
        if (actualIdVgImportedLayer.DataPlots.Count() != 2
            || actualIdVgImportedRightLayer.DataPlots.Count() != 3)
            return 719;

        WorksheetPage actualIdVgWorkbook(actualIdVgImportResult.Workbook.strVal);
        Worksheet actualIdVgCurves;
        if (actualIdVgWorkbook)
            actualIdVgCurves = actualIdVgWorkbook.Layers("Curves");
        if (!actualIdVgCurves || actualIdVgCurves.GetNumRows() < 100)
            return 720;

        XYRange actualIdVgFirstTwoCols;
        actualIdVgFirstTwoCols.Add(actualIdVgCurves, 0, "X", 0, 0,
                                   actualIdVgCurves.GetNumRows() - 1);
        actualIdVgFirstTwoCols.Add(actualIdVgCurves, 1, "Y", 1, 0,
                                   actualIdVgCurves.GetNumRows() - 1);
        GraphPage actualIdVgTwoColGraph;
        actualIdVgTwoColGraph.Create("Origin");
        actualIdVgTwoColGraph.SetName("FETActualIdVgTwoColGraph", OCD_ENUM_NEXT);
        GraphLayer actualIdVgTwoColLayer = actualIdVgTwoColGraph.Layers(0);
        int actualIdVgPlotIndex = actualIdVgTwoColLayer.AddPlot(
            actualIdVgFirstTwoCols, IDM_PLOT_LINE);
        DataPlot actualIdVgPlot = actualIdVgTwoColLayer.DataPlots(actualIdVgPlotIndex);
        actualIdVgTwoColGraph.SetShow(PAGE_ACTIVATE);
        set_active_layer(actualIdVgTwoColLayer);

        Tree actualIdVgScanInfo;
        int actualIdVgScanErr = fet_analyzer_detect_scan_for_plot(
            actualIdVgPlot, actualIdVgScanInfo);
        if (actualIdVgScanErr != 0 || actualIdVgScanInfo.IsDouble.nVal != 1
            || actualIdVgScanInfo.TurnIndex.nVal < 90)
            return 721 + actualIdVgScanErr;
        int actualIdVgCursorErr = fet_analyzer_place_free_cursors_for_plot(
            actualIdVgPlot);
        if (actualIdVgCursorErr != 0)
            return 730 + actualIdVgCursorErr;
        if (!actualIdVgTwoColLayer.GraphObjects("FET_BWD_SS_START")
            || !actualIdVgTwoColLayer.GraphObjects("FET_BWD_SS_END")
            || !actualIdVgTwoColLayer.GraphObjects("FET_BWD_VTH_START")
            || !actualIdVgTwoColLayer.GraphObjects("FET_BWD_VTH_END"))
            return 740;

        set_active_layer(actualIdVgTwoColLayer);
        int actualIdVgPrepareErr = fet_analyzer_prepare_active_graph_for_test();
        if (actualIdVgPrepareErr != 0)
            return 750 + actualIdVgPrepareErr;
    }

    vector doubleX, doubleY;
    for (int ff = 0; ff < vx.GetSize(); ff++)
    {
        doubleX.Add(vx[ff]);
        doubleY.Add(vy[ff]);
    }
    for (int bb = vx.GetSize() - 2; bb >= 0; bb--)
    {
        doubleX.Add(vx[bb]);
        doubleY.Add(vy[bb] * 0.85);
    }
    WorksheetPage doublePage;
    doublePage.Create("Origin");
    doublePage.SetName("FETDoubleSmokeData", OCD_ENUM_NEXT);
    Worksheet doubleWks = doublePage.Layers(0);
    doubleWks.SetSize(doubleX.GetSize(), 2);
    doubleWks.Columns(0).SetType(OKDATAOBJ_DESIGNATION_X);
    doubleWks.Columns(1).SetType(OKDATAOBJ_DESIGNATION_Y);
    XYRange doubleRange;
    doubleRange.Add(doubleWks, 0, "X", 0, 0, doubleX.GetSize() - 1);
    doubleRange.Add(doubleWks, 1, "Y", 1, 0, doubleY.GetSize() - 1);
    doubleRange.SetData(&doubleY, &doubleX);

    GraphPage doubleGraph;
    doubleGraph.Create("Origin");
    doubleGraph.SetName("FETDoubleSmokeGraph", OCD_ENUM_NEXT);
    GraphLayer doubleLayer = doubleGraph.Layers(0);
    int doublePlotIndex = doubleLayer.AddPlot(doubleRange, IDM_PLOT_LINE);
    DataPlot doublePlot = doubleLayer.DataPlots(doublePlotIndex);
    doubleGraph.SetShow(PAGE_ACTIVATE);
    set_active_layer(doubleLayer);
    Tree scanInfo;
    int scanErr = fet_analyzer_detect_scan_for_plot(doublePlot, scanInfo);
    if (scanErr != 0 || scanInfo.IsDouble.nVal != 1
        || scanInfo.TurnIndex.nVal != vx.GetSize() - 1)
        return 600 + scanErr;
    int cursorErr = fet_analyzer_place_free_cursors_for_plot(doublePlot);
    if (cursorErr != 0)
        return 610 + cursorErr;
    if (!doubleLayer.GraphObjects("FET_BWD_SS_START")
        || !doubleLayer.GraphObjects("FET_BWD_SS_END")
        || !doubleLayer.GraphObjects("FET_BWD_VTH_START")
        || !doubleLayer.GraphObjects("FET_BWD_VTH_END"))
        return 620;
    if (!doubleLayer.GraphObjects("FET_SUMMARY_PLUS")
        || !doubleLayer.GraphObjects("FET_SUMMARY_MINUS")
        || !doubleLayer.GraphObjects("FET_ON_PLUS")
        || !doubleLayer.GraphObjects("FET_OFF_PLUS")
        || !doubleLayer.GraphObjects("FET_ON_MINUS")
        || !doubleLayer.GraphObjects("FET_OFF_MINUS")
        || doubleLayer.GraphObjects("FET_RATIO_PLUS")
        || doubleLayer.GraphObjects("FET_RATIO_MINUS"))
        return 621;
    doubleLayer.LT_execute("__FET_OFF_MINUS_VMOVE=FET_OFF_MINUS.VMOVE;");
    double offMinusVmove = 0;
    if (!LT_get_var("__FET_OFF_MINUS_VMOVE", &offMinusVmove)
        || offMinusVmove < 0.5)
        return 622;
    doubleLayer.LT_execute("__FET_OFF_MINUS_HMOVE=FET_OFF_MINUS.HMOVE;__FET_OFF_MINUS_ATTACH=FET_OFF_MINUS.ATTACH;");
    double offMinusHmove = 0, offMinusAttach = 0;
    if (!LT_get_var("__FET_OFF_MINUS_HMOVE", &offMinusHmove)
        || !LT_get_var("__FET_OFF_MINUS_ATTACH", &offMinusAttach)
        || offMinusHmove > 0.5 || fabs(offMinusAttach - 2) > 0.5)
        return 623;
    int secondDoubleRefresh = fet_analyzer_refresh_preview();
    if (secondDoubleRefresh != 0)
        return 630 + secondDoubleRefresh;
    int leftPlotsAfterRefresh = doubleLayer.DataPlots.Count();
    GraphLayer doubleRightLayer = doubleGraph.Layers.Count() > 1
                                ? doubleGraph.Layers(1) : doubleLayer;
    int rightPlotsAfterRefresh = doubleRightLayer.DataPlots.Count();

    doubleLayer.LT_execute("FET_OFF_PLUS.y1=-10;FET_OFF_PLUS.y2=-10;");
    set_active_layer(doubleLayer);
    int ioffRefresh = fet_analyzer_refresh_preview();
    if (ioffRefresh != 0)
        return 633 + ioffRefresh;
    if (doubleLayer.DataPlots.Count() != leftPlotsAfterRefresh
        || doubleRightLayer.DataPlots.Count() != rightPlotsAfterRefresh)
        return 634;

    WorksheetPage doubleResults("FETGraphData");
    Worksheet doubleCurves;
    Worksheet doubleExtracted;
    if (doubleResults)
    {
        doubleCurves = doubleResults.Layers("Curves");
        doubleExtracted = doubleResults.Layers("Extracted Parameters");
    }
    if (!doubleCurves
        || !_fet_smoke_column_has_value(doubleCurves, 4)
        || !_fet_smoke_column_has_value(doubleCurves, 5))
        return 632;
    if (!doubleExtracted)
        return 635;

    // Summary rows are now keyed by "<curve dataset> [+]"/" [-]" instead of
    // a hardcoded row 0/1, so repeated refreshes of the same curve update in
    // place instead of overwriting whatever else was last analyzed.
    string doubleCurveLabel = doublePlot.GetDatasetName();
    string doubleForwardKey = doubleCurveLabel + " [+]";
    string doubleBackwardKey = doubleCurveLabel + " [-]";
    int doubleForwardRow = _fet_smoke_find_summary_row(doubleExtracted, doubleForwardKey);
    int doubleBackwardRow = _fet_smoke_find_summary_row(doubleExtracted, doubleBackwardKey);
    if (doubleForwardRow < 0 || doubleBackwardRow < 0)
        return 637;

    double currentIoff = doubleExtracted.Cell(doubleForwardRow, 12);
    double currentRatio = doubleExtracted.Cell(doubleForwardRow, 13);
    // The Ioff cursor's .script$ now re-triggers analysis on every property
    // write (needed so dragging it live-updates Ioff/Ratio -- see
    // _fet_add_horizontal_cursor_line). Setting .y1 then .y2 in one LT_execute
    // call therefore re-enters refresh twice, and each cursor
    // delete+recreate cycle re-reads its position through axis-scale
    // attachment, which can drift by a fraction of a percent. A tight
    // absolute tolerance chases that noise, so check relative closeness
    // instead of exact equality.
    if (is_missing_value(currentIoff) || fabs(currentIoff - 1.0e-10) > 0.01e-10
        || is_missing_value(currentRatio) || currentRatio <= 0)
        return 636;
    if (!doubleLayer.GraphObjects("FET_SUMMARY_PLUS")
        || !doubleLayer.GraphObjects("FET_SUMMARY_MINUS")
        || !doubleLayer.GraphObjects("FET_OFF_PLUS")
        || !doubleLayer.GraphObjects("FET_OFF_MINUS"))
        return 631;

    // Bug-1 regression guard: analyzing a completely different curve must
    // get its own row rather than overwriting the double-scan curve's
    // already-written row.
    WorksheetPage secondPage;
    secondPage.Create("Origin");
    secondPage.SetName("FETSecondSmokeData", OCD_ENUM_NEXT);
    Worksheet secondWks = secondPage.Layers(0);
    secondWks.SetSize(vx.GetSize(), 2);
    secondWks.Columns(0).SetType(OKDATAOBJ_DESIGNATION_X);
    secondWks.Columns(1).SetType(OKDATAOBJ_DESIGNATION_Y);
    XYRange secondRange;
    secondRange.Add(secondWks, 0, "X", 0, 0, vx.GetSize() - 1);
    secondRange.Add(secondWks, 1, "Y", 1, 0, vx.GetSize() - 1);
    secondRange.SetData(&vy, &vx);
    GraphPage secondGraph;
    secondGraph.Create("Origin");
    secondGraph.SetName("FETSecondSmokeGraph", OCD_ENUM_NEXT);
    GraphLayer secondLayer = secondGraph.Layers(0);
    int secondPlotIndex = secondLayer.AddPlot(secondRange, IDM_PLOT_LINE);
    DataPlot secondPlot = secondLayer.DataPlots(secondPlotIndex);
    secondGraph.SetShow(PAGE_ACTIVATE);
    set_active_layer(secondLayer);
    int secondCursorErr = fet_analyzer_place_free_cursors_for_plot(secondPlot);
    if (secondCursorErr != 0)
        return 640 + secondCursorErr;

    double doubleIoffAfterSecond = doubleExtracted.Cell(doubleForwardRow, 12);
    if (is_missing_value(doubleIoffAfterSecond)
        || fabs(doubleIoffAfterSecond - currentIoff) > 1.0e-13)
        return 646;
    // Analyzing a distinct curve reuses a leftover blank row (Origin
    // worksheets have a nonzero minimum row count) rather than always
    // growing the sheet, so what matters is that it landed somewhere of its
    // own -- not that the row count necessarily increased.
    string secondCurveLabel = secondPlot.GetDatasetName();
    string secondForwardKey = secondCurveLabel + " [+]";
    if (_fet_smoke_find_summary_row(doubleExtracted, secondForwardKey) < 0)
        return 648;

    // Bug-2 regression guard: switching scanMode to Forward-only must not
    // delete the backward range cursors (only what gets analyzed/plotted),
    // and switching back to Both must immediately reuse them.
    set_active_layer(doubleLayer);
    fet_analyzer_set_scan_mode_for_test(1); // FET_SCAN_FORWARD
    int fwdOnlyErr = fet_analyzer_refresh_preview();
    if (fwdOnlyErr != 0)
        return 660 + fwdOnlyErr;
    if (!doubleLayer.GraphObjects("FET_BWD_SS_START")
        || !doubleLayer.GraphObjects("FET_BWD_SS_END")
        || !doubleLayer.GraphObjects("FET_BWD_VTH_START")
        || !doubleLayer.GraphObjects("FET_BWD_VTH_END"))
        return 670;
    if (doubleLayer.GraphObjects("FET_SUMMARY_MINUS"))
        return 671;

    fet_analyzer_set_scan_mode_for_test(3); // FET_SCAN_BOTH
    int bothAgainErr = fet_analyzer_refresh_preview();
    if (bothAgainErr != 0)
        return 680 + bothAgainErr;
    if (!doubleLayer.GraphObjects("FET_BWD_SS_START")
        || !doubleLayer.GraphObjects("FET_BWD_SS_END")
        || !doubleLayer.GraphObjects("FET_BWD_VTH_START")
        || !doubleLayer.GraphObjects("FET_BWD_VTH_END"))
        return 690;
    if (!doubleLayer.GraphObjects("FET_SUMMARY_MINUS"))
        return 691;

    fet_analyzer_reset_options_for_test();

    // scanMode must hide (not delete) the non-selected direction's visible
    // curve on the "analyze a plain plot" path (_fet_create_doubley_graph_
    // from_plot / FETAnalysisGraph), and show it again on Both -- exercised
    // via the same double-scan data used above but as a fresh plain plot.
    GraphPage plainGraph;
    plainGraph.Create("Origin");
    plainGraph.SetName("FETVisibilitySmokeGraph", OCD_ENUM_NEXT);
    GraphLayer plainLayer = plainGraph.Layers(0);
    XYRange plainRange;
    plainRange.Add(doubleWks, 0, "X", 0, 0, doubleX.GetSize() - 1);
    plainRange.Add(doubleWks, 1, "Y", 1, 0, doubleY.GetSize() - 1);
    int plainPlotIndex = plainLayer.AddPlot(plainRange, IDM_PLOT_LINE);
    if (plainPlotIndex < 0)
        return 800;
    plainGraph.SetShow(PAGE_ACTIVATE);
    set_active_layer(plainLayer);
    int visErr1 = fet_analyzer_analyze_active_for_test();
    if (visErr1 != 0)
        return 801;

    GraphPage visGraph("FETAnalysisGraph");
    if (!visGraph)
        return 802;
    GraphLayer visLeft = visGraph.Layers(0);
    GraphLayer visRight = visGraph.Layers.Count() > 1 ? visGraph.Layers(1) : visLeft;
    if (!visLeft || !visRight)
        return 803;
    visGraph.SetShow(PAGE_ACTIVATE);
    set_active_layer(visRight);
    int visErr2 = fet_analyzer_analyze_active_for_test();
    if (visErr2 != 0)
        return 804;

    fet_analyzer_set_scan_mode_for_test(1); // FET_SCAN_FORWARD
    set_active_layer(visRight);
    int visErrFwd = fet_analyzer_analyze_active_for_test();
    if (visErrFwd != 0)
        return 810;
    // Fit lines/markers for the unused direction are still expected to be
    // removed (not just hidden) here -- only the raw forward/backward data
    // segments must survive as hidden rather than deleted.
    int hiddenVisible = 0, hiddenHidden = 0;
    for (int vv = 0; vv < visRight.DataPlots.Count(); vv++)
    {
        DataPlot p = visRight.DataPlots(vv);
        if (!p)
            continue;
        XYRange pr;
        p.GetDataRange(pr, 0, -1);
        vector px, py;
        if (!pr.GetData(py, px) || px.GetSize() == 0 || px.GetSize() >= doubleX.GetSize()
            || p.GetDatasetName().Find("FETFitData") >= 0)
            continue; // skip the hidden full-range source and fit/marker plots
        if (p.IsShow())
            hiddenVisible++;
        else
            hiddenHidden++;
    }
    if (hiddenVisible != 1 || hiddenHidden != 1)
        return 812;

    fet_analyzer_set_scan_mode_for_test(3); // FET_SCAN_BOTH
    set_active_layer(visRight);
    int visErrBoth = fet_analyzer_analyze_active_for_test();
    if (visErrBoth != 0)
        return 820;
    int visibleSegments = 0;
    for (int ww = 0; ww < visRight.DataPlots.Count(); ww++)
    {
        DataPlot p = visRight.DataPlots(ww);
        if (!p)
            continue;
        XYRange pr;
        p.GetDataRange(pr, 0, -1);
        vector px, py;
        if (!pr.GetData(py, px) || px.GetSize() == 0 || px.GetSize() >= doubleX.GetSize()
            || p.GetDatasetName().Find("FETFitData") >= 0)
            continue;
        if (p.IsShow())
            visibleSegments++;
    }
    if (visibleSegments != 2)
        return 821;

    fet_analyzer_reset_options_for_test();
    return 0;
}
