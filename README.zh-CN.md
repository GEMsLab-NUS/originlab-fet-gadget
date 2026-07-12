# FET Gadget for Origin

<p align="center">
  <a href="LICENSE"><img alt="License: MIT" src="https://img.shields.io/badge/License-MIT-yellow.svg"></a>
  <a href="#环境要求"><img alt="Origin" src="https://img.shields.io/badge/Origin-2018%2B-blue.svg"></a>
  <a href="../../releases"><img alt="Release" src="https://img.shields.io/badge/release-latest-brightgreen.svg"></a>
  <a href="docs/architecture.md"><img alt="Origin C" src="https://img.shields.io/badge/built%20with-Origin%20C-informational.svg"></a>
</p>

<p align="center"><a href="README.md">English</a> | <b>简体中文</b></p>

面向 Origin/OriginPro 的交互式 FET（场效应晶体管）transfer curve 分析 App。导入 CSV 原始数据，
在图上用可拖动的 range cursor 圈定拟合区间，一键提取 SS、Vth、gm、迁移率、Ion/Ioff 等参数，并把
结果整理进结果表，支持正向/反向双扫描（forward/backward）分别提取和累计多条曲线的结果。

App 内部名称为 **FET Gadget**（`package.ini` / OPX 文件名 / Origin Apps 目录下的文件夹名都是
`FET Gadget`），仓库目录沿用了早期的 `FET Analyzer` 命名，两者指同一个 App。

<p align="center">
  <img src="docs/demo.png" alt="FET Gadget 在 Origin 中分析正反向 Id-Vg 扫描" width="720">
  <br>
  <sub>双轴 Id-Vg 图，带可拖动的拟合区间 cursor，正向（<code>[+]</code>）/ 反向（<code>[-]</code>）
  参数摘要直接叠加在图上。</sub>
</p>

## 目录

- [功能](#功能)
- [环境要求](#环境要求)
- [安装](#安装)
  - [方式一：安装已构建的 OPX（推荐给使用者）](#方式一安装已构建的-opx推荐给使用者)
  - [方式二：从源码构建（推荐给开发者）](#方式二从源码构建推荐给开发者)
- [使用方法](#使用方法)
- [支持的 CSV 格式](#支持的-csv-格式)
- [输出说明](#输出说明)
- [项目结构](#项目结构)
- [开发与测试](#开发与测试)
- [已知限制 / 后续计划](#已知限制--后续计划)
- [License](#license)

## 功能

| | |
|---|---|
| **灵活导入** | 支持仪器多段 `DataName/DataValue` CSV、普通 `Vg/Id` 表格，或显式拆分的 `Forward Vg/Forward Id/Backward Vg/Backward Id` 列。 |
| **自动识别双向扫描** | 正向（上扫）和反向（下扫）自动识别并配对为同一条曲线的两个方向。 |
| **双轴图** | 自动生成 Id-Vg 图：左轴 `\|Id\|` 对数坐标，右轴 `Id` 线性坐标，均按沟道宽度归一化为 `uA/um`。 |
| **可拖动拟合区间** | 每个方向两对自由 cursor——正向实线、反向点划线——标记 SS 和 Vth 拟合窗口，不吸附数据点。 |
| **一键提取** | SS、Vth、最大 gm、场效应迁移率、Ion/Ioff、滞回，拟合线/参考线/参数摘要直接叠加在图上。 |
| **Ioff 实时更新** | 拖动 Ioff 参考线，Ioff 和 Ion/Ioff 立即在图上和结果表中重新计算。 |
| **切换扫描模式不破坏数据** | 在设置里切换 Auto/Forward/Backward/Both 只是隐藏（不删除）未选中方向的曲线，也不会丢失已经调好的 cursor 位置。 |
| **结果持续累计** | 隐藏的 `Extracted Parameters` 表里每条曲线每个方向一行——重复分析同一曲线更新原行，分析不同曲线追加新行。 |
| **精简多曲线导入** | 一次选择多个 CSV 文件时，只生成一个多列 `Vg/Id`（XYXY…）workbook，不建图——一张轻量数据表，供下一步多曲线分析使用。单文件导入仍是完整 6 列并自带图形，行为不变。 |
| **一体化多曲线分析** | 一条命令直接读取当前激活的 worksheet 或 graph（不再弹文件选择框），对每条曲线提取 SS/Vth/gm/迁移率/Ion/Ioff，并把叠加图和参数分布图一起生成。叠加图沿用经典的单曲线配色（对数轴靛蓝、线性轴琥珀，坐标轴也保持同色），除 SS 最优的一条曲线保持实线不透明高亮外，其余曲线均为真正的半透明——不需要图例。拟合参数可通过任一张图上的 `[FET Multi]` 按钮设置，并配有 `[Prev]`/`[Next]` 按钮切换六个参数。 |
| **散点图 + 边缘直方图** | 任选两个批量结果参数（SS、Vth、gm、迁移率、Ion、Ioff、Ion/Ioff、log₁₀ 比值）互相作图，两条轴各带一个与散点范围对齐的直方图，用两个下拉菜单选择 X/Y。优先尝试 Origin 自带的「Marginal Histograms」图库模板，失败时自动回退到手工搭建的等效图。 |
| **相关性矩阵** | 用复选框勾选要纳入的参数，生成一张相关性**图**——Origin 自带的「Scatter Matrix」图库模板；若无法确认生成成功，自动回退为 Pearson 相关系数表。 |
| **进度展示** | 多文件导入和多曲线批量拟合都会显示原生进度条（支持取消）以及状态栏文字读数，不会让你干等。 |

## 环境要求

- **Origin 或 OriginPro 2018 及以上**（`package.ini` 中声明的最低版本为 **9.5**，对应 Origin
  2018——App Gallery/OPX 打包机制大致是从这个年代开始成熟的；比这更早、没有 App Gallery 的版本，
  不管这个数字写多少都装不上）。代码本身只用到了长期存在的基础 Origin C / Apps API，但这个下限
  没有在真实的 Origin 2018 上跑过验证，遇到版本相关的问题欢迎[提 issue](../../issues)。
- Windows（Origin C / OPX 打包工具链本身是 Windows-only）。
- 从源码构建（`tools/build-opx.ps1`）额外需要：
  - Origin 已在本机注册为 COM Server（正常安装即会注册）。
  - PowerShell（Windows 自带的 5.1 版本即可）。

## 安装

### 方式一：安装已构建的 OPX（推荐给使用者）

1. 从 [Releases](../../releases) 页面下载最新的 `FET Gadget.opx`。
2. 直接把 `.opx` 文件拖入正在运行的 Origin 窗口，按提示完成安装。
3. 安装完成后，在 App Gallery（或 **Apps** 面板）里能看到 **FET Gadget** 图标。

### 方式二：从源码构建（推荐给开发者）

```powershell
git clone https://github.com/GEMsLab-NUS/originlab-fet-gadget.git
cd originlab-fet-gadget
powershell.exe -NoProfile -ExecutionPolicy Bypass -File .\tools\build-opx.ps1
```

构建脚本会依次：

1. 启动 Origin（通过 COM），编译 `origin-app/FET Analyzer/src/FETAnalyzer.c`。
2. 编译并运行 `tests/FETAnalyzerSmoke.c` 的运行时 smoke test（可用 `-SkipSmoke` 跳过）。
3. 把 `origin-app/FET Analyzer` 同步到 `%LocalAppData%\OriginLab\Apps\FET Gadget`，用 `mkOPX`
   打包为 `build/FET Gadget.opx`。

由于构建过程本身也会把 App 安装/更新到本机的 Origin Apps 目录，本机可以直接在 Origin 里试用，不
需要再手动拖拽安装。生成的 `build/FET Gadget.opx` 仍然可以拷贝到任意一台安装了 Origin 的机器，
按方式一安装。

<details>
<summary>手动开发安装（直接在 Code Builder 里调试，不打 OPX）</summary>

把 `origin-app/FET Analyzer` 整个复制到 `%LocalAppData%\OriginLab\Apps\FET Gadget`，在 Origin 的
Code Builder 中把该文件夹加入 workspace 即可直接调试。完整的打包/发布流程见
[docs/opx-packaging.md](docs/opx-packaging.md)。

</details>

## 使用方法

启动 **FET Gadget** 会弹出一个带按钮的对话框——**Import**（导入）、**Single-Curve Analysis**
（单曲线分析）、**Multi-Curve Analysis**（多曲线分析），以及一个 **More...** 按钮，里面是
**Scatter + Histograms**（散点+直方图）和 **Correlation Matrix**（相关性矩阵）。

### 演示视频

**导入 + 单曲线分析**

https://github.com/user-attachments/assets/25d83a72-da7c-4a33-8b2c-386c827eeda9

**多曲线分析**

https://github.com/user-attachments/assets/73d247df-0e6e-4a9b-8612-da2a005568a7

### 导入

选择一个或多个 CSV 文件。

- **单文件** — 完整导入（`Vg/Id/Ig/absId/Vd/logAbsId`）并自动生成双轴 Id-Vg 图，可直接进入
  单曲线分析。
- **多文件** — 只生成一个精简 workbook：仅 `Vg`/`Id` 两列（XYXY…），不建图——多曲线分析的
  数据入口。

### 单曲线分析

1. 点击图上要分析的那条 `Id` 曲线选中它，再运行 **Single-curve analysis**——会自动放置
   range cursor（检测到 forward/backward 双向扫描时放两组）。
2. 拖动 cursor 调整 SS/Vth 拟合区间（实线 = forward，点划线 = backward），拖动 Ioff 参考线
   设置 off-state 电流水平。
3. 再运行一次 **Single-curve analysis** 完成计算：图上画出拟合线和参数摘要（`[+]`/`[-]`），
   结果写入 `FETGraphData → Extracted Parameters`。
4. 点击图上的 `FET Gadget` 按钮打开 Device/Extraction/Cursors/Graph/Output 配置（`L`/`W`/
   `Cox`/`Vd`、平滑、拟合窗口大小、最小 R²、扫描模式、配色）。切换扫描模式不会删除另一侧的
   cursor。

### 多曲线分析

**从不弹出文件选择框**——直接读取当前已经激活的 worksheet 或 graph。

1. 运行 **Multi-Curve Analysis**（或点击结果图上的 `[FET Multi]` 按钮），设置器件参数、拟合
   参数、直方图 bin 数、统计图默认打开的参数。
2. 每条曲线自动完成拟合（带进度条和取消支持），生成：
   - **叠加图**（`FETMultiOverlayGraph`）— log `|Id|`/线性 `Id`，除 SS 最优的曲线保持不透明
     高亮外，其余均为半透明。
   - **统计图**（`FETStatsGraph`）— 一次显示一个参数的直方图（SS、Vth、gm、µFE、Ion、
     log₁₀(Ion/Ioff)），叠加正态拟合曲线；`[Prev]`/`[Next]` 循环切换六个参数。
   - **数据**（`FETStatsData`）— `Parameters`、`Statistics`、`Histogram`、`OverlayCurves` 四个
     sheet。

### 散点图 + 边缘直方图 / 相关性矩阵

两者都需要先运行过一次**多曲线分析**（读取已有的 `[FETStatsData]Parameters`），本身不会重新
拟合曲线。

- **散点图 + 边缘直方图** — 选 X/Y 轴参数（SS、Vth、gm、迁移率、Ion、Ioff、Ion/Ioff、
  log₁₀ 比值）；生成 `FETScatterGraph`（优先用 Origin 原生「Marginal Histograms」模板，否则
  回退为手工搭建的等效图）。
- **相关性矩阵** — 至少勾选两个参数；生成 `FETCorrelationGraph`（优先用原生「Scatter Matrix」
  模板，否则回退为系数表 `[FETStatsData]Correlation`）。

## 支持的 CSV 格式

参见 [`origin-app/FET Analyzer/examples/`](origin-app/FET%20Analyzer/examples/) 下的示例文件：

| 文件 | 格式 |
|---|---|
| `FET_transfer_sample.csv` | 最简单的 `Vg,Id` 两列表格 |
| `FET_transfer_double_scan.csv` | 单列 `Vg,Id`，一次连续的正扫+反扫 |
| `FET_transfer_split_scan.csv` | 显式拆分的 `Forward Vg,Forward Id,Backward Vg,Backward Id` 四列 |
| `FET_transfer_instrument_multicurve.csv` | 仪器导出格式：带元数据 + 多个 `DataName/DataValue` 数据块 |

## 输出说明

| 位置 | 内容 |
|---|---|
| 导入时创建的 workbook（如 `FETImportedData1`） | `Curves`：每条曲线 6 列（`Vg`/`Id`/`Ig`/`absId`/`Vd`/`logAbsId`）；`RawMeta`：跳过的原始文本行，便于排查导入问题。 |
| Id-Vg graph | 左轴 `\|Id\|` 对数坐标 + 右轴 `Id` 线性坐标叠加显示，forward 实线、backward 细实线；叠加拟合线/参考线/参数摘要。 |
| 隐藏 workbook `FETGraphData` → `Curves` | 当前正在分析的曲线的 forward/backward 拆分数据 + 合并后的完整曲线，供图上拟合线/hidden source plot 使用，一般不需要手动查看。 |
| 隐藏 workbook `FETGraphData` → `Extracted Parameters` | **主要结果表**：每条曲线每个方向一行，包含 SS、Vth、gm、迁移率、Ion/Ioff、滞回、拟合参数等全部数值列，可导出或用于批量整理。 |
| 多曲线 workbook（如 `FETMultiData1`） | `Curves`：一次导入多个文件时只有 `Vg`/`Id` 两列（XYXY…）。不建图——在这个 workbook 上运行多曲线分析。 |
| graph `FETMultiOverlayGraph` | 左轴对数 `\|Id\|`（靛蓝）+ 右轴线性 `Id`（琥珀）——经典单曲线配色，坐标轴同色，无图例。大部分曲线为真正的半透明，SS 最优的一条实线不透明高亮。带 `[FET Multi]` 重跑按钮。 |
| workbook `FETStatsData` + graph `FETStatsGraph` | 多曲线分析输出：`Parameters`（每条分析成功的曲线一行）、`Statistics`（每参数 N/均值/标准差/中位数/最值/CV）、`Histogram`（bin 数据）、`OverlayCurves`（叠加图所用派生数据），以及带 `[Prev]`/`[Next]` 按钮的单面板直方图（用于切换六个参数）。每次运行多曲线分析都会重建。 |
| graph `FETScatterGraph` | 两个所选批量参数的散点图，两条轴各带一个与坐标范围对齐的边缘直方图（优先原生 Origin 模板，否则自动回退为手工搭建版本）。每次运行散点图+直方图都会重建。 |
| graph `FETCorrelationGraph`（或作为回退的 workbook `FETStatsData` → `Correlation`） | 所勾选参数的原生 Scatter Matrix 图；若无法确认生成成功，则是 Pearson 相关系数表。每次运行相关性矩阵都会重建。 |

## 项目结构

```text
origin-app/FET Analyzer/       App 源码与打包资源（Origin Apps 目录下的实际名字是 "FET Gadget"）
  src/FETAnalyzer.c             核心 Origin C 实现（导入、图形、分析全部逻辑）
  launch.ogs                    App 启动入口（LabTalk）
  package.ini                   App 元数据（名称、版本、启用条件等）
  xfunctions/                   可选的 X-Function 封装说明（fet_analyze）
  examples/                     示例 CSV，覆盖各种支持的输入格式
  templates/                    图形/分析模板占位说明
tests/FETAnalyzerSmoke.c        运行时 smoke test（真实调用 Origin C 函数，覆盖导入/分析/UI 状态）
tools/
  build-opx.ps1                 一键编译 + smoke test + 打包 OPX
  check-package.ps1              纯静态的包结构/元数据检查（不需要 Origin）
docs/
  architecture.md                运行时数据流、图层/列布局等实现细节
  opx-packaging.md               OPX 打包与发布的详细步骤
data/                            开发过程中使用的真实测量数据样例
```

## 开发与测试

```powershell
# 完整构建：编译 + smoke test + 打包
powershell.exe -NoProfile -ExecutionPolicy Bypass -File .\tools\build-opx.ps1

# 跳过运行时 smoke test，只编译 + 打包（更快，但失去运行时验证）
powershell.exe -NoProfile -ExecutionPolicy Bypass -File .\tools\build-opx.ps1 -SkipSmoke

# 不依赖 Origin 的纯静态包结构检查
powershell.exe -NoProfile -ExecutionPolicy Bypass -File .\tools\check-package.ps1
```

`tests/FETAnalyzerSmoke.c` 会真实调用 Origin C 导出的函数（导入、放置 cursor、计算、切换扫描模式
等），并在检测到桌面上存在真实测量 CSV 时额外跑一遍真实数据的导入/分析，验证曲线数量、图层结构、
配置按钮、cursor 读写和参数计算结果。详见 [docs/architecture.md](docs/architecture.md) 中的运行时
数据流说明。

## 已知限制 / 后续计划

- 目前只支持恒流法之外的一种 Vth 提取方式（线性外推）；恒流法、最大 gm 法待补充。
- 多曲线分析固定使用自动 SS/Vth 窗口选择；逐条手动调整 cursor 只在单曲线流程中提供。
- 叠加图的半透明使用 Origin 线条透明度格式标签实现，在当前构建上验证可正确读写，但未在所有
  Origin 版本上逐一确认；若某个安装不支持该标签会自动退回不透明实线，而不是报错。
- 栅极漏电（`Ig`）已导入但尚未用于分析。
- 图形/分析设置目前只在当前 Origin 会话内生效，尚未持久化到 graph 对象或工程文件。
- 散点图+直方图与相关性矩阵都会优先尝试 Origin 原生图库模板（`plot_marginal`/`plotmatrix`），
  若无法确认生成真实结果则自动回退为手工搭建的图/表。这套回退逻辑已经过充分验证，但原生模板本身
  是否能在真实交互式 Origin 会话中正确渲染尚未独立确认——如果你发现始终只看到回退结果（散点图+
  直方图的手工搭建版本，或相关性矩阵的纯系数表而非 Scatter Matrix 图），请
  [提交 issue](../../issues) 以便进一步调整检测逻辑。
- 散点图+直方图回退版本右侧的边缘直方图用的是水平条形图类型，其实际渲染方向未经独立可视化确认
  （无论方向如何都能正常编译并生成有效的 plot 对象）。

## License

[MIT](LICENSE)
