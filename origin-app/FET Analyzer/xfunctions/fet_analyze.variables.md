# `fet_analyze` X-Function Builder 定义

在 Origin 的 X-Function Builder（F10）中创建 `fet_analyze`，类型使用普通 X-Function（非 Gadget）；启用 Auto GetN Dialog 和 LabTalk access。

按下列顺序定义变量：

| Name | I/O | Type | Default | Label |
|---|---|---|---|---|
| input | Input | XYRange | `<active>` | Input Id-Vg Curve |
| ss | Input | XYRange |  | SS Fit Range |
| vth | Input | XYRange |  | Vth Fit Range |
| L_um | Input | double | 10 | Channel Length (um) |
| W_um | Input | double | 1000 | Channel Width (um) |
| Cox | Input | double | 1.15e-8 | Cox (F/cm^2) |
| Vd | Input | double | 0.1 | Drain Voltage (V) |
| annotate | Input | int | 1 | Annotate Active Graph |
| output | Input | int | 1 | Append Summary Worksheet |
| result | Output | TreeNode |  | Results |

生成的 main prototype 应等价于：

```cpp
void fet_analyze(const XYRange& input, const XYRange& ss,
    const XYRange& vth, const double& L_um, const double& W_um,
    const double& Cox, const double& Vd, const int& annotate,
    const int& output, TreeNode& result)
```

将 `fet_analyze_xf_main.inc` 的 main body 放入 Builder 生成的 main function。`src/FETAnalyzer.c` 必须在同一 Code Builder workspace 中先编译，或由 `launch.ogs` 加载。
