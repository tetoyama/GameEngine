#pragma once
#include "Service/IService.h"
#include <vector>
#include "InterFace/IEditorUI.h"

struct EditorDrawContext;
class EditorService;
class Manubar;

class EditorService : public IService {

public:

	void Initialize();
	void Draw(EditorDrawContext ctx);
	void Shutdown() override;

	Manubar* GetManubar();

	std::vector<IEditorUI*> UIs;
};

struct EditorDrawContext {
	EditorService* editor;
	double UpdateTime;
	double DrawTime;
	double FPS;
	double DeltaFPS;
};
