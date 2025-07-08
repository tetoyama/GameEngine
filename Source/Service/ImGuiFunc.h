#pragma once

class Vector3;

namespace ImGui{
	void DragVec3(const char* label, Vector3& Vec3, bool readOnly = false);
}