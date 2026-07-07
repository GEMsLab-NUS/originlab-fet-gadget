# FET Analyzer for Origin

面向 Origin/OriginPro 的交互式 FET transfer curve 分析 App。

## 当前能力

- 导入仪器 CSV，支持单文件多段 `DataName/DataValue` 数据块，也支持普通 `Vg/Id` 表格。
- 自动生成 `Curves`/`RawMeta` workbook。
- 自动生成 Id-Vg graph：左 Y 轴为 `abs(Id)` 对数坐标，右 Y 轴为 `Id` 线性坐标。
- 在图上生成可水平拖动的自由 range cursors：
  - 红色一对：SS 亚阈值拟合范围。
  - 蓝色一对：Vth 线性外推范围。
- 计算 SS、Vth、最大 gm、场效应迁移率、Ion/Ioff。
- 在图中叠加 SS 拟合线、Vth 拟合/截线、gm/Vth 参考线、summary annotation。
- 图上提供 `FET Analyzer v` 配置按钮，可打开 Device、Extraction、Cursors、Graph、Output 分组配置界面。
- 结果追加到 `[FETResults]Summary`。

## 使用流程

1. 从 App Gallery 启动 **FET Analyzer**，选择一个或多个 CSV 文件。
2. App 会创建 workbook 和双 Y 轴 graph。
3. 在 graph 中点击要分析的右轴 `Id` 曲线，使其成为 active plot。
4. 再次启动 **FET Analyzer**。如果图上还没有 range cursors，App 会自动放置红/蓝两组竖线。
5. 拖动竖线调整范围：
   - 红色范围覆盖 SS 亚阈值区域。
   - 蓝色范围覆盖 Vth 使用的线性导通段。
6. 再次启动 **FET Analyzer**，确认参数配置后计算。

## 构建

```powershell
powershell.exe -NoProfile -ExecutionPolicy Bypass -File .\tools\build-opx.ps1
```

构建脚本会执行 Origin C 编译、运行时 smoke test，并生成：

```text
build/FET Analyzer.opx
```

当前 smoke test 会真实导入桌面上的三份实际 CSV：

- `Transfer curve 1V.csv`
- `Transfer curve 1V-2.csv`
- `Transfer curve 1V-3.csv`

并验证总计 10 条曲线会生成 20 个 graph plots、配置按钮存在、自由 cursor 范围可回读并可完成计算。
