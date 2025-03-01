#include "DebugUI.h"

#include <numbers>
#include <format>

#include "imgui.h"

using namespace rosy;

namespace
{
    constexpr double pi{std::numbers::pi};
    constexpr auto button_dims = ImVec2(200.f, 35.f);
}


void debug_ui::graphics_debug_ui(const engine_stats& eng_stats, const graphics_stats& stats, const graphics_data& data,
                                 const read_level_state* rls) const
{
    if (ImGui::BeginTabItem("Graphics"))
    {
        wls->enable_edit = true;

        if (ImGui::CollapsingHeader("Scene Data"))
        {
            if (ImGui::BeginTable("##SceneDataTable", 2,
                                  ImGuiTableFlags_NoSavedSettings | ImGuiTableFlags_Borders))
            {
                ImGui::TableNextRow();
                ImGui::TableNextColumn();
                ImGui::Text("Camera position");
                ImGui::TableNextColumn();
                ImGui::Text("(%.2f,  %.2f,  %.2f)", data.camera_position[0],
                            data.camera_position[1], data.camera_position[2]);
                ImGui::TableNextRow();
                ImGui::TableNextColumn();
                ImGui::Text("Camera Orientation");
                ImGui::TableNextColumn();
                ImGui::Text("Pitch: %.2f Yaw: %.2f)", rls->cam.pitch, rls->cam.yaw);
                ImGui::TableNextRow();
                ImGui::TableNextColumn();
                ImGui::Text("Light direction");
                ImGui::TableNextColumn();
                ImGui::Text("(%.2f,  %.2f,  %.2f)", data.sunlight[0], data.sunlight[1],
                            data.sunlight[2]);
                ImGui::EndTable();
            }
        }
        if (ImGui::CollapsingHeader("Performance"))
        {
            if (ImGui::BeginTable("##PerformanceOptions", 1,
                                  ImGuiTableFlags_NoSavedSettings | ImGuiTableFlags_Borders))
            {
                ImGui::TableNextRow();
                ImGui::TableNextColumn();
                ImGui::SliderFloat("Target fps", &wls->target_fps, 30.f, 960.f, "%.1f", 0);


                ImGui::EndTable();
            }
            if (ImGui::BeginTable("##PerformanceData", 2,
                                  ImGuiTableFlags_NoSavedSettings | ImGuiTableFlags_Borders))
            {
                ImGui::TableNextRow();
                ImGui::TableNextColumn();
                ImGui::Text("a_fps");
                ImGui::TableNextColumn();
                ImGui::Text("%.0f rad/s", eng_stats.a_fps);

                ImGui::TableNextRow();
                ImGui::TableNextColumn();
                ImGui::Text("d_fps");
                ImGui::TableNextColumn();
                ImGui::Text("%.0f deg/s", eng_stats.d_fps);

                ImGui::TableNextRow();
                ImGui::TableNextColumn();
                ImGui::Text("r_fps");
                ImGui::TableNextColumn();
                ImGui::Text("%.0f", eng_stats.r_fps);

                ImGui::TableNextRow();
                ImGui::TableNextColumn();
                ImGui::Text("frame time");
                ImGui::TableNextColumn();
                ImGui::Text("%.3fms", eng_stats.frame_time);

                ImGui::TableNextRow();
                ImGui::TableNextColumn();
                ImGui::Text("update time");
                ImGui::TableNextColumn();
                ImGui::Text("%.3f ms", eng_stats.level_update_time);

                ImGui::TableNextRow();
                ImGui::TableNextColumn();
                ImGui::Text("draw time");
                ImGui::TableNextColumn();
                ImGui::Text("%.3f ms", stats.draw_time);

                ImGui::TableNextRow();
                ImGui::TableNextColumn();
                ImGui::Text("triangles");
                ImGui::TableNextColumn();
                ImGui::Text("%i", stats.triangle_count);

                ImGui::TableNextRow();
                ImGui::TableNextColumn();
                ImGui::Text("lines");
                ImGui::TableNextColumn();
                ImGui::Text("%i", stats.line_count);

                ImGui::TableNextRow();
                ImGui::TableNextColumn();
                ImGui::Text("draws ");
                ImGui::TableNextColumn();
                ImGui::Text("%i", stats.draw_call_count);

                ImGui::EndTable();
            }
        }
        if (ImGui::CollapsingHeader("Lighting"))
        {
            if (ImGui::BeginTable("Edit Scene Data", 1, ImGuiTableFlags_NoSavedSettings | ImGuiTableFlags_Borders))
            {
                ImGui::TableNextRow();
                ImGui::TableNextColumn();
                ImGui::SliderFloat("Spherical distance", &wls->light_debug.sun_distance, 0.f, 25.f);

                ImGui::TableNextRow();
                ImGui::TableNextColumn();
                ImGui::SliderFloat("Spherical pitch", &wls->light_debug.sun_pitch, 0.f, 4 * static_cast<float>(pi));

                ImGui::TableNextRow();
                ImGui::TableNextColumn();
                ImGui::SliderFloat("Spherical yaw", &wls->light_debug.sun_yaw, 0.f, 4 * static_cast<float>(pi));

                ImGui::TableNextRow();
                ImGui::TableNextColumn();
                ImGui::SliderFloat("Light depth", &wls->light_debug.orthographic_depth, 0.f, 500.f);

                ImGui::TableNextRow();
                ImGui::TableNextColumn();
                ImGui::SliderFloat("Light cascade level", &wls->light_debug.cascade_level, 0.f, 50.f);

                ImGui::TableNextRow();
                ImGui::TableNextColumn();
                ImGui::SliderFloat("Depth bias constant", &wls->light.depth_bias_constant, -500.f, 500.f);

                ImGui::TableNextRow();
                ImGui::TableNextColumn();
                ImGui::SliderFloat("Depth bias clamp", &wls->light.depth_bias_clamp, -500.f, 500.f);

                ImGui::TableNextRow();
                ImGui::TableNextColumn();
                ImGui::SliderFloat("Depth bias slope factor", &wls->light.depth_bias_slope_factor,  -50.f, 50.f);

                ImGui::TableNextRow();
                ImGui::TableNextColumn();
                ImGui::SliderFloat("Ambient light", &wls->light.ambient_light, 0.f, 1.f);

                ImGui::TableNextRow();
                ImGui::TableNextColumn();
                ImGui::ColorEdit4("Sunlight color", wls->light.sunlight_color.data(), 0);

                ImGui::EndTable();
            }
            if (ImGui::BeginTable("##ToggleOptions", 2, ImGuiTableFlags_NoSavedSettings | ImGuiTableFlags_Borders))
            {
                ImGui::TableNextRow();
                ImGui::TableNextColumn();
                ImGui::Checkbox("Enable light camera", &wls->light_debug.enable_light_cam);
                ImGui::TableNextColumn();
                ImGui::Checkbox("Enable light perspective", &wls->light_debug.enable_light_perspective);

                ImGui::TableNextRow();
                ImGui::TableNextColumn();
                ImGui::Checkbox("Enable sun debug", &wls->light_debug.enable_sun_debug);
                ImGui::TableNextColumn();
                ImGui::Checkbox("Enable depth bias", &wls->light.depth_bias_enabled);

                ImGui::TableNextRow();
                ImGui::TableNextColumn();
                ImGui::Checkbox("Flip light x", &wls->light.flip_light_x);
                ImGui::TableNextColumn();
                ImGui::Checkbox("Flip light y", &wls->light.flip_light_y);

                ImGui::TableNextRow();
                ImGui::TableNextColumn();
                ImGui::Checkbox("Flip light z", &wls->light.flip_light_z);
                ImGui::TableNextColumn();
                ImGui::Text("");

                ImGui::TableNextRow();
                ImGui::TableNextColumn();
                ImGui::Checkbox("Flip tangent x", &wls->light.flip_tangent_x);
                ImGui::TableNextColumn();
                ImGui::Checkbox("Flip tangent y", &wls->light.flip_tangent_y);

                ImGui::TableNextRow();
                ImGui::TableNextColumn();
                ImGui::Checkbox("Flip tangent z", &wls->light.flip_tangent_z);
                ImGui::TableNextColumn();
                ImGui::Checkbox("Flip tangent w", &wls->light.flip_tangent_w);

                ImGui::TableNextRow();
                ImGui::TableNextColumn();
                ImGui::Checkbox("Inverse BNT", &wls->light.inverse_bnt);
                ImGui::TableNextColumn();
                ImGui::Text("");

                ImGui::EndTable();
            }
        }
        if (ImGui::CollapsingHeader("Shadow map"))
        {
            const ImVec4 border_col = ImGui::GetStyleColorVec4(ImGuiCol_Border);
            constexpr auto uv_min = ImVec2(0.0f, 0.0f);
            constexpr auto uv_max = ImVec2(1.0f, 1.0f);
            constexpr auto tint_col = ImVec4(1.0f, 1.0f, 1.0f, 1.0f);
            const ImVec2 content_area = ImGui::GetContentRegionAvail();
            ImGui::Image(data.shadow_map_img_id, content_area,
                         uv_min, uv_max, tint_col, border_col);
        }
        if (ImGui::CollapsingHeader("Draw config"))
        {
            if (ImGui::BeginTable("##ToggleOptions", 2,
                                  ImGuiTableFlags_NoSavedSettings | ImGuiTableFlags_Borders))
            {
                ImGui::TableNextRow();
                ImGui::TableNextColumn();
                ImGui::Checkbox("Enable wireframe", &wls->draw_config.wire_enabled);
                ImGui::TableNextColumn();
                ImGui::Checkbox("Thick lines", &wls->draw_config.thick_wire_lines);

                ImGui::TableNextRow();
                ImGui::TableNextColumn();
                ImGui::Checkbox("Toggle winding order",
                                &wls->draw_config.reverse_winding_order_enabled);
                ImGui::TableNextColumn();
                ImGui::Checkbox("Enable cull", &wls->draw_config.cull_enabled);

                ImGui::EndTable();
            }
        }
        if (ImGui::CollapsingHeader("Fragment Config"))
        {
            ImGui::RadioButton("default", &wls->fragment_config.output, 0);
            ImGui::SameLine();
            ImGui::RadioButton("normal", &wls->fragment_config.output, 1);
            ImGui::SameLine();
            ImGui::RadioButton("tangent", &wls->fragment_config.output, 2);
            ImGui::SameLine();
            ImGui::RadioButton("light", &wls->fragment_config.output, 3);
            ImGui::SameLine();
            ImGui::RadioButton("view", &wls->fragment_config.output, 4);
            ImGui::SameLine();
            ImGui::RadioButton("vertex colors", &wls->fragment_config.output, 5);
            if (ImGui::BeginTable("##ToggleFragmentOptions", 2,
                                  ImGuiTableFlags_NoSavedSettings | ImGuiTableFlags_Borders))
            {
                ImGui::TableNextRow();
                ImGui::TableNextColumn();
                ImGui::Checkbox("Enable light", &wls->fragment_config.light_enabled);
                ImGui::TableNextColumn();
                ImGui::Checkbox("Enable tangent space", &wls->fragment_config.tangent_space_enabled);

                ImGui::TableNextRow();
                ImGui::TableNextColumn();
                ImGui::Checkbox("Enable Shadows", &wls->fragment_config.shadows_enabled);
                ImGui::TableNextColumn();
                ImGui::Checkbox("Enable Normal maps", &wls->fragment_config.normal_maps_enabled);

                ImGui::EndTable();
            }
        }
        if (ImGui::CollapsingHeader("Mobs"))
        {
            if (ImGui::BeginTable("##Mob states", 2,
                                  ImGuiTableFlags_NoSavedSettings | ImGuiTableFlags_Borders))
            {
                for (const auto& mob_states : rls->mob_read.mob_states)
                {
                    ImGui::TableNextRow();
                    ImGui::TableNextColumn();
                    ImGui::Text("name");
                    ImGui::TableNextColumn();
                    ImGui::Text("%s", mob_states.name.c_str());

                    ImGui::TableNextRow();
                    ImGui::TableNextColumn();
                    ImGui::Text("position");
                    ImGui::TableNextColumn();
                    ImGui::Text("(%.2f,  %.2f,  %.2f)", mob_states.position[0], mob_states.position[1],
                                mob_states.position[2]);

                    ImGui::TableNextRow();
                    ImGui::TableNextColumn();
                    ImGui::Text("yaw");
                    ImGui::TableNextColumn();
                    ImGui::Text("(%.2f)", mob_states.yaw);

                    ImGui::TableNextRow();
                    ImGui::TableNextColumn();
                    ImGui::Text("target");
                    ImGui::TableNextColumn();
                    ImGui::Text("(%.2f,  %.2f,  %.2f)", mob_states.target[0], mob_states.target[1],
                                mob_states.target[2]);

                    ImGui::TableNextRow();
                    ImGui::TableNextColumn();
                    ImGui::Text("");
                    ImGui::TableNextColumn();
                    ImGui::Text("");
                }
                ImGui::EndTable();
            }

            if (ImGui::BeginCombo("Select mob",
                                  rls->mob_read.mob_states[wls->mob_edit.edit_index].name.c_str()))
            {
                for (size_t i = 0; i < rls->mob_read.mob_states.size(); ++i)
                {
                    const auto& mob_state = rls->mob_read.mob_states[i];
                    const bool is_selected = (wls->mob_edit.edit_index == i);
                    if (ImGui::Selectable(mob_state.name.c_str(), is_selected))
                    {
                        wls->mob_edit.edit_index = i;
                    }
                    if (is_selected)
                    {
                        ImGui::SetItemDefaultFocus();
                    }
                }
                ImGui::EndCombo();
            }
            if (!wls->mob_edit.updated)
                wls->mob_edit.position = rls->mob_read.mob_states[wls->mob_edit.
                                                                       edit_index].position;
            if (ImGui::InputFloat3("position", wls->mob_edit.position.data()))
                wls->mob_edit.updated =
                    true;
            if (ImGui::Button("Update", button_dims)) wls->mob_edit.submitted = true;
        }
        if (ImGui::CollapsingHeader("Picking"))
        {
            ImVec4 color;
            switch (rls->pick_debugging.space)
            {
            case pick_debug_read_state::picking_space::screen:
                color = ImVec4(0.f, 1.f, 0.f, 1.f);
                ImGui::TextColored(color, "Pick debugging in screen space");
                break;
            case pick_debug_read_state::picking_space::view:
                color = ImVec4(1.f, 1.f, 0.f, 1.f);
                ImGui::TextColored(color, "Pick debugging in view space");
                break;
            case pick_debug_read_state::picking_space::disabled:
                ImGui::Text("Pick debugging disabled");
                break;
            }
        }
        if (ImGui::CollapsingHeader("Game Camera"))
        {
            if (ImGui::BeginTable("##GameCameraOptions", 1,
                                  ImGuiTableFlags_NoSavedSettings | ImGuiTableFlags_Borders))
            {
                ImGui::TableNextRow();
                ImGui::TableNextColumn();
                ImGui::SliderFloat("Yaw", &wls->game_camera_yaw, 0, static_cast<float>(pi) * 2.f,
                                   "%.3f", 0);

                ImGui::EndTable();
            }
        }
        ImGui::EndTabItem();
    }
}

void debug_ui::assets_debug_ui([[maybe_unused]] const read_level_state* rls)
{
    if (ImGui::BeginTabItem("Assets"))
    {
        if (ImGui::Button("Save Level", button_dims))
        {
            const editor_command cmd_desc{
                .command_type = editor_command::editor_command_type::write_level,
            };
            wls->editor_commands.commands.push_back(cmd_desc);
        }
        ImGui::SameLine();
        if (ImGui::Button("Load Level", button_dims))
        {
            const editor_command cmd_desc{
                .command_type = editor_command::editor_command_type::read_level,
            };
            wls->editor_commands.commands.push_back(cmd_desc);
        }
        if (rls->editor_state.assets.size() > selected_asset)
        {
            if (ImGui::CollapsingHeader("Level Details", &asset_details))
            {
                if (ImGui::CollapsingHeader("Mobs", &asset_details))
                {
                    if (ImGui::BeginTable("##Mobs", 2, ImGuiTableFlags_NoSavedSettings | ImGuiTableFlags_Borders))
                    {
                        size_t index{1};
                        for (const auto& md : rls->editor_state.current_level_data.mob_models)
                        {
                            ImGui::TableNextRow();
                            ImGui::TableNextColumn();
                            ImGui::Text("");
                            ImGui::TableNextColumn();
                            ImGui::Text("");

                            ImGui::TableNextRow();
                            ImGui::TableNextColumn();
                            ImGui::Text("id");
                            ImGui::TableNextColumn();
                            ImGui::Text("%s", md.id.c_str());

                            ImGui::TableNextRow();
                            ImGui::TableNextColumn();
                            ImGui::Text("name");
                            ImGui::TableNextColumn();
                            ImGui::Text("%s", md.name.c_str());

                            ImGui::TableNextRow();
                            ImGui::TableNextColumn();
                            ImGui::Text("location");
                            ImGui::TableNextColumn();
                            ImGui::Text("(%.3f, %.3f, %.3f)", md.location[0], md.location[1], md.location[2]);

                            ImGui::TableNextRow();
                            ImGui::TableNextColumn();
                            ImGui::Text("scale");
                            ImGui::TableNextColumn();
                            ImGui::Text("%.3f", md.scale);

                            ImGui::TableNextRow();
                            ImGui::TableNextColumn();
                            ImGui::Text("yaw");
                            ImGui::TableNextColumn();
                            ImGui::Text("%.3f", md.yaw);

                            ImGui::TableNextRow();
                            ImGui::TableNextColumn();
                            if (ImGui::Button(std::format("#{} Edit {}", index, md.name).c_str(), button_dims))
                            {
                                ImGui::OpenPopup(std::format("{}:{}", md.name, index).c_str());
                            }
                            if (ImGui::BeginPopup(std::format("{}:{}", md.name, index).c_str()))
                            {
                                if (level_edit_model_id != md.id)
                                {
                                    level_edit_translate = md.location;
                                    level_edit_scale = md.scale;
                                    level_edit_yaw = md.yaw;
                                    level_edit_model_id = md.id;
                                    level_edit_model_type = md.model_type;
                                }
                                ImGui::Text("%s", std::format("Edit {} @ {}", md.name, index).c_str());
                                ImGui::Text("location");
                                ImGui::SameLine();
                                ImGui::InputFloat3(std::format("##edit_translate{}:{}", md.name, index).c_str(), level_edit_translate.data(), "%.3f");
                                ImGui::Text("scale");
                                ImGui::SameLine();
                                ImGui::InputFloat(std::format("##edit_scale{}:{}", md.name, index).c_str(), &level_edit_scale, 0.1f, 0.2f, "%.3f");
                                ImGui::Text("yaw");
                                ImGui::SameLine();
                                ImGui::InputFloat(std::format("##edit_yaw{}:{}", md.name, index).c_str(), &level_edit_yaw, 0.1f, 0.2f, "%.3f");
                                if (ImGui::Button(std::format("#{} Save {}", index, md.name).c_str(), button_dims))
                                {
                                    const editor_command cmd_desc{
                                        .command_type = editor_command::editor_command_type::edit_level_node,
                                        .mode_type_option = level_edit_model_type,
                                        .id = md.id,
                                        .node_data = {
                                            .location = level_edit_translate,
                                            .scale = level_edit_scale,
                                            .yaw = level_edit_yaw,
                                        },
                                    };
                                    wls->editor_commands.commands.push_back(cmd_desc);
                                }
                                ImGui::EndPopup();
                            }
                            ImGui::TableNextColumn();
                            ImGui::Text("");

                            index += 1;
                        }
                        ImGui::EndTable();
                    }
                }
                if (ImGui::CollapsingHeader("Static", &asset_details))
                {
                    if (ImGui::BeginTable("##Static", 2, ImGuiTableFlags_NoSavedSettings | ImGuiTableFlags_Borders))
                    {
                        size_t index{1};
                        for (const auto& md : rls->editor_state.current_level_data.static_models)
                        {
                            ImGui::TableNextRow();
                            ImGui::TableNextColumn();
                            ImGui::Text("");
                            ImGui::TableNextColumn();
                            ImGui::Text("");

                            ImGui::TableNextRow();
                            ImGui::TableNextColumn();
                            ImGui::Text("id");
                            ImGui::TableNextColumn();
                            ImGui::Text("%s", md.id.c_str());

                            ImGui::TableNextRow();
                            ImGui::TableNextColumn();
                            ImGui::Text("name");
                            ImGui::TableNextColumn();
                            ImGui::Text("%s", md.name.c_str());

                            ImGui::TableNextRow();
                            ImGui::TableNextColumn();
                            ImGui::Text("location");
                            ImGui::TableNextColumn();
                            ImGui::Text("(%.3f, %.3f, %.3f)", md.location[0], md.location[1], md.location[2]);

                            ImGui::TableNextRow();
                            ImGui::TableNextColumn();
                            ImGui::Text("scale");
                            ImGui::TableNextColumn();
                            ImGui::Text("%.3f", md.scale);

                            ImGui::TableNextRow();
                            ImGui::TableNextColumn();
                            ImGui::Text("yaw");
                            ImGui::TableNextColumn();
                            ImGui::Text("%.3f", md.yaw);

                            ImGui::TableNextRow();
                            ImGui::TableNextColumn();
                            if (ImGui::Button(std::format("#{} Edit {}", index, md.name).c_str(), button_dims))
                                ImGui::OpenPopup(std::format("{}:{}", md.name, index).c_str());
                            if (ImGui::BeginPopup(std::format("{}:{}", md.name, index).c_str()))
                            {
                                if (level_edit_model_id != md.id)
                                {
                                    level_edit_translate = md.location;
                                    level_edit_scale = md.scale;
                                    level_edit_yaw = md.yaw;
                                    level_edit_model_id = md.id;
                                    level_edit_model_type = md.model_type;
                                }
                                ImGui::Text("%s", std::format("Edit {} @ {}", md.name, index).c_str());
                                ImGui::Text("location");
                                ImGui::SameLine();
                                ImGui::InputFloat3(std::format("##edit_translate{}:{}", md.name, index).c_str(), level_edit_translate.data(), "%.3f");
                                ImGui::Text("scale");
                                ImGui::SameLine();
                                ImGui::InputFloat(std::format("##edit_scale{}:{}", md.name, index).c_str(), &level_edit_scale, 0.1f, 0.2f, "%.3f");
                                ImGui::Text("yaw");
                                ImGui::SameLine();
                                ImGui::InputFloat(std::format("##edit_yaw{}:{}", md.name, index).c_str(), &level_edit_yaw, 0.1f, 0.2f, "%.3f");
                                if (ImGui::Button(std::format("#{} Save {}", index, md.name).c_str(), button_dims))
                                {
                                    const editor_command cmd_desc{
                                        .command_type = editor_command::editor_command_type::edit_level_node,
                                        .mode_type_option = level_edit_model_type,
                                        .id = md.id,
                                        .node_data = {
                                            .location = level_edit_translate,
                                            .scale = level_edit_scale,
                                            .yaw = level_edit_yaw,
                                        },
                                    };
                                    wls->editor_commands.commands.push_back(cmd_desc);
                                }
                                ImGui::EndPopup();
                            }
                            ImGui::TableNextColumn();
                            ImGui::Text("");

                            index += 1;
                        }
                        ImGui::EndTable();
                    }
                }
            }
            if (ImGui::BeginListBox("##AssetsList"))
            {
                size_t index{0};
                for (const asset_description& a : rls->editor_state.assets)
                {
                    if (ImGui::Selectable(std::format("##{}", a.id).c_str(), selected_asset == index))
                    {
                        selected_asset = index;
                        selected_model = 0;
                    }
                    ImGui::SameLine();
                    ImGui::Text("%s", a.name.c_str());
                    index += 1;
                }
                ImGui::EndListBox();
            }
            if (ImGui::CollapsingHeader("Asset Details", &asset_details))
            {
                if (ImGui::BeginTable("##AssetDetails", 2, ImGuiTableFlags_NoSavedSettings | ImGuiTableFlags_Borders))
                {
                    const asset_description& description = rls->editor_state.assets[selected_asset];
                    ImGui::TableNextRow();
                    ImGui::TableNextColumn();
                    ImGui::Text("id");
                    ImGui::TableNextColumn();
                    ImGui::Text("%s", description.id.c_str());

                    ImGui::TableNextRow();
                    ImGui::TableNextColumn();
                    ImGui::Text("name");
                    ImGui::TableNextColumn();
                    ImGui::Text("%s", description.name.c_str());

                    ImGui::TableNextRow();
                    ImGui::TableNextColumn();
                    ImGui::Text("model count");
                    ImGui::TableNextColumn();
                    ImGui::Text("%zu", description.models.size());

                    ImGui::TableNextRow();
                    ImGui::TableNextColumn();
                    ImGui::Text("Load asset");
                    ImGui::TableNextColumn();
                    if (ImGui::Button("Update", button_dims))
                    {
                        const editor_command cmd_desc{
                            .command_type = editor_command::editor_command_type::load_asset,
                            .id = description.id,
                        };
                        wls->editor_commands.commands.push_back(cmd_desc);
                    }

                    ImGui::EndTable();

                    if (ImGui::BeginListBox("##ModelsList"))
                    {
                        size_t index{0};
                        for (const model_description& m : description.models)
                        {
                            if (ImGui::Selectable(std::format("##{}", m.id).c_str(), selected_model == index))
                            {
                                selected_model = index;
                            }
                            ImGui::SameLine();
                            ImGui::Text("%s", m.name.c_str());
                            index += 1;
                        }
                        ImGui::EndListBox();
                    }
                    if (description.models.size() > selected_model)
                    {
                        if (ImGui::CollapsingHeader("Model Details", &model_details))
                        {
                            if (ImGui::BeginTable("##ModelDetails", 2,
                                                  ImGuiTableFlags_NoSavedSettings | ImGuiTableFlags_Borders))
                            {
                                const model_description& m = description.models[selected_model];
                                ImGui::TableNextRow();
                                ImGui::TableNextColumn();
                                ImGui::Text("id");
                                ImGui::TableNextColumn();
                                ImGui::Text("%s", m.id.c_str());

                                ImGui::TableNextRow();
                                ImGui::TableNextColumn();
                                ImGui::Text("name");
                                ImGui::TableNextColumn();
                                ImGui::Text("%s", m.name.c_str());

                                ImGui::TableNextRow();
                                ImGui::TableNextColumn();
                                ImGui::Text("location");
                                ImGui::TableNextColumn();
                                ImGui::Text("(%.3f, %.3f, %.3f)", m.location[0], m.location[1], m.location[2]);

                                ImGui::TableNextRow();
                                ImGui::TableNextColumn();
                                ImGui::Text("yaw");
                                ImGui::TableNextColumn();
                                ImGui::Text("%.3f", m.yaw);

                                ImGui::TableNextRow();
                                ImGui::TableNextColumn();
                                if (ImGui::Button("Add Mob", button_dims))
                                {
                                    const editor_command cmd_desc{
                                        .command_type = editor_command::editor_command_type::add_to_level,
                                        .mode_type_option = editor_command::model_type::mob_model,
                                        .id = m.id,
                                    };
                                    wls->editor_commands.commands.push_back(cmd_desc);
                                }
                                ImGui::TableNextColumn();
                                if (ImGui::Button("Add Static", button_dims))
                                {
                                    const editor_command cmd_desc{
                                        .command_type = editor_command::editor_command_type::add_to_level,
                                        .mode_type_option = editor_command::model_type::static_model,
                                        .id = m.id,
                                    };
                                    wls->editor_commands.commands.push_back(cmd_desc);
                                }
                                ImGui::TableNextColumn();

                                ImGui::TableNextRow();
                                ImGui::TableNextColumn();
                                if (ImGui::Button("Remove", button_dims))
                                {
                                    const editor_command cmd_desc{
                                        .command_type = editor_command::editor_command_type::remove_from_level,
                                        .id = m.id,
                                    };
                                    wls->editor_commands.commands.push_back(cmd_desc);
                                }
                                ImGui::TableNextColumn();
                                ImGui::Text("");

                                ImGui::EndTable();
                            }
                        }
                    }
                }
            }
        }
        ImGui::EndTabItem();
    }
}
