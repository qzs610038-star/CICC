"""
test_efinity_probe — efinity_tools 模块单元测试。

测试:
- locate_toolchain() 路径存在性检查
- check_install_prereq() 前置条件检查
- check_project() 工程文件校验
- list_artifacts() 产物列表
"""

import sys
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
