"""
test_config — FpgaRobotConfig 加载与路径解析测试。
"""

import json
import os
import sys
import tempfile
from pathlib import Path

# 确保 src 在 path 中
_src = Path(__file__).resolve().parent.parent / "src"
if str(_src) not in sys.path:
    sys.path.insert(0, str(_src))

from fpga_robot_mcp.config import (
    FpgaRobotConfig,
    load_config,
    get_default_config,
    hide_sensitive,
)


def test_get_default_config():
    """默认配置应包含合理的默认路径。"""
    cfg = get_default_config()
    assert cfg.project_root == "D:/第十届集创赛-雄芯院材料"
    assert cfg.efinity.home == "D:/Efinity/2025.2"
    assert cfg.efinity.bin == "D:/Efinity/2025.2/bin"
    assert cfg.efinity.project_xml == "final_project/fpga/efinity/mem_test.xml"
    assert cfg.efinity.burn_project_root == "D:/final_project_shaolu"
    assert cfg.programmer.ftdi_program_py.endswith("ftdi_program.py")
    assert cfg.programmer.default_mode == "jtag"
    assert cfg.mycobot280.baudrate == 1000000
    assert cfg.mycobot280.allow_motion is False
    assert cfg.safety.allow_hardware_actions is False


def test_load_config_from_json():
    """从 JSON 文件加载配置应正确解析各字段。"""
    config_data = {
        "project_root": "D:/test",
        "efinity": {
            "home": "D:/Efinity/2025.2",
            "device": "TJ375N529",
        },
        "mycobot280": {
            "allow_motion": True,
        },
    }
    with tempfile.NamedTemporaryFile(
        mode="w", suffix=".json", delete=False, encoding="utf-8"
    ) as f:
        json.dump(config_data, f)
        tmp_path = f.name

    try:
        cfg = load_config(tmp_path)
        assert cfg.project_root == "D:/test"
        assert cfg.efinity.home == "D:/Efinity/2025.2"
        assert cfg.efinity.device == "TJ375N529"
        assert cfg.mycobot280.allow_motion is True
        # 未覆盖的字段应保留默认值
        assert cfg.mycobot280.baudrate == 1000000
    finally:
        os.unlink(tmp_path)


def test_load_config_nonexistent():
    """配置文件不存在时应返回默认配置而非崩溃。"""
    cfg = load_config("/nonexistent/path/config.json")
    assert cfg is not None
    assert isinstance(cfg, FpgaRobotConfig)


def test_resolve_relative():
    """resolve() 应正确拼接相对路径。"""
    cfg = get_default_config()
    resolved = cfg.resolve("赛方提供材料", "mem_test.xml")
    assert str(resolved).replace("\\", "/").endswith("赛方提供材料/mem_test.xml")
    assert str(resolved).startswith("D:")


def test_resolve_absolute():
    """resolve() 对绝对路径应直接返回。"""
    cfg = get_default_config()
    resolved = cfg.resolve("C:/Windows")
    assert str(resolved) == "C:\\Windows" or str(resolved) == "C:/Windows"


def test_efinity_project_path():
    """efinity_project_path() 应返回工程 XML 的完整路径。"""
    cfg = get_default_config()
    path = cfg.efinity_project_path()
    assert isinstance(path, Path)
    assert "mem_test.xml" in str(path)
    assert "final_project" in str(path)


def test_hide_sensitive():
    """hide_sensitive() 应隐藏 token 等敏感字段。"""
    cfg = get_default_config()
    hidden = hide_sensitive(cfg)
    # 配置中不应有明文的 token
    assert "token" not in str(hidden) or "****" in str(hidden)


def test_load_config_env_override():
    """环境变量 FPGA_ROBOT_MCP_CONFIG 应被优先使用。"""
    config_data = {
        "project_root": "D:/env_test",
    }
    with tempfile.NamedTemporaryFile(
        mode="w", suffix=".json", delete=False, encoding="utf-8"
    ) as f:
        json.dump(config_data, f)
        tmp_path = f.name

    try:
        os.environ["FPGA_ROBOT_MCP_CONFIG"] = tmp_path
        cfg = load_config()
        assert cfg.project_root == "D:/env_test"
    finally:
        os.unlink(tmp_path)
        os.environ.pop("FPGA_ROBOT_MCP_CONFIG", None)


def test_efinity_toolchain_prefix():
    """工具链前缀应为 riscv-none-embed-（对应实际安装）。"""
    cfg = get_default_config()
    assert cfg.efinity.toolchain_prefix == "riscv-none-embed-"


def test_safety_defaults():
    """安全配置默认值应禁止硬件操作。"""
    cfg = get_default_config()
    assert cfg.safety.allow_hardware_actions is False
    assert cfg.safety.require_confirm_token is True
    assert "docs/evidence" in cfg.safety.evidence_dir
    assert "方案评审" in cfg.safety.formal_review_dir


if __name__ == "__main__":
    # 运行所有 test_* 函数
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
    print(f"\n{'-'*40}")
    print(f"{len([k for k in globals() if k.startswith('test_') and callable(globals()[k])]) - failures}/{len([k for k in globals() if k.startswith('test_') and callable(globals()[k])])} 通过")
    sys.exit(failures)
