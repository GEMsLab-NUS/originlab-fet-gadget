/*
 * X-Function adapter source fragment (.inc prevents accidental standalone build).
 *
 * Create fet_analyze.oxf with X-Function Builder using the adjacent variable
 * specification. Keep Builder-generated framework/event code, and use the
 * body below as the generated main function body.
 */

void fet_analyze(const XYRange& input, const XYRange& ss,
    const XYRange& vth, const double& L_um, const double& W_um,
    const double& Cox, const double& Vd, const int& annotate,
    const int& output, TreeNode& result)
{
    int nErr = fet_analyze_xf_core(input, ss, vth, L_um, W_um, Cox, Vd,
                                   annotate != 0, output != 0, result);
    if (nErr != 0)
        XF_THROW(nErr);
}
