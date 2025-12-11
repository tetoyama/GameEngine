#pragma once
struct EditorDrawContext;
class IEditorUI {
public:
	virtual void Initialize() = 0;
	virtual void Draw(EditorDrawContext ctx) = 0;
	virtual void Finalize() = 0;
};
