# OPX 打包与安装

## 开发安装

1. 将 `origin-app/FET Analyzer` 复制到：

   `%LocalAppData%\OriginLab\Apps\FET Gadget`

   （目录名要用 `package.ini` 里的 App 名称 `FET Gadget`，不是仓库里的文件夹名 `FET Analyzer`。）

2. 在 Origin Code Builder 中把该 App 文件夹加入 workspace。
3. 右键 App 文件夹，选择 **Generate...** 打开 Package Manager。
4. 核对 `package.ini` 中的元数据、App icon、Launch Script 和 Graph 启用条件。
5. 用 Package Manager 的 **File > Save** 保存为 `FET Gadget.opx`。
6. 把生成的 `.opx` 拖入 Origin 完成安装或覆盖开发版本。

Origin 官方 App 开发流程要求先生成并安装一次 OPX，App Gallery 才能正常管理和启动该 App。后续修改应从 Code Builder workspace 中的 App 文件夹再次执行 **Generate...**，不要直接打开旧 OPX 修改。

**`package.ini` 的 `[Package] Version` 必须是最多两位小数的纯数字**（OriginLab 官方文档示例：`1.11`），不支持三段式语义化版本号（如 `0.11.0`）——用三段式会导致 App Gallery 里版本号显示错乱（例如 `v-0.00`）。仓库里更细粒度的三段式版本号只保留在 `FETAnalyzer.c` 的 `FET_APP_TITLE`（对话框标题里显示）和 commit message 里；`package.ini` 的 `Version` 每次发布只需递增到下一个两位小数（如 `0.12`、`0.13`），不必和 `FET_APP_TITLE` 的补丁号一一对应。

## 发布前 X-Function 步骤

MVP 不依赖 `.OXF` 即可运行。若要发布稳定的脚本/Analysis Template API：

1. 按 `xfunctions/fet_analyze.variables.md` 在 X-Function Builder（F10）创建 `fet_analyze`。
2. 将 Builder 生成的 main function body 替换为 `xfunctions/fet_analyze_xf_main.inc` 的对应内容。
3. 将生成的 `fet_analyze.oxf` 放回 App 的 `xfunctions` 文件夹。
4. 在 Package Manager 中确认 `.oxf` 和 `src/FETAnalyzer.c` 都已列入 Files。

`.OXF` 应由目标 Origin 版本的 X-Function Builder 生成。仓库刻意不提交手工伪造的 `.oxf`，因为其框架代码和序列化格式由 Origin 生成并随版本演进。

## 模板

在 Origin 中完成论文风格图后，将图保存为 `.otpu`，放入 `templates`；分析模板可保存为 `.ogwu`。二进制模板同样应由 Origin 创建，不应以文本占位文件冒充。

## 静态检查

在 PowerShell 中运行：

```powershell
./tools/check-package.ps1
```

静态检查只能验证文件和元数据。Origin C 编译、图形对象行为和 OPX 安装必须在目标 Origin 版本内执行集成测试。

## 自动构建

Origin 已正确注册为 COM Server 时，可在仓库根目录运行：

```powershell
powershell.exe -NoProfile -ExecutionPolicy Bypass -File .\tools\build-opx.ps1
```

该脚本依次执行核心 Origin C 编译、`tests/FETAnalyzerSmoke.c` 运行时测试和 `mkOPX`，输出到 `build/FET Gadget.opx`（`mkOPX` 按 `package.ini` 中的 App 名称 `FET Gadget` 打包，与仓库目录名 `FET Analyzer` 不同）。
