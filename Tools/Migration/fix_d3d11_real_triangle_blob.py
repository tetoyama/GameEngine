from pathlib import Path

path = Path("Tests/D3D11RHIRealTriangleTest.cpp")
text = path.read_text(encoding="utf-8-sig")

old = '''std::span<const std::byte> BlobBytes(const ID3DBlob& blob){
\treturn {
\t\treinterpret_cast<const std::byte*>(blob.GetBufferPointer()),
\t\tblob.GetBufferSize()
\t};
}
'''
new = '''std::span<const std::byte> BlobBytes(ID3DBlob* blob){
\tassert(blob);
\treturn {
\t\treinterpret_cast<const std::byte*>(blob->GetBufferPointer()),
\t\tblob->GetBufferSize()
\t};
}
'''
if text.count(old) != 1:
    raise RuntimeError("BlobBytes definition mismatch")
text = text.replace(old, new, 1)

replacements = {
    "BlobBytes(*vertexByteCode)": "BlobBytes(vertexByteCode.Get())",
    "BlobBytes(*pixelByteCode)": "BlobBytes(pixelByteCode.Get())",
}
for old_call, new_call in replacements.items():
    if text.count(old_call) != 1:
        raise RuntimeError(f"shader blob call mismatch: {old_call}")
    text = text.replace(old_call, new_call, 1)

path.write_text(text, encoding="utf-8", newline="\n")
print("D3D11 triangle shader blob access fixed")
