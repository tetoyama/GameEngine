from __future__ import annotations

from pathlib import Path


BASE_SCRIPT = Path("Tools/Migration/add_render_graph_queue_sync.py")
source = BASE_SCRIPT.read_text(encoding="utf-8-sig")

original_replace_once = '''def replace_once(text: str, old: str, new: str, label: str) -> str:
    count = text.count(old)
    if count != 1:
        raise RuntimeError(f"{label}: expected one match, found {count}")
    return text.replace(old, new, 1)
'''

hardened_replace_once = '''def replace_once(text: str, old: str, new: str, label: str) -> str:
    count = text.count(old)

    # Compile() and Reset() originally shared the same three-line clear block.
    # The compile block appears first, so consume that occurrence here. The
    # later Reset migration then sees the remaining single occurrence.
    if label == "Compile queue sync reset" and count == 2:
        return text.replace(old, new, 1)

    if count != 1:
        raise RuntimeError(f"{label}: expected one match, found {count}")
    return text.replace(old, new, 1)
'''

count = source.count(original_replace_once)
if count != 1:
    raise RuntimeError(
        f"replace_once definition: expected one match, found {count}"
    )
source = source.replace(original_replace_once, hardened_replace_once, 1)

exec(
    compile(source, str(BASE_SCRIPT), "exec"),
    {"__name__": "__main__", "__file__": str(BASE_SCRIPT)},
)
