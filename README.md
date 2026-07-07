# FET Gadget (FET Analyzer) for Origin

[![License: MIT](https://img.shields.io/badge/License-MIT-yellow.svg)](LICENSE)
[![Origin](https://img.shields.io/badge/Origin-10.3%2B-blue.svg)](#环境要求)

面向 Origin/OriginPro 的交互式 FET（场效应晶体管）transfer curve 分析 App。导入 CSV 原始数据，
在图上用可拖动的 range cursor 圈定拟合区间，一键提取 SS、Vth、gm、迁移率、Ion/Ioff 等参数，并把
结果整理进结果表，支持正向/反向双扫描（forward/backward）分别提取和累计多条曲线的结果。

App 内部名称为 **FET Gadget**（`package.ini` / OPX 文件名 / Origin Apps 目录下的文件夹名都是
`FET Gadget`），仓库目录沿用了早期的 `FET Analyzer` 命名，两者指同一个 App。

## 目录

- [功能](#功能)
- [环境要求](#环境要求)
- [安装](#安装)
  - [方式一：安装已构建的 OPX（推荐给使用者）](#方式一安装已构建的-opx推荐给使用者)
  - [方式二：从源码构建（推荐给开发者）](#方式二从源码构建推荐给开发者)
- [使用方法](#使用方法)
  - [1. 导入数据](#1-导入数据)
  - [2. 选中曲线并放置 cursor](#2-选中曲线并放置-cursor)
  - [3. 调整拟合区间](#3-调整拟合区间)
  - [4. 查看结果](#4-查看结果)
  - [5. 配置项（Settings）](#5-配置项settings)
  - [支持的 CSV 格式](#支持的-csv-格式)
- [输出说明](#输出说明)
- [项目结构](#项目结构)
- [开发与测试](#开发与测试)
- [已知限制 / 后续计划](#已知限制--后续计划)
- [License](#license)

## 功能

- 导入仪器 CSV：支持单文件多段 `DataName/DataValue` 数据块，也支持普通 `Vg/Id` 表格，以及显式拆分好
  的 `Forward Vg/Forward Id/Backward Vg/Backward Id` 列。
- 自动识别双向扫描（forward 上扫 + backward 下扫），分别作为独立曲线处理。
- 自动生成 workbook（`Curves`/`RawMeta`）和双 Y 轴 Id-Vg graph：左轴为 `|Id|` 对数坐标，右轴为
  `Id` 线性坐标，两者按沟道宽度归一化显示为 `uA/um`。
- 图上提供两组可水平拖动的自由 range cursor：
  - 实线一对：forward（正向）SS 亚阈值拟合范围 + Vth 线性外推范围。
  - 点划线一对：backward（反向）对应范围，与实线视觉上明确区分。
- 计算 SS、Vth、最大 gm、场效应迁移率、Ion/Ioff、滞回（hysteresis）等参数。
- 图中叠加 SS 拟合线、Vth 拟合/截线、gm/Vth 参考线、on/off 参考线、summary 文本框，forward/backward
  分别标注为 `[+]`/`[-]`。
- Ioff 参考线可拖动，松手即重新计算 Ioff 和 Ion/Ioff 并更新图和结果表。
- 结果按"每条曲线 × 每个扫描方向"一行，写入隐藏的 `[FETGraphData]Extracted Parameters` 表：同一条
  曲线重复分析（例如拖动 cursor 微调）会更新已有的那一行，分析不同曲线则追加新行，不会互相覆盖。
- 图上提供 `FET Gadget` 配置按钮，可打开 Device / Extraction / Cursors / Graph / Output 分组配置
  对话框，切换 Auto/Forward/Backward/Both 扫描模式只影响本次绘图与分析，不会清空另一侧已调好的
  cursor 位置。

## 环境要求

- **Origin 或 OriginPro 2022 及以上**（`package.ini` 中声明的最低版本为 **10.3**）。
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

生成的 `build/FET Gadget.opx` 可以按方式一的方法拖入任意一台安装了 Origin 的机器完成安装；构建
过程本身也会把 App 安装/更新到本机的 Origin Apps 目录，因此本机可以直接在 Origin 里试用而不需要
再手动拖拽安装。

也可以手动开发安装（不打 OPX，直接在 Code Builder 里改）：把 `origin-app/FET Analyzer` 整个复制
到 `%LocalAppData%\OriginLab\Apps\FET Gadget`，在 Origin 的 Code Builder 中把该文件夹加入
workspace 即可直接调试。更完整的打包细节见 [docs/opx-packaging.md](docs/opx-packaging.md)。

## 使用方法

### 1. 导入数据

在任意 workbook 或空白窗口下启动 **FET Gadget**（App Gallery 图标或工具栏按钮），在弹出的文件选择
框中选择一个或多个 CSV 文件。App 会为每个文件（以及文件内每个 `DataName` 数据块）创建一条曲线，
生成一个 workbook（`Curves` + `RawMeta` 两个 sheet）和一张双 Y 轴 Id-Vg graph。

### 2. 选中曲线并放置 cursor

在 graph 中点击你要分析的那条右轴 `Id` 曲线，使其成为 active plot（一张图上可能有多条曲线，必须
先点选）。再次启动 **FET Gadget**：如果图上还没有 range cursor，App 会根据数据自动放置一组（或两组，
如果检测到 forward+backward 双向扫描）。

### 3. 调整拟合区间

拖动竖线 cursor 调整拟合范围：

- 实线一对覆盖 forward 的 SS 亚阈值区域 / Vth 线性导通段。
- 点划线一对（如果存在）覆盖 backward 对应区域。

拖动图中橙色/红色的 Ioff 水平参考线可以手动指定 off-state 电流水平，松手会立即重新计算 Ioff 和
Ion/Ioff。Ion 参考线是不可拖动的固定参考。

### 4. 查看结果

再次启动 **FET Gadget** 完成计算。图上会显示 SS/Vth 拟合线和参数摘要文本框（forward 标 `[+]`，
backward 标 `[-]`）。完整结果同时写入隐藏 workbook `FETGraphData` 的 `Extracted Parameters` sheet
（每条曲线每个方向一行，可累计多条曲线的历史结果）。

### 5. 配置项（Settings）

点击图上的 `FET Gadget` 按钮打开配置对话框，可调整：

- **Device**：沟道长宽 `L`/`W`、栅氧电容 `Cox`（直接输入或按材料/厚度换算）、`Vd`。
- **Extraction**：平滑窗口、SS/Vth 自动拟合窗口大小、最小 R²。
- **Cursors**：扫描模式 Auto/Forward/Backward/Both、是否重新放置 cursor。
- **Graph**：曲线/坐标轴配色。
- **Output**：是否标注图、是否追加结果表、是否显示拟合线/marker。

切换扫描模式只影响*这一次*要绘制和分析哪个方向，不会删除另一侧已经调好的 cursor——切回去可以立刻
复用，不需要重新从头调整。

### 支持的 CSV 格式

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
| Id-Vg graph | 左轴 `\|Id\|` 对数坐标 + 右轴 `Id` 线性坐标叠加显示，forward 实线、backward 点划线，叠加拟合线/参考线/参数摘要。 |
| 隐藏 workbook `FETGraphData` → `Curves` | 当前正在分析的曲线的 forward/backward 拆分数据 + 合并后的完整曲线，供图上拟合线/hidden source plot 使用，一般不需要手动查看。 |
| 隐藏 workbook `FETGraphData` → `Extracted Parameters` | **主要结果表**：每条曲线每个方向一行，包含 SS、Vth、gm、迁移率、Ion/Ioff、滞回、拟合参数等全部数值列，可导出或用于批量整理。 |

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
- 尚未提供跨曲线的批量报表导出（结果已经在 `Extracted Parameters` 表里逐行累计，可自行用 Origin
  的导出功能整理）。
- 栅极漏电（`Ig`）已导入但尚未用于分析。
- 图形/分析设置目前只在当前 Origin 会话内生效，尚未持久化到 graph 对象或工程文件。

## License

[MIT](LICENSE)
