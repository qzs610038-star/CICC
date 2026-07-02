"""
fpga_robot_mcp.config — 配置加载、路径解析、默认值。

提供 FpgaRobotConfig 数据结构和 load_config() 函数。

加载优先级：
1. FPGA_ROBOT_MCP_CONFIG 环境变量指向的 JSON 文件
2. 包内默认路径 configs/fpga_robot.local.json
3. 硬编码默认值（get_default_config()）
"""

import json
import os
import sys
import dataclasses
from dataclasses import dataclass, field, MISSING
from pathlib import Path
from typing import Any, Optional

# ── 子配置 ──────────────────────────────────────────────────


@dataclass
class EfinityConfig:
    home: str = "D:/Efinity/2025.2"
    bin: str = "D:/Efinity/2025.2/bin"
    project_xml: str = "赛方提供材料/TJ375N529_SC431HAI2LCD_Demo_V3/mem_test.xml"
    constrain_sdc: str = "赛方提供材料/TJ375N529_SC431HAI2LCD_Demo_V3/constrain.sdc"
    device: str = "TJ375N529"
    timing_model: str = "I3"
    patch_dir: str = "赛方提供材料/efinity-2025.2.288.4.15-windows-x64-patch"
    pgm_fli: str = "D:/Efinity/2025.2/pgm/fli"
    toolchain_prefix: str = "riscv-none-embed-"


@dataclass
class RiscvIdeConfig:
    home: str = "D:/Efinity/efinity-riscv-ide-2025.2"
    toolchain_bin: str = ""
    openocd: str = ""
    build_tools: str = ""


@dataclass
class BoardConfig:
    name: str = "WZZY_FPGA / TJ375N529"
    cpu_route: str = "board_cpu_qcrv32"
    uart_candidates: tuple[str, ...] = (
        "JTAG-IF UART",
        "Type-C UART",
        "UART2 TO Peripherals",
    )
    gpio_notes: str = (
        "J13/J15 are 3.3V GPIO candidates; "
        "do not connect to myCobot before voltage/protocol confirmation."
    )


@dataclass
class MycobotConfig:
    baudrate: int = 1000000
    preferred_vid: str = "10C4"
    model: str = "MyCobot"
    allow_motion: bool = False
    max_speed: int = 30
    default_port: Optional[str] = None


@dataclass
class SafetyConfig:
    allow_hardware_actions: bool = False
    require_confirm_token: bool = True
    evidence_dir: str = "final_project/docs/evidence"
    formal_review_dir: str = "方案评审"


@dataclass
class FpgaRobotConfig:
    """顶层 MCP 配置，聚合所有子配置域。"""

    project_root: str = ""
    efinity: EfinityConfig = field(default_factory=EfinityConfig)
    riscv_ide: RiscvIdeConfig = field(default_factory=RiscvIdeConfig)
    board: BoardConfig = field(default_factory=BoardConfig)
    mycobot280: MycobotConfig = field(default_factory=MycobotConfig)
    safety: SafetyConfig = field(default_factory=SafetyConfig)

    # ── 路径工具 ────────────────────────────────────────────

    def resolve(self, *parts: str) -> Path:
        """
        相对 project_root 解析路径；如果已经是绝对路径则直接返回。
        >>> cfg.resolve("赛方提供材料", "mem_test.xml")
        # → Path("D:/第十届集创赛-雄芯院材料/赛方提供材料/mem_test.xml")
        """
        joined = os.path.join(*parts) if parts else ""
        p = Path(joined)
        if p.is_absolute():
            return p
        base = Path(self.project_root) if self.project_root else Path.cwd()
        return base / p

    def efinity_home_path(self) -> Path:
        return Path(self.efinity.home)

    def efinity_bin_path(self) -> Path:
        return Path(self.efinity.bin)

    def efinity_project_path(self) -> Path:
        return self.resolve(self.efinity.project_xml)

    def constrain_sdc_path(self) -> Path:
        return self.resolve(self.efinity.constrain_sdc)

    def evidence_path(self) -> Path:
        return self.resolve(self.safety.evidence_dir)

    def formal_review_path(self) -> Path:
        return self.resolve(self.safety.formal_review_dir)


# ── 加载与默认值 ────────────────────────────────────────────


def get_default_config() -> FpgaRobotConfig:
    """返回硬编码默认配置（对应本机 Phase -1 安装后的实际路径）。"""
    return FpgaRobotConfig(
        project_root="D:/第十届集创赛-雄芯院材料",
    )


def _project_root_from_cwd() -> str:
    """尝试从当前目录推断 project_root。"""
    cwd = Path.cwd()
    # 如果当前目录包含 "赛方提供材料" 或 "final_project"，向上找
    for marker in ["赛方提供材料", "final_project", ".git"]:
        parts = list(cwd.rglob(marker))
        if parts:
            return str(parts[0].parent) if marker != ".git" else str(parts[0].parent)
    return str(cwd)


def _dict_to_dataclass(d: dict, cls: type, parent_cfg: FpgaRobotConfig | None = None):
    """递归将嵌套字典转换为对应 dataclass 实例。"""
    if not isinstance(d, dict):
        return d
    # 获取构造函数的参数名
    field_names = {f.name for f in dataclasses.fields(cls)}
    kwargs: dict[str, Any] = {}

    for key, value in d.items():
        if key not in field_names:
            continue
        sub_field = next((f for f in dataclasses.fields(cls) if f.name == key), None)
        if sub_field is None:
            kwargs[key] = value
            continue
        sub_type = sub_field.type
        # 如果是嵌套 dataclass 且 value 是 dict，递归转换
        if isinstance(value, dict) and isinstance(sub_type, type) and hasattr(sub_type, "__dataclass_fields__"):
            kwargs[key] = _dict_to_dataclass(value, sub_type, parent_cfg)
        elif isinstance(value, list) and sub_field.name == "uart_candidates":
            kwargs[key] = tuple(value)
        else:
            kwargs[key] = value

    # 补全缺失字段的默认值
    for f in dataclasses.fields(cls):
        if f.name not in kwargs and f.default is not MISSING:
            kwargs[f.name] = f.default
        elif f.name not in kwargs and f.default_factory is not MISSING:
            kwargs[f.name] = f.default_factory()

    return cls(**kwargs)


def load_config(path: str | None = None) -> FpgaRobotConfig:
    """
    加载 MCP 配置，优先级：
    1. path 参数
    2. FPGA_ROBOT_MCP_CONFIG 环境变量
    3. 包内默认路径
    4. hard-coded 默认值
    """
    # 确定配置来源
    config_path: str | None = path

    if config_path is None:
        config_path = os.environ.get("FPGA_ROBOT_MCP_CONFIG")

    if config_path is None:
        # 搜索包内默认路径
        default_candidates = [
            Path(__file__).resolve().parent.parent.parent
            / "configs"
            / "fpga_robot.local.json",
            Path.cwd() / "configs" / "fpga_robot.local.json",
        ]
        for cand in default_candidates:
            if cand.exists():
                config_path = str(cand)
                break

    # 无可用配置文件 → 返回默认
    if not config_path or not os.path.exists(config_path):
        cfg = get_default_config()
        if not cfg.project_root:
            cfg.project_root = _project_root_from_cwd()
        return cfg

    # 加载 JSON
    try:
        with open(config_path, "r", encoding="utf-8") as f:
            raw = json.load(f)
    except (json.JSONDecodeError, OSError) as e:
        print(f"[config] 配置文件 {config_path} 加载失败: {e}", file=sys.stderr)
        return get_default_config()

    # 递归构建 dataclass
    cfg = _dict_to_dataclass(raw, FpgaRobotConfig)
    if not cfg.project_root:
        # fallback: 从配置文件位置推断
        cfg_path = Path(config_path)
        # 尝试找到项目根（包含 "赛方提供材料" 的父目录）
        for parent in cfg_path.parents:
            if (parent / "赛方提供材料").exists():
                cfg.project_root = str(parent)
                break
        if not cfg.project_root:
            cfg.project_root = _project_root_from_cwd()

    return cfg


# ── 敏感字段隐藏 ────────────────────────────────────────────

_HIDDEN_FIELDS = {"default_port"}

SENSITIVE_KEYS = {"token", "password", "secret", "key", "confirm_token"}


def hide_sensitive(cfg: FpgaRobotConfig) -> dict:
    """返回可安全显示的配置字典（隐藏敏感字段）。"""
    MISSING = dataclasses.MISSING

    def _hide(obj):
        if isinstance(obj, (str, int, float, bool, type(None))):
            return obj
        if isinstance(obj, (list, tuple)):
            return [_hide(i) for i in obj]
        if isinstance(obj, dict):
            return {k: ("****" if k.lower() in SENSITIVE_KEYS else _hide(v)) for k, v in obj.items()}
        if hasattr(obj, "__dataclass_fields__"):
            result = {}
            for f in dataclasses.fields(obj):
                val = getattr(obj, f.name)
                if f.name in _HIDDEN_FIELDS:
                    result[f.name] = "****"
                elif f.name.lower() in SENSITIVE_KEYS:
                    result[f.name] = "****"
                else:
                    result[f.name] = _hide(val)
            return result
        return str(obj)

    return _hide(cfg)


# ── 快速测试 ────────────────────────────────────────────────


if __name__ == "__main__":
    cfg = load_config()
    print("=== 配置加载结果 ===")
    print(f"  项目根: {cfg.project_root}")
    print(f"  Efinity: {cfg.efinity.home}")
    print(f"  工程 XML: {cfg.efinity_project_path()}")
    print(f"  证据目录: {cfg.evidence_path()}")
    print(f"  myCobot 允许运动: {cfg.mycobot280.allow_motion}")
    print(f"  safety 硬件动作: {cfg.safety.allow_hardware_actions}")
    print()
    print("=== 隐藏敏感字段后 ===")
    import json as _json
    print(_json.dumps(hide_sensitive(cfg), ensure_ascii=False, indent=2))
