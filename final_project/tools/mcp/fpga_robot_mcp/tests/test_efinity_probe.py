"""
test_efinity_probe — efinity_tools 模块单元测试。

测试:
- locate_toolchain() 路径存在性检查
- check_install_prereq() 前置条件检查
- check_project() 工程文件校验
- list_artifacts() 产物列表
"""

import sys
import tempfile
from pathlib import Path

_src = Path(__file__).resolve().parent.parent / "src"
if str(_src) not in sys.path:
    sys.path.insert(0, str(_src))

from fpga_robot_mcp import efinity_tools


def test_locate_toolchain_returns_dict():
    """locate_toolchain() 应返回包含 status 的 dict。"""
    result = efinity_tools.locate_toolchain()
    assert isinstance(result, dict)
    assert "status" in result


def test_locate_toolchain_has_checks():
    """locate_toolchain() 应包含 checks 字段。"""
    result = efinity_tools.locate_toolchain()
    data = result.get("data", result)
    if "checks" in data:
        checks = data["checks"]
        assert "home_dir" in checks


def test_locate_toolchain_has_found_tools():
    """locate_toolchain() 应包含 found_tools 列表。"""
    result = efinity_tools.locate_toolchain()
    data = result.get("data", result)
    if "found_tools" in data:
        assert isinstance(data["found_tools"], list)


def test_check_install_prereq_returns_dict():
    """check_install_prereq() 应返回 dict。"""
    result = efinity_tools.check_install_prereq()
    assert isinstance(result, dict)
    assert "status" in result


def test_check_install_prereq_has_rar_check():
    """check_install_prereq() 应检查 WinRAR。"""
    result = efinity_tools.check_install_prereq()
    data = result.get("data", result)
    rar_keys = [k for k in data.keys() if "rar" in k.lower()]
    assert len(rar_keys) > 0, "应包含 RAR 相关检查项"


def test_check_install_prereq_has_disk_check():
    """check_install_prereq() 应检查磁盘空间。"""
    result = efinity_tools.check_install_prereq()
    data = result.get("data", result)
    disk_keys = [k for k in data.keys() if "disk" in k.lower()]
    assert len(disk_keys) > 0, "应包含磁盘空间检查项"


def test_check_project_returns_dict():
    """check_project() 应返回 dict。"""
    result = efinity_tools.check_project()
    assert isinstance(result, dict)
    assert "status" in result


def test_check_project_has_device():
    """check_project() 应包含器件信息。"""
    result = efinity_tools.check_project()
    data = result.get("data", result)
    if "device" in data:
        assert data["device"] == "TJ375N529"


def test_check_project_has_checks():
    """check_project() 应包含 checks 子字段。"""
    result = efinity_tools.check_project()
    data = result.get("data", {})
    assert "checks" in data
    checks = data["checks"]
    assert "project_xml_exists" in checks
    assert "constrain_sdc_exists" in checks


def test_list_artifacts_returns_dict():
    """list_artifacts() 应返回 dict 而非抛出异常。"""
    result = efinity_tools.list_artifacts(limit=10, offset=0)
    assert isinstance(result, dict)
    assert "status" in result


def test_list_artifacts_has_pagination():
    """list_artifacts() 应包含 limit/offset 分页信息。"""
    result = efinity_tools.list_artifacts(limit=5, offset=0)
    data = result.get("data", {})
    if "limit" in data:
        assert data["limit"] == 5


def test_list_artifacts_returns_list():
    """list_artifacts() 的 artifacts 应为列表。"""
    result = efinity_tools.list_artifacts()
    data = result.get("data", {})
    if "artifacts" in data:
        assert isinstance(data["artifacts"], list)


def test_parse_list_usb_output():
    """--list_usb 输出应解析出 URL。"""
    stdout = """Available USB targets:
Usb Target: YLS_4232DL, Bus 002 Device 013: ID 0403:6011 S/N FTBI7G42
        url[0]: None
        url[1]: ftdi://0x0403:0x6011:2:d/2
        url[2]: None
"""
    targets = efinity_tools._parse_list_usb_output(stdout)
    assert len(targets) == 1
    assert targets[0]["name"] == "YLS_4232DL"
    assert targets[0]["serial_no"] == "FTBI7G42"
    assert targets[0]["urls"][0]["url"] == "ftdi://0x0403:0x6011:2:d/2"


def test_parse_scan_usb_output_no_idcode():
    """--scan_usb JSON 中空 idcode 应表示 JTAG ID 不可见。"""
    stdout = '[{"vid":"0x0403","pid":"0x6011","serial_no":"FTBI7G42","idcode":[],"ir_width":[]}]'
    targets = efinity_tools._parse_scan_usb_output(stdout)
    assert len(targets) == 1
    assert efinity_tools._has_scan_usb_target(targets) is True
    assert efinity_tools._has_jtag_idcode(targets) is False


def test_parse_scan_usb_output_error_not_visible():
    """--scan_usb 错误文本不应被当成 USB target。"""
    targets = efinity_tools._parse_scan_usb_output("ERROR: No USB target detected, aborting!")
    assert len(targets) == 1
    assert efinity_tools._has_scan_usb_target(targets) is False
    assert efinity_tools._has_jtag_idcode(targets) is False


def test_parse_scan_usb_output_with_idcode():
    """--scan_usb JSON 中非空 idcode 应表示 JTAG ID 可见。"""
    stdout = '[{"vid":"0x0403","pid":"0x6011","serial_no":"FTBI7G42","idcode":["0x12345678"],"ir_width":[10]}]'
    targets = efinity_tools._parse_scan_usb_output(stdout)
    assert efinity_tools._has_scan_usb_target(targets) is True
    assert efinity_tools._has_jtag_idcode(targets) is True


def test_assess_program_result_accepts_valid_jtag_id():
    """JTAG 烧录成功必须包含有效 Device ID 和 finished 标志。"""
    stdout = """jtag programming started!
Device ID read from JTAG: 0x006A0EF3
... finished with JTAG programming
"""
    result = efinity_tools._assess_program_result("jtag", 0, stdout, "")
    assert result["status"] == "ok"
    assert result["valid_device_ids"] == ["0X006A0EF3"]


def test_assess_program_result_rejects_all_ones_jtag_id():
    """0xFFFFFFFF 常见于未上电/链路浮空，返回码 0 也不能算成功。"""
    stdout = """jtag programming started!
Device ID read from JTAG: 0xFFFFFFFF
... finished with JTAG programming
"""
    result = efinity_tools._assess_program_result("jtag", 0, stdout, "")
    assert result["status"] == "program_suspicious"
    assert result["valid_device_ids"] == []


def test_run_build_dry_run_uses_project_flow():
    """构建 dry-run 应生成 Efinity project flow 命令。"""
    result = efinity_tools.run_build(dry_run=True)
    assert result["status"] == "ok"
    data = result["data"]
    assert data["flow"] == "compile"
    assert "efx_run.bat" in data["command"]
    assert '"--prj"' in data["command"]
    assert '"-f"' in data["command"]
    assert '"compile"' in data["command"]


def test_run_build_dry_run_can_disable_debug():
    """disable_debug dry-run 应预览临时 no-debug 工程 XML。"""
    result = efinity_tools.run_build(dry_run=True, disable_debug=True)
    assert result["status"] == "ok"
    data = result["data"]
    assert data["disable_debug"] is True
    assert data["generated_project_xml"].endswith(".codex_nodebug.xml")
    assert ".codex_nodebug.xml" in data["command"]
    assert Path(data["generated_project_xml"]).name in data["next_step"]
    assert data["command"] in data["next_step"]


def test_cleanup_generated_project_xml_removes_temp_file():
    """临时 no-debug 工程 XML 应可在构建结束后清理。"""
    with tempfile.NamedTemporaryFile(suffix=".codex_nodebug.xml", delete=False) as f:
        temp_path = Path(f.name)
        f.write(b"<project />")
    cleanup = efinity_tools._cleanup_generated_project_xml(temp_path)
    assert cleanup["removed"] is True
    assert cleanup["error"] == ""
    assert not temp_path.exists()


def test_program_bitstream_dry_run_returns_command():
    """dry-run 只生成烧录命令，不应真实执行。"""
    with tempfile.NamedTemporaryFile(suffix=".bit", delete=False) as f:
        f.write(b"dummy")
        bit_path = f.name
    try:
        result = efinity_tools.program_bitstream(bit_path, mode="jtag", dry_run=True)
        assert result["status"] == "dry_run"
        data = result["data"]
        assert '"-m"' in data["command"]
        assert '"jtag"' in data["command"]
        assert "ftdi_program.py" in data["command"]
    finally:
        Path(bit_path).unlink(missing_ok=True)


def test_program_bitstream_rejects_wrong_extension():
    """jtag 模式不应接受 .hex。"""
    with tempfile.NamedTemporaryFile(suffix=".hex", delete=False) as f:
        f.write(b"dummy")
        hex_path = f.name
    try:
        result = efinity_tools.program_bitstream(hex_path, mode="jtag", dry_run=True)
        assert result["status"] == "error"
        assert "jtag" in result["message"]
    finally:
        Path(hex_path).unlink(missing_ok=True)


if __name__ == "__main__":
    import inspect
    failures = 0
    for name, func in sorted(globals().items()):
        if name.startswith("test_") and callable(func):
            try:
                func()
                print(f"  ✅ {name}")
            except Exception as e:
                print(f"  ❌ {name}: {e}")
                failures += 1
    total = len([k for k in globals() if k.startswith("test_") and callable(globals()[k])])
    print(f"\n{'-' * 40}")
    print(f"{total - failures}/{total} 通过")
    sys.exit(failures)
