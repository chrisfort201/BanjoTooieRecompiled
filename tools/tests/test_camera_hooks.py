"""Source/build-artifact consistency checks for camera hooks (no ROM needed).

Run with: python tools/tests/test_camera_hooks.py
"""
from pathlib import Path
import tomllib

root = Path(__file__).resolve().parents[2]
config = tomllib.loads((root / "banjotooie.us.toml").read_text())
hooks = config["patches"]["hook"]
expected = [
    ("func_801111D0", 0x801111D0, "tooie_camera_hook_invert(rdram, ctx);", "funcs_37.c"),
    ("func_801111D0", 0x80111294, "tooie_camera_hook_rate(rdram, ctx);", "funcs_37.c"),
    ("func_801111D0", 0x8011129C, "tooie_camera_hook_rate_complete(rdram, ctx);", "funcs_37.c"),
    ("func_80112A84", 0x80112A98, "tooie_camera_hook_tick(rdram, ctx, 0);", "funcs_38.c"),
    ("func_80112A84", 0x80112C28, "tooie_camera_hook_tick(rdram, ctx, 1);", "funcs_38.c"),
]
for fn, address, call, filename in expected:
    found = [h for h in hooks if h["func"] == fn and h["before_vram"] == address
             and h["text"] == call]
    assert len(found) == 1, (fn, hex(address), call, found)
    data = (root / "RecompiledFuncs" / filename).read_text()
    function_body = data.split(f"RECOMP_FUNC void {fn}(", 1)[1].split("\nRECOMP_FUNC void ", 1)[0]
    assert function_body.count(call) == 1, (fn, hex(address), call)
    original = f"// 0x{address:08X}:"
    assert original in function_body, (fn, original)
    # Hook injected immediately ahead of the *instruction*, or at entry.
    assert function_body.index(call) < function_body.index(original)
    intervening = function_body[function_body.index(call) + len(call):function_body.index(original)]
    assert not intervening.strip(), (fn, hex(address), intervening)

c = (root / "RecompiledFuncs/funcs_37.c").read_text()
assert "// Optional camera-only inversion; never alter the C-button inputs." not in c
assert "// Optional manual yaw rotation-rate multiplier (original = 45.0f)." not in c
assert c.count("func_801111D0(rdram, ctx);") == 2
print("PASS: 5 native TOML hooks match committed generated C positions")
