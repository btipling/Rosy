#include "scene.h"

#include "imgui.h"


void scene_selector::draw_ui()
{
	ImGui::Begin("Scene Information");
	ImGui::Text("Select scene");
	const char* items[] = { "Dev Scene", "Scene Graph" };
	const int current_index = selected_scene;
	if (ImGui::ListBox("##listbox", &selected_scene, items, IM_ARRAYSIZE(items), 4))
	{
		if (!updated)
		{
			updated = current_index != selected_scene;
		}
	}
	ImGui::End();
}
