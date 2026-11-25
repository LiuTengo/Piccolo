#include "editor/include/editor_animation_fsm.h"

#include "runtime/core/base/macro.h"
#include "runtime/function/animation/animation_fsm_validation.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <utility>

namespace Piccolo
{
    namespace
    {
        constexpr float kNodeHalfWidth  = 90.0f;
        constexpr float kNodeHalfHeight = 42.0f;
        constexpr float kSplitterWidth  = 7.0f;
        constexpr float kMinSideWidth   = 180.0f;
        constexpr float kMinCenterWidth = 220.0f;

        bool inputTextString(const char* label, std::string& value)
        {
            char buffer[256] {};
            const size_t copy_size = std::min(value.size(), sizeof(buffer) - 1);
            std::memcpy(buffer, value.data(), copy_size);

            if (ImGui::InputText(label, buffer, sizeof(buffer)))
            {
                value = buffer;
                return true;
            }
            return false;
        }

        ImVec2 sampleBezier(const ImVec2& p0, const ImVec2& p1, const ImVec2& p2, const ImVec2& p3, float t)
        {
            const float one_minus_t = 1.0f - t;
            const float b0          = one_minus_t * one_minus_t * one_minus_t;
            const float b1          = 3.0f * one_minus_t * one_minus_t * t;
            const float b2          = 3.0f * one_minus_t * t * t;
            const float b3          = t * t * t;

            return ImVec2 {
                b0 * p0.x + b1 * p1.x + b2 * p2.x + b3 * p3.x,
                b0 * p0.y + b1 * p1.y + b2 * p2.y + b3 * p3.y,
            };
        }

        float squaredDistance(const ImVec2& lhs, const ImVec2& rhs)
        {
            const float dx = lhs.x - rhs.x;
            const float dy = lhs.y - rhs.y;
            return dx * dx + dy * dy;
        }

        std::string getFileName(const std::string& file_path)
        {
            return std::filesystem::path(file_path).filename().generic_string();
        }

        void drawVerticalSplitter(const char* id, float height, float& panel_width, float drag_direction, float min_width, float max_width)
        {
            ImGui::InvisibleButton(id, ImVec2(kSplitterWidth, height));
            const bool hovered = ImGui::IsItemHovered();
            const bool active  = ImGui::IsItemActive();

            if (hovered || active)
            {
                ImGui::SetMouseCursor(ImGuiMouseCursor_ResizeEW);
            }
            if (active)
            {
                panel_width = std::clamp(panel_width + ImGui::GetIO().MouseDelta.x * drag_direction,
                                         min_width,
                                         std::max(min_width, max_width));
            }

            const ImVec2 min = ImGui::GetItemRectMin();
            const ImVec2 max = ImGui::GetItemRectMax();
            ImDrawList* draw_list = ImGui::GetWindowDrawList();
            if (hovered || active)
            {
                draw_list->AddRectFilled(min, max, IM_COL32(95, 95, 110, 120));
            }
            draw_list->AddLine(ImVec2((min.x + max.x) * 0.5f, min.y + 2.0f),
                               ImVec2((min.x + max.x) * 0.5f, max.y - 2.0f),
                               IM_COL32(120, 124, 140, 180),
                               1.0f);
        }
    } // namespace

    void AnimationFSMEditor::open(const std::string& file_path)
    {
        if (m_dirty && m_open)
        {
            m_pending_action    = PendingAction::OpenFile;
            m_pending_file_path = file_path;
            m_discard_popup_requested = true;
            return;
        }

        openFileNow(file_path);
    }

    void AnimationFSMEditor::draw()
    {
        if (!m_open)
        {
            return;
        }

        std::string window_title = m_dirty ? "* Animation State Machine###AnimationFSMEditor"
                                           : "Animation State Machine###AnimationFSMEditor";
        ImGui::SetNextWindowSize(ImVec2(1280.0f, 760.0f), ImGuiCond_FirstUseEver);
        if (!ImGui::Begin(window_title.c_str(), &m_open))
        {
            ImGui::End();
            return;
        }

        drawToolbar();
        ImGui::Separator();

        const ImVec2 available = ImGui::GetContentRegionAvail();
        const float max_left_width = std::max(
            kMinSideWidth,
            available.x - 2.0f * kSplitterWidth - kMinCenterWidth - m_right_panel_width);
        m_left_panel_width = std::clamp(m_left_panel_width, kMinSideWidth, max_left_width);

        const float max_right_width = std::max(
            kMinSideWidth,
            available.x - 2.0f * kSplitterWidth - kMinCenterWidth - m_left_panel_width);
        m_right_panel_width = std::clamp(m_right_panel_width, kMinSideWidth, max_right_width);

        const float center_width = std::max(
            kMinCenterWidth,
            available.x - m_left_panel_width - m_right_panel_width - 2.0f * kSplitterWidth);

        ImGui::BeginChild("##FSMStatesPanel", ImVec2(m_left_panel_width, available.y), true);
        drawSidePanel();
        ImGui::EndChild();

        ImGui::SameLine(0.0f, 0.0f);
        drawVerticalSplitter("##FSMLeftSplitter",
                             available.y,
                             m_left_panel_width,
                             1.0f,
                             kMinSideWidth,
                             std::max(kMinSideWidth, available.x - 2.0f * kSplitterWidth - kMinCenterWidth - m_right_panel_width));
        ImGui::SameLine(0.0f, 0.0f);
        ImGui::BeginChild("##FSMGraphPanel", ImVec2(center_width, available.y), true);
        drawGraph();
        ImGui::EndChild();

        ImGui::SameLine(0.0f, 0.0f);
        drawVerticalSplitter("##FSMRightSplitter",
                             available.y,
                             m_right_panel_width,
                             -1.0f,
                             kMinSideWidth,
                             std::max(kMinSideWidth, available.x - 2.0f * kSplitterWidth - kMinCenterWidth - m_left_panel_width));
        ImGui::SameLine(0.0f, 0.0f);
        ImGui::BeginChild("##FSMInspectorPanel", ImVec2(m_right_panel_width, available.y), true);
        drawInspector();
        ImGui::EndChild();

        drawDiscardPopup();
        ImGui::End();
    }

    void AnimationFSMEditor::drawToolbar()
    {
        ImGui::TextUnformatted(m_file_path.empty() ? "<No file>" : getFileName(m_file_path).c_str());
        ImGui::SameLine();
        ImGui::TextDisabled("%s", m_dirty ? "Modified" : "Saved");

        if (ImGui::Button("Save"))
        {
            saveDocument();
        }
        ImGui::SameLine();
        if (ImGui::Button("Reload"))
        {
            if (m_dirty)
            {
                m_pending_action          = PendingAction::ReloadFile;
                m_pending_file_path       = m_file_path;
                m_discard_popup_requested = true;
            }
            else
            {
                reloadFile(m_file_path);
            }
        }
        ImGui::SameLine();
        if (ImGui::Button("Validate"))
        {
            validateDocument();
        }
        ImGui::SameLine();
        if (ImGui::Button("Add State"))
        {
            addState();
        }
        ImGui::SameLine();
        if (ImGui::Button("Add Transition"))
        {
            if (m_selected_state >= 0 && m_selected_state < static_cast<int>(m_states.size()))
            {
                m_transition_source    = m_selected_state;
                m_linking_transition   = true;
                m_selected_transition  = -1;
            }
        }

        if (m_linking_transition)
        {
            ImGui::SameLine();
            ImGui::TextColored(ImVec4(1.0f, 0.75f, 0.2f, 1.0f), "Select target state. Esc cancels.");
        }
    }

    void AnimationFSMEditor::drawSidePanel()
    {
        if (ImGui::CollapsingHeader("Parameters", ImGuiTreeNodeFlags_DefaultOpen))
        {
            if (ImGui::Button("+ Parameter", ImVec2(-1.0f, 0.0f)))
            {
                ImGui::OpenPopup("AddParameterPopup");
            }
            if (ImGui::BeginPopup("AddParameterPopup"))
            {
                const std::array<const char*, 3> types {"Float", "Bool", "Int"};
                for (const char* type : types)
                {
                    if (ImGui::MenuItem(type))
                    {
                        AnimationFSMEditorParameter parameter;
                        parameter.m_type = type;
                        std::string base_name = "new_parameter";
                        int         suffix    = 0;
                        parameter.m_name      = base_name;
                        while (findParameterIndex(parameter.m_name) >= 0)
                        {
                            ++suffix;
                            parameter.m_name = base_name + "_" + std::to_string(suffix);
                        }
                        if (parameter.m_type == "Bool")
                            parameter.m_default_value = false;
                        else if (parameter.m_type == "Int")
                            parameter.m_default_value = 0;
                        else
                            parameter.m_default_value = 0.0;
                        m_parameters.push_back(parameter);
                        markDirty();
                    }
                }
                ImGui::EndPopup();
            }

            int remove_parameter = -1;
            for (int parameter_index = 0; parameter_index < static_cast<int>(m_parameters.size());
                 ++parameter_index)
            {
                AnimationFSMEditorParameter& parameter = m_parameters[parameter_index];
                ImGui::PushID(parameter_index);

                std::string old_name = parameter.m_name;
                if (inputTextString("##ParameterName", parameter.m_name))
                {
                    for (auto& transition : m_transitions)
                    {
                        for (auto& condition : transition.m_conditions)
                        {
                            if (condition.parameter_name == old_name)
                            {
                                condition.parameter_name = parameter.m_name;
                            }
                        }
                    }
                    markDirty();
                }

                ImGui::SameLine();
                if (ImGui::SmallButton("X"))
                {
                    remove_parameter = parameter_index;
                }

                if (ImGui::BeginCombo("##ParameterType", parameter.m_type.c_str()))
                {
                    const std::array<const char*, 3> types {"Float", "Bool", "Int"};
                    for (const char* type : types)
                    {
                        const bool selected = parameter.m_type == type;
                        if (ImGui::Selectable(type, selected))
                        {
                            parameter.m_type = type;
                            if (type == "Bool")
                                parameter.m_default_value = false;
                            else if (type == "Int")
                                parameter.m_default_value = 0;
                            else
                                parameter.m_default_value = 0.0;
                            markDirty();
                        }
                    }
                    ImGui::EndCombo();
                }

                ImGui::SameLine();
                if (drawConditionValue("##ParameterDefault", parameter.m_type, parameter.m_default_value))
                {
                    markDirty();
                }

                ImGui::PopID();
            }
            if (remove_parameter >= 0)
            {
                m_parameters.erase(m_parameters.begin() + remove_parameter);
                markDirty();
            }
            ImGui::TextDisabled("Built-in: clip_finish (Bool)");
        }

        ImGui::Separator();
        if (ImGui::CollapsingHeader("States", ImGuiTreeNodeFlags_DefaultOpen))
        {
            for (int state_index = 0; state_index < static_cast<int>(m_states.size()); ++state_index)
            {
                AnimationFSMEditorState& state = m_states[state_index];
                ImGui::PushID(state_index);
                std::string label;
                if (state.m_state_id == m_initial_state_id)
                    label += "* ";
                label += state.m_state_name;
                if (ImGui::Selectable(label.c_str(), m_selected_state == state_index))
                {
                    m_selected_state      = state_index;
                    m_selected_transition = -1;
                }
                ImGui::SameLine();
                ImGui::TextDisabled("%s", state.m_loop ? "[loop]" : "[once]");
                ImGui::PopID();
            }
        }

        ImGui::Separator();
        if (ImGui::CollapsingHeader("Transitions", ImGuiTreeNodeFlags_DefaultOpen))
        {
            for (int transition_index = 0; transition_index < static_cast<int>(m_transitions.size());
                 ++transition_index)
            {
                const AnimationFSMEditorTransition& transition = m_transitions[transition_index];
                const std::string                    source_name =
                    transition.m_source >= 0 && transition.m_source < static_cast<int>(m_states.size())
                        ? m_states[transition.m_source].m_state_name
                        : "<invalid>";
                const std::string target_name =
                    transition.m_target >= 0 && transition.m_target < static_cast<int>(m_states.size())
                        ? m_states[transition.m_target].m_state_name
                        : "<invalid>";
                const std::string label = source_name + " -> " + target_name + "  [" +
                                          std::to_string(transition.m_conditions.size()) + " cond]";
                ImGui::PushID(transition_index);
                if (ImGui::Selectable(label.c_str(), m_selected_transition == transition_index))
                {
                    m_selected_transition = transition_index;
                    m_selected_state      = -1;
                }
                ImGui::PopID();
            }
        }

        ImGui::Separator();
        drawValidationErrors();
    }

    void AnimationFSMEditor::drawGraph()
    {
        const ImVec2 canvas_size = ImGui::GetContentRegionAvail();
        if (canvas_size.x < 2.0f || canvas_size.y < 2.0f)
        {
            return;
        }

        const ImVec2 canvas_origin = ImGui::GetCursorScreenPos();
        ImGui::InvisibleButton("##FSMGraphCanvas",
                               canvas_size,
                               ImGuiButtonFlags_MouseButtonLeft | ImGuiButtonFlags_MouseButtonRight |
                                   ImGuiButtonFlags_MouseButtonMiddle);
        const bool hovered = ImGui::IsItemHovered();

        if (hovered)
        {
            const ImGuiIO& io = ImGui::GetIO();
            if (std::abs(io.MouseWheel) > 0.001f)
            {
                const ImVec2 mouse = io.MousePos;
                const ImVec2 world {
                    (mouse.x - canvas_origin.x - m_canvas_offset.x) / m_zoom,
                    (mouse.y - canvas_origin.y - m_canvas_offset.y) / m_zoom,
                };
                m_zoom = std::clamp(m_zoom * (1.0f + io.MouseWheel * 0.12f), 0.4f, 2.0f);
                m_canvas_offset.x = mouse.x - canvas_origin.x - world.x * m_zoom;
                m_canvas_offset.y = mouse.y - canvas_origin.y - world.y * m_zoom;
            }

            if ((ImGui::IsMouseDown(ImGuiMouseButton_Middle) || ImGui::IsMouseDown(ImGuiMouseButton_Right)))
            {
                const ImVec2 delta = io.MouseDelta;
                m_canvas_offset.x += delta.x;
                m_canvas_offset.y += delta.y;
            }
        }

        if (ImGui::IsKeyPressed(ImGuiKey_Escape))
        {
            m_linking_transition = false;
            m_transition_source  = -1;
        }

        const ImVec2 mouse       = ImGui::GetIO().MousePos;
        int          hovered_state = -1;
        for (int state_index = 0; state_index < static_cast<int>(m_states.size()); ++state_index)
        {
            const ImVec2 center = getStateCenter(canvas_origin, canvas_size, state_index);
            const ImVec2 min {center.x - kNodeHalfWidth * m_zoom, center.y - kNodeHalfHeight * m_zoom};
            const ImVec2 max {center.x + kNodeHalfWidth * m_zoom, center.y + kNodeHalfHeight * m_zoom};
            if (mouse.x >= min.x && mouse.y >= min.y && mouse.x <= max.x && mouse.y <= max.y)
            {
                hovered_state = state_index;
                break;
            }
        }

        if (hovered && ImGui::IsMouseClicked(ImGuiMouseButton_Left))
        {
            if (hovered_state >= 0)
            {
                if (m_linking_transition && m_transition_source >= 0)
                {
                    if (hovered_state != m_transition_source)
                    {
                        addTransition(m_transition_source, hovered_state);
                    }
                    m_linking_transition = false;
                    m_transition_source  = -1;
                }
                else
                {
                    m_selected_state      = hovered_state;
                    m_selected_transition = -1;
                    m_dragged_state       = hovered_state;
                }
            }
            else if (!m_linking_transition)
            {
                const int transition_index = hitTestTransition(mouse, canvas_origin, canvas_size);
                m_selected_transition      = transition_index;
                m_selected_state           = -1;
            }
        }

        if (m_dragged_state >= 0 && m_dragged_state < static_cast<int>(m_states.size()))
        {
            if (ImGui::IsMouseDown(ImGuiMouseButton_Left))
            {
                const ImVec2 delta = ImGui::GetIO().MouseDelta;
                if (delta.x != 0.0f || delta.y != 0.0f)
                {
                    m_states[m_dragged_state].m_editor_x += delta.x / m_zoom;
                    m_states[m_dragged_state].m_editor_y += delta.y / m_zoom;
                    markDirty();
                }
            }
            else
            {
                m_dragged_state = -1;
            }
        }

        ImDrawList* draw_list = ImGui::GetWindowDrawList();
        draw_list->PushClipRect(canvas_origin,
                                ImVec2(canvas_origin.x + canvas_size.x, canvas_origin.y + canvas_size.y),
                                true);
        draw_list->AddRectFilled(canvas_origin,
                                 ImVec2(canvas_origin.x + canvas_size.x, canvas_origin.y + canvas_size.y),
                                 IM_COL32(28, 28, 32, 255),
                                 4.0f);

        for (int transition_index = 0; transition_index < static_cast<int>(m_transitions.size());
             ++transition_index)
        {
            drawTransition(draw_list,
                           canvas_origin,
                           canvas_size,
                           transition_index,
                           transition_index == m_selected_transition);
        }

        for (int state_index = 0; state_index < static_cast<int>(m_states.size()); ++state_index)
        {
            const AnimationFSMEditorState& state = m_states[state_index];
            const ImVec2 center = getStateCenter(canvas_origin, canvas_size, state_index);
            const ImVec2 min {center.x - kNodeHalfWidth * m_zoom, center.y - kNodeHalfHeight * m_zoom};
            const ImVec2 max {center.x + kNodeHalfWidth * m_zoom, center.y + kNodeHalfHeight * m_zoom};

            ImU32 fill_color = IM_COL32(54, 58, 66, 255);
            if (state.m_state_id == m_initial_state_id)
                fill_color = IM_COL32(61, 82, 67, 255);
            if (state_index == m_selected_state)
                fill_color = IM_COL32(78, 82, 105, 255);
            if (m_transition_source == state_index)
                fill_color = IM_COL32(105, 78, 45, 255);

            draw_list->AddRectFilled(min, max, fill_color, 6.0f);
            draw_list->AddRect(min,
                               max,
                               state_index == m_selected_state ? IM_COL32(220, 220, 255, 255)
                                                               : IM_COL32(110, 114, 125, 255),
                               6.0f,
                               0,
                               1.5f);

            const float font_size = std::max(10.0f, ImGui::GetFontSize() * m_zoom);
            const ImVec2 name_size = ImGui::CalcTextSize(state.m_state_name.c_str());
            draw_list->AddText(
                ImGui::GetFont(),
                font_size,
                ImVec2(center.x - name_size.x * m_zoom * 0.5f, center.y - 24.0f * m_zoom),
                IM_COL32(245, 245, 250, 255),
                state.m_state_name.c_str());
            draw_list->AddText(ImGui::GetFont(),
                               font_size * 0.8f,
                               ImVec2(min.x + 10.0f * m_zoom, center.y + 2.0f * m_zoom),
                               IM_COL32(195, 200, 210, 255),
                               state.m_clip_name.c_str());
            draw_list->AddText(ImGui::GetFont(),
                               font_size * 0.72f,
                               ImVec2(min.x + 10.0f * m_zoom, center.y + 22.0f * m_zoom),
                               IM_COL32(150, 155, 165, 255),
                               state.m_loop ? "Loop" : "Once");
        }

        if (m_linking_transition)
        {
            draw_list->AddText(ImVec2(canvas_origin.x + 12.0f, canvas_origin.y + 10.0f),
                               IM_COL32(255, 210, 110, 255),
                               "Adding transition: select target state");
        }

        draw_list->PopClipRect();
    }

    void AnimationFSMEditor::drawInspector()
    {
        if (m_selected_transition >= 0 &&
            m_selected_transition < static_cast<int>(m_transitions.size()))
        {
            AnimationFSMEditorTransition& transition = m_transitions[m_selected_transition];
            ImGui::TextUnformatted("Transition");
            ImGui::Separator();

            const std::string source_label =
                transition.m_source >= 0 && transition.m_source < static_cast<int>(m_states.size())
                    ? m_states[transition.m_source].m_state_name
                    : "<invalid>";
            const std::string target_label =
                transition.m_target >= 0 && transition.m_target < static_cast<int>(m_states.size())
                    ? m_states[transition.m_target].m_state_name
                    : "<invalid>";

            if (ImGui::BeginCombo("Source", source_label.c_str()))
            {
                for (int state_index = 0; state_index < static_cast<int>(m_states.size()); ++state_index)
                {
                    if (ImGui::Selectable(m_states[state_index].m_state_name.c_str(),
                                          transition.m_source == state_index))
                    {
                        transition.m_source = state_index;
                        markDirty();
                    }
                }
                ImGui::EndCombo();
            }
            if (ImGui::BeginCombo("Target", target_label.c_str()))
            {
                for (int state_index = 0; state_index < static_cast<int>(m_states.size()); ++state_index)
                {
                    if (ImGui::Selectable(m_states[state_index].m_state_name.c_str(),
                                          transition.m_target == state_index))
                    {
                        transition.m_target = state_index;
                        markDirty();
                    }
                }
                ImGui::EndCombo();
            }

            ImGui::Separator();
            ImGui::TextUnformatted("Conditions");
            int remove_condition = -1;
            for (int condition_index = 0;
                 condition_index < static_cast<int>(transition.m_conditions.size());
                 ++condition_index)
            {
                ImGui::PushID(condition_index);
                ImGui::Text("Condition %d", condition_index + 1);
                ImGui::SameLine();
                if (ImGui::SmallButton("Remove"))
                {
                    remove_condition = condition_index;
                }
                if (drawParameterCondition(transition.m_conditions[condition_index], condition_index))
                {
                    markDirty();
                }
                ImGui::Separator();
                ImGui::PopID();
            }
            if (remove_condition >= 0)
            {
                transition.m_conditions.erase(transition.m_conditions.begin() + remove_condition);
                markDirty();
            }

            if (ImGui::Button("+ Condition", ImVec2(-1.0f, 0.0f)))
            {
                FSMCondition condition;
                condition.parameter_name = m_parameters.empty() ? "clip_finish" : m_parameters.front().m_name;
                condition.op             = CompareOperator::Equal;
                applyDefaultValue(condition, getParameterType(condition.parameter_name));
                transition.m_conditions.push_back(condition);
                markDirty();
            }

            ImGui::Separator();
            if (ImGui::Button("Delete Transition", ImVec2(-1.0f, 0.0f)))
            {
                deleteSelectedTransition();
            }
            return;
        }

        if (m_selected_state >= 0 && m_selected_state < static_cast<int>(m_states.size()))
        {
            AnimationFSMEditorState& state = m_states[m_selected_state];
            ImGui::TextUnformatted("State");
            ImGui::Separator();

            if (inputTextString("Name", state.m_state_name))
                markDirty();
            if (inputTextString("Clip", state.m_clip_name))
                markDirty();
            if (ImGui::Checkbox("Loop", &state.m_loop))
                markDirty();

            if (state.m_state_id == m_initial_state_id)
            {
                ImGui::TextColored(ImVec4(0.55f, 1.0f, 0.55f, 1.0f), "Initial state");
            }
            else if (ImGui::Button("Set As Initial State", ImVec2(-1.0f, 0.0f)))
            {
                m_initial_state_id = state.m_state_id;
                markDirty();
            }

            ImGui::Separator();
            if (ImGui::Button("Delete State", ImVec2(-1.0f, 0.0f)))
            {
                deleteSelectedState();
            }
            return;
        }

        ImGui::TextUnformatted("Nothing selected.");
        ImGui::Spacing();
        ImGui::TextWrapped("Select a state or transition to edit its properties.");
    }

    void AnimationFSMEditor::drawValidationErrors()
    {
        if (!m_has_validation_result)
        {
            ImGui::TextDisabled("Validation: not checked");
            return;
        }

        if (m_validation_errors.empty())
        {
            ImGui::TextColored(ImVec4(0.55f, 1.0f, 0.55f, 1.0f), "Validation: OK");
            return;
        }

        ImGui::TextColored(ImVec4(1.0f, 0.45f, 0.45f, 1.0f),
                           "Validation: %zu error(s)",
                           m_validation_errors.size());
        ImGui::BeginChild("##FSMValidationErrors", ImVec2(0.0f, 110.0f), true);
        for (const std::string& error : m_validation_errors)
        {
            ImGui::TextWrapped("%s", error.c_str());
        }
        ImGui::EndChild();
    }

    void AnimationFSMEditor::drawDiscardPopup()
    {
        if (m_discard_popup_requested)
        {
            ImGui::OpenPopup("Unsaved FSM Changes");
            m_discard_popup_requested = false;
        }

        if (ImGui::BeginPopupModal("Unsaved FSM Changes", nullptr, ImGuiWindowFlags_AlwaysAutoResize))
        {
            ImGui::TextUnformatted("Discard unsaved changes?");
            if (ImGui::Button("Discard", ImVec2(120.0f, 0.0f)))
            {
                const PendingAction action = m_pending_action;
                const std::string   path   = m_pending_file_path;
                m_pending_action           = PendingAction::None;
                m_pending_file_path.clear();
                ImGui::CloseCurrentPopup();
                if (action == PendingAction::OpenFile)
                    openFileNow(path);
                else if (action == PendingAction::ReloadFile)
                    reloadFile(path);
            }
            ImGui::SameLine();
            if (ImGui::Button("Cancel", ImVec2(120.0f, 0.0f)))
            {
                m_pending_action = PendingAction::None;
                m_pending_file_path.clear();
                ImGui::CloseCurrentPopup();
            }
            ImGui::EndPopup();
        }
    }

    bool AnimationFSMEditor::openFileNow(const std::string& file_path)
    {
        m_file_path = file_path;
        m_open      = true;
        std::ifstream file(file_path);
        if (!file.is_open())
        {
            m_validation_errors = {"Open FSM file failed: " + file_path};
            m_has_validation_result = true;
            return false;
        }

        std::stringstream buffer;
        buffer << file.rdbuf();
        std::string   parse_error;
        json11::Json root = json11::Json::parse(buffer.str(), parse_error);
        if (!parse_error.empty())
        {
            m_validation_errors = {"Parse FSM JSON failed: " + parse_error};
            m_has_validation_result = true;
            return false;
        }

        const bool parsed = parseDocument(root);
        const AnimationFSMValidationResult validation = validateAnimationFSMJson(root);
        m_validation_errors = validation.m_errors;
        m_has_validation_result = true;
        m_dirty = false;
        return parsed;
    }

    bool AnimationFSMEditor::reloadFile(const std::string& file_path)
    {
        return openFileNow(file_path);
    }

    bool AnimationFSMEditor::parseDocument(const json11::Json& root)
    {
        if (!root.is_object())
        {
            m_validation_errors = {"FSM root must be a JSON object."};
            m_has_validation_result = true;
            return false;
        }

        m_parameters.clear();
        m_states.clear();
        m_transitions.clear();
        m_selected_state      = -1;
        m_selected_transition = -1;
        m_initial_state_id    = root["initial_state_id"].int_value();

        const auto& parameters = root["parameters"].array_items();
        for (const json11::Json& parameter_json : parameters)
        {
            AnimationFSMEditorParameter parameter;
            parameter.m_name          = parameter_json["name"].string_value();
            parameter.m_type          = parameter_json["type"].string_value();
            parameter.m_default_value = parameter_json["default_value"];
            m_parameters.push_back(parameter);
        }

        const auto& states = root["states"].array_items();
        for (int state_index = 0; state_index < static_cast<int>(states.size()); ++state_index)
        {
            const json11::Json& state_json = states[state_index];
            AnimationFSMEditorState state;
            state.m_state_id   = state_index;
            state.m_state_name = state_json["state_name"].string_value();
            state.m_clip_name  = state_json["clip_name"].string_value();
            state.m_loop       = state_json["loop"].bool_value();
            state.m_editor_x   = static_cast<float>(80.0 + (state_index % 4) * 250.0);
            state.m_editor_y   = static_cast<float>(80.0 + (state_index / 4) * 180.0);
            if (state_json["editor_position"].is_object())
            {
                state.m_editor_x = static_cast<float>(state_json["editor_position"]["x"].number_value());
                state.m_editor_y = static_cast<float>(state_json["editor_position"]["y"].number_value());
            }
            m_states.push_back(state);
        }

        const auto& transitions = root["transitions"].array_items();
        for (const json11::Json& transition_json : transitions)
        {
            AnimationFSMEditorTransition transition;
            transition.m_source = transition_json["source"].int_value();
            transition.m_target = transition_json["target"].int_value();

            for (const json11::Json& condition_json : transition_json["conditions"].array_items())
            {
                FSMCondition condition;
                condition.parameter_name = condition_json["parameter_name"].string_value();
                const std::string op      = condition_json["op"].string_value();
                if (op == "NotEqual")
                    condition.op = CompareOperator::NotEqual;
                else if (op == "Greater")
                    condition.op = CompareOperator::Greater;
                else if (op == "Less")
                    condition.op = CompareOperator::Less;
                else if (op == "GreaterEqual")
                    condition.op = CompareOperator::GreaterEqual;
                else if (op == "LessEqual")
                    condition.op = CompareOperator::LessEqual;
                else
                    condition.op = CompareOperator::Equal;
                condition.value = condition_json["value"];
                transition.m_conditions.push_back(condition);
            }
            m_transitions.push_back(transition);
        }

        return true;
    }

    json11::Json AnimationFSMEditor::buildDocumentJson() const
    {
        json11::Json::array parameter_array;
        for (const AnimationFSMEditorParameter& parameter : m_parameters)
        {
            json11::Json::object parameter_json;
            parameter_json["name"]          = parameter.m_name;
            parameter_json["type"]          = parameter.m_type;
            parameter_json["default_value"] = parameter.m_default_value;
            parameter_array.push_back(parameter_json);
        }

        std::vector<std::vector<int>> outgoing_transitions(m_states.size());
        for (int transition_index = 0; transition_index < static_cast<int>(m_transitions.size());
             ++transition_index)
        {
            const int source = m_transitions[transition_index].m_source;
            if (source >= 0 && source < static_cast<int>(m_states.size()))
            {
                outgoing_transitions[source].push_back(transition_index);
            }
        }

        json11::Json::array state_array;
        for (int state_index = 0; state_index < static_cast<int>(m_states.size()); ++state_index)
        {
            const AnimationFSMEditorState& state = m_states[state_index];
            json11::Json::object state_json;
            state_json["state_id"]   = state.m_state_id;
            state_json["state_name"] = state.m_state_name;
            state_json["clip_name"]  = state.m_clip_name;
            state_json["loop"]       = state.m_loop;

            json11::Json::object position_json;
            position_json["x"] = static_cast<double>(state.m_editor_x);
            position_json["y"] = static_cast<double>(state.m_editor_y);
            state_json["editor_position"] = position_json;
            state_json["transitions"] =
                json11::Json(outgoing_transitions[static_cast<size_t>(state_index)]);
            state_array.push_back(state_json);
        }

        json11::Json::array transition_array;
        for (const AnimationFSMEditorTransition& transition : m_transitions)
        {
            json11::Json::object transition_json;
            transition_json["source"] = transition.m_source;
            transition_json["target"] = transition.m_target;

            json11::Json::array condition_array;
            for (const FSMCondition& condition : transition.m_conditions)
            {
                json11::Json::object condition_json;
                condition_json["parameter_name"] = condition.parameter_name;
                condition_json["op"]             = getOperatorName(condition.op);
                condition_json["value"]          = condition.value;
                condition_array.push_back(condition_json);
            }
            transition_json["conditions"] = condition_array;
            transition_array.push_back(transition_json);
        }

        json11::Json::object root;
        root["version"]          = 1;
        root["initial_state_id"] = m_initial_state_id;
        root["parameters"]       = parameter_array;
        root["states"]           = state_array;
        root["transitions"]      = transition_array;
        return root;
    }

    bool AnimationFSMEditor::saveDocument()
    {
        validateDocument();
        if (!m_validation_errors.empty())
        {
            LOG_ERROR("Cannot save invalid animation FSM: {}", m_file_path);
            return false;
        }

        const json11::Json root = buildDocumentJson();
        std::ofstream file(m_file_path);
        if (!file.is_open())
        {
            LOG_ERROR("Open animation FSM for writing failed: {}", m_file_path);
            return false;
        }
        std::string json_text;
        root.dump(json_text);
        file << json_text;
        file.flush();
        if (!file.good())
        {
            LOG_ERROR("Write animation FSM failed: {}", m_file_path);
            return false;
        }

        m_dirty = false;
        return true;
    }

    void AnimationFSMEditor::validateDocument()
    {
        const json11::Json root = buildDocumentJson();
        m_validation_errors     = validateAnimationFSMJson(root).m_errors;
        m_has_validation_result = true;
    }

    bool AnimationFSMEditor::drawParameterCondition(FSMCondition& condition, int condition_index)
    {
        bool changed = false;
        std::string parameter_type = getParameterType(condition.parameter_name);

        ImGui::TextUnformatted("Parameter");
        ImGui::SetNextItemWidth(-1.0f);
        if (drawParameterCombo("##ConditionParameter", condition.parameter_name))
        {
            parameter_type = getParameterType(condition.parameter_name);
            applyDefaultValue(condition, parameter_type);
            changed = true;
        }

        ImGui::TextUnformatted("Operator");
        ImGui::SetNextItemWidth(-1.0f);
        if (drawOperatorCombo("##ConditionOperator", condition.op, parameter_type))
            changed = true;

        ImGui::TextUnformatted("Value");
        ImGui::SetNextItemWidth(-1.0f);
        if (drawConditionValue("##ConditionValue", parameter_type, condition.value))
            changed = true;
        return changed;
    }

    bool AnimationFSMEditor::drawParameterCombo(const char* label, std::string& parameter_name)
    {
        bool changed = false;
        if (ImGui::BeginCombo(label, parameter_name.c_str()))
        {
            for (const AnimationFSMEditorParameter& parameter : m_parameters)
            {
                if (ImGui::Selectable(parameter.m_name.c_str(), parameter.m_name == parameter_name))
                {
                    parameter_name = parameter.m_name;
                    changed        = true;
                }
            }
            if (ImGui::Selectable("clip_finish", parameter_name == "clip_finish"))
            {
                parameter_name = "clip_finish";
                changed        = true;
            }
            ImGui::EndCombo();
        }
        return changed;
    }

    bool AnimationFSMEditor::drawOperatorCombo(const char* label,
                                               CompareOperator& op,
                                               const std::string& parameter_type)
    {
        const std::array<std::pair<CompareOperator, const char*>, 6> operators {{
            {CompareOperator::Equal, "Equal"},
            {CompareOperator::NotEqual, "NotEqual"},
            {CompareOperator::Greater, "Greater"},
            {CompareOperator::Less, "Less"},
            {CompareOperator::GreaterEqual, "GreaterEqual"},
            {CompareOperator::LessEqual, "LessEqual"},
        }};

        bool changed = false;
        if (ImGui::BeginCombo(label, getOperatorName(op).c_str()))
        {
            for (const auto& [candidate, name] : operators)
            {
                if (parameter_type == "Bool" &&
                    candidate != CompareOperator::Equal && candidate != CompareOperator::NotEqual)
                {
                    continue;
                }
                if (ImGui::Selectable(name, candidate == op))
                {
                    op      = candidate;
                    changed = true;
                }
            }
            ImGui::EndCombo();
        }
        return changed;
    }

    bool AnimationFSMEditor::drawConditionValue(const char*     label,
                                                const std::string& parameter_type,
                                                json11::Json&   value)
    {
        if (parameter_type == "Bool")
        {
            bool current = value.bool_value();
            if (ImGui::Checkbox(label, &current))
            {
                value = current;
                return true;
            }
            return false;
        }

        if (parameter_type == "Int")
        {
            int current = value.int_value();
            if (ImGui::InputInt(label, &current))
            {
                value = current;
                return true;
            }
            return false;
        }

        float current = static_cast<float>(value.number_value());
        if (ImGui::DragFloat(label, &current, 0.01f))
        {
            value = static_cast<double>(current);
            return true;
        }
        return false;
    }

    void AnimationFSMEditor::addState()
    {
        AnimationFSMEditorState state;
        state.m_state_id   = static_cast<int>(m_states.size());
        state.m_state_name = "NewState_" + std::to_string(state.m_state_id);
        state.m_clip_name.clear();
        state.m_loop     = true;
        state.m_editor_x = 100.0f + static_cast<float>((state.m_state_id % 4) * 230);
        state.m_editor_y = 120.0f + static_cast<float>((state.m_state_id / 4) * 150);
        m_states.push_back(state);
        m_selected_state      = static_cast<int>(m_states.size()) - 1;
        m_selected_transition = -1;
        markDirty();
    }

    void AnimationFSMEditor::deleteSelectedState()
    {
        if (m_selected_state < 0 || m_selected_state >= static_cast<int>(m_states.size()))
            return;

        const int deleted_index = m_selected_state;
        m_states.erase(m_states.begin() + deleted_index);
        normalizeAfterStateDelete(deleted_index);
        m_selected_state      = -1;
        m_selected_transition = -1;
        markDirty();
    }

    void AnimationFSMEditor::addTransition(int source, int target)
    {
        AnimationFSMEditorTransition transition;
        transition.m_source = source;
        transition.m_target = target;
        m_transitions.push_back(transition);
        m_selected_transition = static_cast<int>(m_transitions.size()) - 1;
        m_selected_state      = -1;
        markDirty();
    }

    void AnimationFSMEditor::deleteSelectedTransition()
    {
        if (m_selected_transition < 0 ||
            m_selected_transition >= static_cast<int>(m_transitions.size()))
        {
            return;
        }
        m_transitions.erase(m_transitions.begin() + m_selected_transition);
        m_selected_transition = -1;
        markDirty();
    }

    void AnimationFSMEditor::normalizeAfterStateDelete(int deleted_index)
    {
        for (int state_index = 0; state_index < static_cast<int>(m_states.size()); ++state_index)
        {
            m_states[state_index].m_state_id = state_index;
        }

        std::vector<AnimationFSMEditorTransition> filtered;
        for (AnimationFSMEditorTransition transition : m_transitions)
        {
            if (transition.m_source == deleted_index || transition.m_target == deleted_index)
                continue;
            if (transition.m_source > deleted_index)
                --transition.m_source;
            if (transition.m_target > deleted_index)
                --transition.m_target;
            filtered.push_back(transition);
        }
        m_transitions = std::move(filtered);

        if (m_initial_state_id == deleted_index)
            m_initial_state_id = 0;
        else if (m_initial_state_id > deleted_index)
            --m_initial_state_id;
    }

    void AnimationFSMEditor::markDirty()
    {
        m_dirty                 = true;
        m_has_validation_result = false;
    }

    std::string AnimationFSMEditor::getParameterType(const std::string& parameter_name) const
    {
        const int parameter_index = findParameterIndex(parameter_name);
        if (parameter_index >= 0)
            return m_parameters[parameter_index].m_type;
        if (parameter_name == "speed")
            return "Float";
        return "Bool";
    }

    int AnimationFSMEditor::findParameterIndex(const std::string& parameter_name) const
    {
        for (int parameter_index = 0; parameter_index < static_cast<int>(m_parameters.size());
             ++parameter_index)
        {
            if (m_parameters[parameter_index].m_name == parameter_name)
                return parameter_index;
        }
        return -1;
    }

    std::string AnimationFSMEditor::getOperatorName(CompareOperator op) const
    {
        switch (op)
        {
            case CompareOperator::Equal:
                return "Equal";
            case CompareOperator::NotEqual:
                return "NotEqual";
            case CompareOperator::Greater:
                return "Greater";
            case CompareOperator::Less:
                return "Less";
            case CompareOperator::GreaterEqual:
                return "GreaterEqual";
            case CompareOperator::LessEqual:
                return "LessEqual";
        }
        return "Equal";
    }

    void AnimationFSMEditor::applyDefaultValue(FSMCondition& condition, const std::string& parameter_type)
    {
        if (parameter_type == "Bool")
            condition.value = false;
        else if (parameter_type == "Int")
            condition.value = 0;
        else
            condition.value = 0.0;
    }

    ImVec2 AnimationFSMEditor::canvasToScreen(const ImVec2& canvas_origin,
                                              const ImVec2& canvas_size,
                                              const ImVec2& point) const
    {
        (void)canvas_size;
        return ImVec2 {
            canvas_origin.x + m_canvas_offset.x + point.x * m_zoom,
            canvas_origin.y + m_canvas_offset.y + point.y * m_zoom,
        };
    }

    ImVec2 AnimationFSMEditor::screenToCanvas(const ImVec2& canvas_origin,
                                              const ImVec2& canvas_size,
                                              const ImVec2& point) const
    {
        (void)canvas_size;
        return ImVec2 {
            (point.x - canvas_origin.x - m_canvas_offset.x) / m_zoom,
            (point.y - canvas_origin.y - m_canvas_offset.y) / m_zoom,
        };
    }

    ImVec2 AnimationFSMEditor::getStateCenter(const ImVec2& canvas_origin,
                                              const ImVec2& canvas_size,
                                              int state_index) const
    {
        if (state_index < 0 || state_index >= static_cast<int>(m_states.size()))
            return canvas_origin;
        const AnimationFSMEditorState& state = m_states[state_index];
        return canvasToScreen(canvas_origin,
                              canvas_size,
                              ImVec2 {state.m_editor_x, state.m_editor_y});
    }

    void AnimationFSMEditor::drawTransition(ImDrawList* draw_list,
                                            const ImVec2& canvas_origin,
                                            const ImVec2& canvas_size,
                                            int transition_index,
                                            bool selected) const
    {
        if (transition_index < 0 || transition_index >= static_cast<int>(m_transitions.size()))
            return;
        const AnimationFSMEditorTransition& transition = m_transitions[transition_index];
        if (transition.m_source < 0 || transition.m_source >= static_cast<int>(m_states.size()) ||
            transition.m_target < 0 || transition.m_target >= static_cast<int>(m_states.size()))
        {
            return;
        }

        const ImVec2 source_center = getStateCenter(canvas_origin, canvas_size, transition.m_source);
        const ImVec2 target_center = getStateCenter(canvas_origin, canvas_size, transition.m_target);
        const float direction = target_center.x >= source_center.x ? 1.0f : -1.0f;
        const ImVec2 p0 {
            source_center.x + direction * kNodeHalfWidth * m_zoom,
            source_center.y,
        };
        const ImVec2 p3 {
            target_center.x - direction * kNodeHalfWidth * m_zoom,
            target_center.y,
        };
        const float distance = std::max(70.0f, std::abs(p3.x - p0.x) * 0.45f);
        const ImVec2 p1 {p0.x + direction * distance, p0.y};
        const ImVec2 p2 {p3.x - direction * distance, p3.y};

        const ImU32 color = selected ? IM_COL32(245, 245, 255, 255) : IM_COL32(150, 155, 170, 230);
        draw_list->AddBezierCubic(p0, p1, p2, p3, color, selected ? 2.5f : 1.7f);

        const ImVec2 arrow_base = sampleBezier(p0, p1, p2, p3, 0.96f);
        const ImVec2 arrow_tip  = p3;
        draw_list->AddTriangleFilled(
            arrow_tip,
            ImVec2(arrow_base.x - 5.0f * direction, arrow_base.y - 5.0f),
            ImVec2(arrow_base.x - 5.0f * direction, arrow_base.y + 5.0f),
            color);

        const ImVec2 midpoint = sampleBezier(p0, p1, p2, p3, 0.5f);
        draw_list->AddCircleFilled(midpoint, selected ? 8.0f : 6.0f, IM_COL32(45, 45, 52, 255));
        draw_list->AddCircle(midpoint, selected ? 8.0f : 6.0f, color, 16, 1.5f);
        if (selected)
        {
            const std::string text = std::to_string(transition.m_conditions.size());
            const ImVec2 text_size = ImGui::CalcTextSize(text.c_str());
            draw_list->AddText(
                ImVec2(midpoint.x - text_size.x * 0.5f, midpoint.y - text_size.y * 0.5f),
                IM_COL32(235, 235, 240, 255),
                text.c_str());
        }
    }

    int AnimationFSMEditor::hitTestTransition(const ImVec2& mouse_position,
                                              const ImVec2& canvas_origin,
                                              const ImVec2& canvas_size) const
    {
        float best_distance = 10.0f * 10.0f;
        int   best_index    = -1;
        for (int transition_index = 0; transition_index < static_cast<int>(m_transitions.size());
             ++transition_index)
        {
            const AnimationFSMEditorTransition& transition = m_transitions[transition_index];
            if (transition.m_source < 0 || transition.m_source >= static_cast<int>(m_states.size()) ||
                transition.m_target < 0 || transition.m_target >= static_cast<int>(m_states.size()))
            {
                continue;
            }

            const ImVec2 source_center = getStateCenter(canvas_origin, canvas_size, transition.m_source);
            const ImVec2 target_center = getStateCenter(canvas_origin, canvas_size, transition.m_target);
            const float direction = target_center.x >= source_center.x ? 1.0f : -1.0f;
            const ImVec2 p0 {
                source_center.x + direction * kNodeHalfWidth * m_zoom,
                source_center.y,
            };
            const ImVec2 p3 {
                target_center.x - direction * kNodeHalfWidth * m_zoom,
                target_center.y,
            };
            const float distance = std::max(70.0f, std::abs(p3.x - p0.x) * 0.45f);
            const ImVec2 p1 {p0.x + direction * distance, p0.y};
            const ImVec2 p2 {p3.x - direction * distance, p3.y};

            for (int sample_index = 0; sample_index <= 20; ++sample_index)
            {
                const float t = static_cast<float>(sample_index) / 20.0f;
                const float point_distance =
                    squaredDistance(sampleBezier(p0, p1, p2, p3, t), mouse_position);
                if (point_distance < best_distance)
                {
                    best_distance = point_distance;
                    best_index    = transition_index;
                }
            }
        }
        return best_index;
    }
} // namespace Piccolo
