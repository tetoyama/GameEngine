#pragma once
class EditorService;
struct EditorDrawContext;
class IEditorUI {
public:
	IEditorUI() {}
	virtual ~IEditorUI() = default;

	virtual void Initialize(EditorService* editor) = 0;
	virtual void Draw(const EditorDrawContext ctx) = 0;
	virtual void Finalize() = 0;
};
