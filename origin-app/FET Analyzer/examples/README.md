# Example Data

`FET_transfer_sample.csv` is a simple table-style synthetic transfer curve.

`FET_transfer_instrument_multicurve.csv` is an instrument-style CSV with metadata and two `DataName/DataValue` data blocks.

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
