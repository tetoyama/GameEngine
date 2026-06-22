from __future__ import annotations

from pathlib import Path


BASE_SCRIPT = Path("Tools/Migration/add_render_graph_queue_sync.py")
source = BASE_SCRIPT.read_text(encoding="utf-8-sig")

compile_old = r'''text = replace_once(
    text,
    """\t\tm_executionOrder.clear();
\t\tm_resourceLifetimes.clear();
\t\tm_finalStates.clear();
""",
    """\t\tm_executionOrder.clear();
\t\tm_resourceLifetimes.clear();
\t\tm_queueSync.clear();
\t\tm_finalStates.clear();
""",
    "Compile queue sync reset",
)
'''
compile_new = r'''text = replace_once(
    text,
    """\t\tm_executionOrder.clear();
\t\tm_resourceLifetimes.clear();
\t\tm_finalStates.clear();
\t\tm_error.clear();
""",
    """\t\tm_executionOrder.clear();
\t\tm_resourceLifetimes.clear();
\t\tm_queueSync.clear();
\t\tm_finalStates.clear();
\t\tm_error.clear();
""",
    "Compile queue sync reset",
)
'''

reset_old = r'''text = replace_once(
    text,
    """\t\tm_executionOrder.clear();
\t\tm_resourceLifetimes.clear();
\t\tm_finalStates.clear();
""",
    """\t\tm_executionOrder.clear();
\t\tm_resourceLifetimes.clear();
\t\tm_queueSync.clear();
\t\tm_finalStates.clear();
""",
    "Reset queue sync",
)
'''
reset_new = r'''text = replace_once(
    text,
    """\t\tm_resources.clear();
\t\tm_passes.clear();
\t\tm_executionOrder.clear();
\t\tm_resourceLifetimes.clear();
\t\tm_finalStates.clear();
""",
    """\t\tm_resources.clear();
\t\tm_passes.clear();
\t\tm_executionOrder.clear();
\t\tm_resourceLifetimes.clear();
\t\tm_queueSync.clear();
\t\tm_finalStates.clear();
""",
    "Reset queue sync",
)
'''

for old, new, label in (
    (compile_old, compile_new, "Compile queue sync replacement"),
    (reset_old, reset_new, "Reset queue sync replacement"),
):
    count = source.count(old)
    if count != 1:
        raise RuntimeError(f"{label}: expected one match, found {count}")
    source = source.replace(old, new, 1)

exec(
    compile(source, str(BASE_SCRIPT), "exec"),
    {"__name__": "__main__", "__file__": str(BASE_SCRIPT)},
)
