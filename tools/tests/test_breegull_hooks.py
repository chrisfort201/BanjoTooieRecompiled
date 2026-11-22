"""Verify regeneration-safe TOML hook declarations and committed C call sites.
Run with: py tools/tests/test_breegull_hooks.py
"""
from pathlib import Path
import tomllib
root = Path(__file__).resolve().parents[2]
hooks = tomllib.loads((root / "banjotooie.us.toml").read_text())["patches"]["hook"]
expected = [
    ("bsfirstp_entrypoint_1", 0x808000E4, "tooie_breegull_hook_player(rdram, ctx);", "funcs_44.c"),
    ("func_8001608C", 0x800160C4, "tooie_breegull_hook_pad(rdram, ctx);", "funcs_0.c"),
    ("func_8001608C", 0x800160E8, "tooie_breegull_hook_consumed(rdram, ctx);", "funcs_0.c"),
    ("func_800165F0", 0x8001661C, "tooie_breegull_hook_produced(rdram, ctx);", "funcs_0.c"),
    ("func_8080682C_bsfirstp", 0x80806884, "tooie_breegull_hook_camera_trace(rdram, ctx, 0);", "funcs_45.c"),
    ("func_8080682C_bsfirstp", 0x808068D8, "tooie_breegull_hook_camera_trace(rdram, ctx, 1);", "funcs_45.c"),
    ("func_808067B0_bsfirstp", 0x808067CC, "tooie_breegull_hook_camera_trace(rdram, ctx, 2);", "funcs_44.c"),
    ("func_808067B0_bsfirstp", 0x808067D8, "tooie_breegull_hook_camera_trace(rdram, ctx, 3);", "funcs_44.c"),
    ("func_808069BC_bsfirstp", 0x808069F0, "tooie_breegull_hook_pitch_delta(rdram, ctx, 0);", "funcs_45.c"),
    ("func_808069FC_bsfirstp", 0x80806AC0, "tooie_breegull_hook_pitch_delta(rdram, ctx, 1);", "funcs_45.c"),
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
print("PASS: 10 Breegull native TOML hooks (4 input + 4 camera trace + 2 pitch) match generated C positions")
