# A10 合成源黄色显示字节序修复记录

> 日期：2026-07-12
> 范围：仅 `synthetic_2ppc_source` 合成验证源及其独立 testbench。
> 状态：源码已修复；动态仿真和上板显示尚未验证。

## 现象

HDMI 循环中出现深蓝和浅蓝，未见黄色。颜色顺序应为红、蓝、黄、白、黑。

## 根因

黄色的内部 RGB 常量正确，为 `{8'hFF, 8'hFF, 8'h00}`。但合成源此前把每个内部 `{R,G,B}` 像素反转成 `{B,G,R}` 后送入专用 HDMI CDC。

`video_2pix_to_1pix_cdc` 先输出输入总线低 24 位、再输出高 24 位，且 HDMI 末端按 `{R,G,B}` 解读。因此反转后的黄色 `{00,FF,FF}` 被显示为青色/浅蓝；蓝色 `{00,00,FF}` 被显示为红色通道值，在当前链路现象中会形成第二种蓝色感受。该问题仅影响直接进入 HDMI CDC 的合成源，不改变摄像头 Debayer 数据路径。

## 修复

- `fpga/rtl/feature_extract/synthetic_2ppc_source.v`
  - 将 2ppc 输出改为 `{pixel1_rgb, pixel0_rgb}`，即 `{R1,G1,B1,R0,G0,B0}`。
  - 保留红、蓝、黄、白、黑五色的 RGB 常量和每 60 帧切色逻辑。
- `tests/fpga_sim/feature_extract/tb_synthetic_2ppc_source.v`
  - 从只检查红色一帧扩展为检查红、蓝、黄、白、黑五帧。
  - 明确验证黄色为 `48'hFF_FF_00_FF_FF_00`。

## 验证状态

- 静态交叉核对：通过。
  - `video_2pix_to_1pix_cdc` 先取 `i_data[23:0]`，后取 `i_data[47:24]`。
  - 修复后的合成源低/高 24 位均为标准 `{R,G,B}` 像素。
- `git diff --check`：通过。
- 独立 Icarus testbench：未执行。本机及 `D:\Efinity\2025.2` 未找到 `iverilog.exe`。
- Efinity map/PNR、bitstream、下载和 HDMI 画面：未执行。

## D 盘构建树核对与同步

- 2026-07-12 19:29 的 HDMI 录像对应的 `D:\final_project\fpga\efinity\outflow\mem_test.bit` 生成于 19:26。
- 当时 `D:\final_project\fpga\rtl\feature_extract\synthetic_2ppc_source.v` 仍是旧版：其 2ppc 输出仍显式按 `B,G,R` 反转字节。因此该 bitstream 不包含本记录的黄色修复，不能用于判断修复是否有效。
- 已仅同步修复后的 `synthetic_2ppc_source.v` 到 D 盘同路径；C/D 两份文件 SHA-256 一致：`B4780BC58CDACAA0B288D35301D54C38824A3402602E9A0672DF6AFD8052CA0D`。
- 未同步整棵工程，未运行综合、PNR、下载或烧录。

## 手动验证步骤

1. `synthetic_2ppc_source.v` 已同步到实际构建树 `D:\final_project`；testbench 不参与 Efinity 构建，无需同步。
2. 重新运行 Efinity map、PNR 和 bitstream 生成；记录新的时序和 warning。
3. 手动下载新 bitstream 后等待至少 5 个颜色周期。
4. 预期画面为：红、蓝、黄、白、黑依次循环，黄色应为纯黄而非浅蓝/青色。

## NOT VERIFIED

- 新 bitstream 是否已进入开发板。
- HDMI 面板最终色彩空间、色温和实际视觉效果。
- 合成源以外的摄像头、Debayer、预处理、CPU、OSD、SoC 与机械臂路径。
