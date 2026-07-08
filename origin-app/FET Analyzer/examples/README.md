# Example Data

`FET_transfer_sample.csv` is a simple table-style synthetic transfer curve.

`FET_transfer_instrument_multicurve.csv` is an instrument-style CSV with metadata and two `DataName/DataValue` data blocks.

`FET_output_curve_rejected.csv` is an Output/Id-Vd curve (Vg held CONST/VAR2 while Vd is the swept VAR1 channel) that also happens to expose `Vg`/`Id`-named columns. It must be rejected by the transfer-curve importer rather than misread as a transfer curve.

`FET_transfer_generic_current.csv` is a transfer curve (Vg confirmed VAR1) whose current column keeps an instrument-generic name (`I1`) instead of `Id`/`Ids`. It exercises the importer's fallback current-column match.

`FET_transfer_metadata_prefix_guard.csv` reproduces a real-world bug: a `Output.List.Data,Vg,Id,Ig,Is,Vg`-style metadata line (present in most real B1500 exports) names the same Vg/Id columns as the real data header, just at different positions. If accepted as the header, every column downstream is misread. The importer must skip that decoy line and use the real header instead.

`FET_output_curve_xaxisname_rejected.csv` is an Output curve from a built-in "ApplicationTest" export (no `Channel.VName`/`Channel.Func` at all) that records the swept axis as `AnalysisSetup, ...XAxis.Name, Vd` instead. It must still be rejected as non-transfer.

Recommended initial ranges for the synthetic sample:

- SS range: `Vg = -3.0 ... 0.0 V`
- Vth range: `Vg = 0.5 ... 3.0 V`
- Device parameters: `L = 10 um`, `W = 1000 um`, `Cox = 1.15e-8 F/cm^2`, `Vd = 0.1 V`

Expected values with the current algorithm:

| Parameter | Expected |
|---|---:|
| SS | 1000.05 mV/dec |
| Vth | 0.2394 V |
| max abs(gm) | 4.1e-6 S |
| mobility | 35.65 cm^2/(V s) |
| Ion/Ioff | 1.11e7 |

Small differences are acceptable when cursor X bounds include slightly different endpoint data points.
