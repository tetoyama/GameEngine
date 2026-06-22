from pathlib import Path

workflow_path = Path(".github/workflows/d3d11-rhi-real-triangle.yml")
if not workflow_path.exists():
    raise RuntimeError("generated D3D11 triangle workflow is missing")
workflow_path.unlink()
print("D3D11 triangle workflow excluded from generated patch")
