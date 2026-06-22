from pathlib import Path

migration_path = Path("Tools/Migration/add_render_graph_pass_culling.py")
source = migration_path.read_text(encoding="utf-8-sig")

old = '''def replace_once(text: str, old: str, new: str, label: str) -> str:
    count = text.count(old)
    if count != 1:
        raise RuntimeError(f"{label}: expected one match, found {count}")
    return text.replace(old, new, 1)
'''

new = '''def replace_once(text: str, old: str, new: str, label: str) -> str:
    count = text.count(old)
    if label == "Compile pass liveness reset" and count == 2:
        return text.replace(old, new, 1)
    if count != 1:
        raise RuntimeError(f"{label}: expected one match, found {count}")
    return text.replace(old, new, 1)
'''

if source.count(old) != 1:
    raise RuntimeError("replace_once helper definition mismatch")

source = source.replace(old, new, 1)
exec(compile(source, str(migration_path), "exec"), {"__name__": "__main__"})
