"""Verify regeneration-safe TOML hook declarations and committed C call sites.
Run with: py tools/tests/test_breegull_hooks.py
"""
from pathlib import Path
import tomllib
root = Path(__file__).resolve().parents[2]
hooks = tomllib.loads((root / "banjotooie.us.toml").read_text())["patches"]["hook"]
expected = [
    ("bsfirstp_entrypoint_1", 0x808000E4, "tooie_breegull_hook_player(rdram, ctx);", "funcs_44.c"),
    ("func_800165F0", 0x8001661C, "tooie_breegull_hook_pad(rdram, ctx);", "funcs_0.c"),
]
for name, address, call, filename in expected:
    assert sum(h.get("func") == name and h.get("before_vram") == address
               and h.get("text") == call for h in hooks) == 1, (name, hex(address))
    text = (root / "RecompiledFuncs" / filename).read_text()
    body = text.split(f"RECOMP_FUNC void {name}(", 1)[1].split("\nRECOMP_FUNC void ", 1)[0]
    assert body.count(call) == 1
    addr_comment = f"// 0x{address:08X}:"
    assert body.index(call) < body.index(addr_comment)
    assert not body[body.index(call) + len(call):body.index(addr_comment)].strip()
print("PASS: 2 Breegull native TOML hooks match committed generated C positions")
