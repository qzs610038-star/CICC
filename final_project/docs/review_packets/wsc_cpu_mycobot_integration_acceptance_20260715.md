# wsc CPU × myCobot 集成验收包（2026-07-15）

## 1. 验收目标

本验收包用于确认代码集成提交 `64bbae69520d9663456f175aa7a3e66b0605f496` 是否可以作为 CPU 集成 PR 候选。CPU 语义与 Host 行为以 wsc 的 MinGW/Efinity 环境为主验收环境；Codex 的 MSVC 结果作为跨编译器兼容门。

本轮只覆盖 G0–G3 软件门：统一结果语义、ARM_DISABLED 候选 adapter、task_matcher 真值 reason、`arm_runtime` structural bridge 和 `NOT_FOR_FLASH` 目标构建。它不放行正式 SoC、APB/OSD wire ABI、烧录、UART2、J52 接线或真实机械臂动作。

## 2. 已完成整改

- 保留 wsc 的 `cpu_result_semantics`、`main_loop_adapter` 和 task_matcher 真值 reason。
- 保留 main 已有 `arm_runtime`、`APP_PROFILE`、disabled/simulated 后端和 real transport 排除门。
- 正式 `main.c` 通过 `cpu_display_from_round_output()` 消费统一语义；矛盾 action/reason/is_target 组合 fail closed。
- 候选 `main_loop_adapter.c` 已加入 competition 构建清单，但在 G4 取得受审事件源和单调时基前不由正式 main 调用。
- 修复两个 `PASS()` 宏的 C4456 变量遮蔽。
- Host 脚本支持 `-Compiler auto|msvc|gcc`，VS 使用显式路径、`VCVARS64_PATH` 或 `vswhere` 发现。
- 两份 Host runner 保持 ASCII-only，避免 Windows PowerShell 5.1 将无 BOM UTF-8 中文注释按本地代码页误解码并破坏命令解析。
- GCC 严格门增加 `-Wshadow -Werror`。
- 正式构建器纳入三个 wsc 新源文件，并修复 Windows PowerShell 丢失完整 gcc stderr 的问题；退出码和 warning allowlist 没有放宽。

## 3. Codex 本机结果

- MSVC 19.42 `/std:c11 /W4 /WX`：`cpu_result_semantics 374/374 PASS`。
- MSVC 19.42 `/std:c11 /W4 /WX`：`main_loop_arm_disabled 117/117 PASS`。
- Efinity RISC-V GCC 8.3.0：`competition/arm_bringup × disabled/simulated` 四组合 BUILD/ELF PASS。
- 四组合全部为 `NOT_FOR_FLASH=True`，UART2/real transport 排除门保持有效。
- Codex 本机没有 PATH gcc，也没有仓库 `tools/mingw64/bin/gcc.exe`，因此带 `-Wshadow` 的 MinGW 复验必须由 wsc 环境完成。
- Windows PowerShell 5.1 与 PowerShell 7 均应使用仓库脚本原文；不得重新加入非 ASCII 注释，除非团队同时冻结 UTF-8 BOM 编码策略。

## 4. 先前复现失败的准确记录

wsc 原环境报告 MinGW GCC 14.2.0 下 `374/374`、`117/117` PASS。Codex 在修复前使用 MSVC 19.42 `/W4 /WX` 时，两项测试均因 `PASS()` 宏局部 `int d` 遮蔽调用函数的同名业务变量而触发 C4456，并在断言执行前停止。

这属于环境警告策略差异暴露出的真实测试可移植性问题，不是已证实的 CPU 功能逻辑失败。禁止使用 `/wd4456`、降低 `/WX`、删除断言或移除 `-Wshadow` 规避。

## 5. 交给 wsc Agent 的直接执行命令

在仓库根目录使用 PowerShell 执行；验收时必须记录实际 commit、编译器版本、完整控制台输出和退出码。

```powershell
$ErrorActionPreference = 'Stop'
git fetch origin
$expected = '64bbae69520d9663456f175aa7a3e66b0605f496'
git rev-parse --verify $expected
git switch --detach $expected
if ((git status --porcelain).Count -ne 0) { throw 'worktree is not clean' }
if ((git rev-parse HEAD) -ne $expected) { throw 'unexpected commit' }

$gcc = Resolve-Path '.\tools\mingw64\bin\gcc.exe'
& $gcc --version

powershell -NoProfile -ExecutionPolicy Bypass -File `
  '.\final_project\cpu\tests\run_cpu_result_semantics_host.ps1' `
  -Compiler gcc
if ($LASTEXITCODE -ne 0) { throw "cpu_result_semantics failed: $LASTEXITCODE" }

powershell -NoProfile -ExecutionPolicy Bypass -File `
  '.\final_project\cpu\tests\run_main_loop_arm_disabled_host.ps1' `
  -Compiler gcc
if ($LASTEXITCODE -ne 0) { throw "main_loop_arm_disabled failed: $LASTEXITCODE" }

$toolchain = 'D:\Efinity\efinity-riscv-ide-2025.2\toolchain'
$out = Join-Path $env:TEMP 'wsc-cpu-mycobot-acceptance'
if (Test-Path -LiteralPath $out) { Remove-Item -LiteralPath $out -Recurse -Force }

foreach ($profile in @('competition', 'arm_bringup')) {
  foreach ($backend in @('disabled', 'simulated')) {
    powershell -NoProfile -ExecutionPolicy Bypass -File `
      '.\final_project\cpu\build_tools\build_arm_profile.ps1' `
      -Profile $profile -Backend $backend `
      -OutputRoot $out -ToolchainPath $toolchain
    if ($LASTEXITCODE -ne 0) {
      throw "target build failed: $profile/$backend exit=$LASTEXITCODE"
    }
  }
}

Get-ChildItem -LiteralPath $out -Recurse -Filter '*.manifest.json' |
  Select-Object FullName
git diff --check origin/main...HEAD
```

如果 wsc 机器的 Efinity 安装位置不同，只允许修改 `$toolchain` 为实际 Efinity 2025.2 RISC-V toolchain 根目录，不得修改编译 flags 或源文件清单。

## 6. Agent 验收回报格式

```text
commit: <40位 SHA>
worktree_clean: PASS/FAIL
gcc_version: <完整第一行>
cpu_result_semantics: PASS/FAIL，断言数 <n/n>
main_loop_arm_disabled: PASS/FAIL，断言数 <n/n>
competition_disabled: BUILD/ELF PASS/FAIL，manifest=<path>
competition_simulated: BUILD/ELF PASS/FAIL，manifest=<path>
arm_bringup_disabled: BUILD/ELF PASS/FAIL，manifest=<path>
arm_bringup_simulated: BUILD/ELF PASS/FAIL，manifest=<path>
unexpected_warnings: 0/<数量与原文>
flags_or_source_list_changed: NO/YES（若 YES 必须说明，默认不得验收）
verdict: APPROVE/BLOCK
```

## 7. 阻塞条件

以下任一项出现即 BLOCK：

- 不是指定集成 commit，或开始验收时工作区不干净。
- GCC 两项 Host 测试未以 `-Wshadow -Werror` 编译并执行。
- 断言数不是 `374/374` 和 `117/117`，或出现跳过测试。
- 四个目标组合任一 BUILD/ELF 失败。
- manifest 未标记 `NOT_FOR_FLASH`，或 UART2/real transport 排除证据失效。
- 为通过测试关闭 warning、修改 flags、删除断言或改变源文件清单。
- 把 Host/compile-only 结果描述成板级、UART2 或真实机械臂闭环。
