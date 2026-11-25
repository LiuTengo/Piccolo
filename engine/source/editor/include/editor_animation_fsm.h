#pragma once

#include "runtime/function/animation/animation_FSM.h"

#include <imgui.h>
#include <json11.hpp>

#include <string>
#include <vector>

namespace Piccolo
{
    struct AnimationFSMEditorParameter
    {
        std::string   m_name;
        std::string   m_type {"Float"};
        json11::Json  m_default_value;
    };

    struct AnimationFSMEditorState
    {
        int         m_state_id {0};
        std::string m_state_name;
        std::string m_clip_name;
        bool        m_loop {true};
        float       m_editor_x {0.0f};
        float       m_editor_y {0.0f};
    };

    struct AnimationFSMEditorTransition
    {
        int                      m_source {0};
        int                      m_target {0};
        std::vector<FSMCondition> m_conditions;
    };

    class AnimationFSMEditor
    {
    public:
        void open(const std::string& file_path);
        void draw();

    private:
        enum class PendingAction
        {
            None,
            OpenFile,
            ReloadFile
        };

        void drawToolbar();
        void drawSidePanel();
        void drawGraph();
        void drawInspector();
        void drawValidationErrors();
        void drawDiscardPopup();

        bool openFileNow(const std::string& file_path);
        bool reloadFile(const std::string& file_path);
        bool parseDocument(const json11::Json& root);
        json11::Json buildDocumentJson() const;
        bool saveDocument();
        void validateDocument();

        bool drawParameterCondition(FSMCondition& condition, int condition_index);
        bool drawParameterCombo(const char* label, std::string& parameter_name);
        bool drawOperatorCombo(const char* label, CompareOperator& op, const std::string& parameter_type);
        bool drawConditionValue(const char* label, const std::string& parameter_type, json11::Json& value);

        void addState();
        void deleteSelectedState();
        void addTransition(int source, int target);
        void deleteSelectedTransition();
        void normalizeAfterStateDelete(int deleted_index);
        void markDirty();

        std::string getParameterType(const std::string& parameter_name) const;
        int         findParameterIndex(const std::string& parameter_name) const;
        std::string getOperatorName(CompareOperator op) const;
        void        applyDefaultValue(FSMCondition& condition, const std::string& parameter_type);

        ImVec2 canvasToScreen(const ImVec2& canvas_origin, const ImVec2& canvas_size, const ImVec2& point) const;
        ImVec2 screenToCanvas(const ImVec2& canvas_origin, const ImVec2& canvas_size, const ImVec2& point) const;
        ImVec2 getStateCenter(const ImVec2& canvas_origin, const ImVec2& canvas_size, int state_index) const;
        void   drawTransition(ImDrawList*       draw_list,
                              const ImVec2&     canvas_origin,
                              const ImVec2&     canvas_size,
                              int               transition_index,
                              bool              selected) const;
        int    hitTestTransition(const ImVec2& mouse_position,
                                 const ImVec2& canvas_origin,
                                 const ImVec2& canvas_size) const;

    private:
        bool                                                     m_open {false};
        bool                                                     m_dirty {false};
        bool                                                     m_linking_transition {false};
        bool                                                     m_discard_popup_requested {false};
        bool                                                     m_has_validation_result {false};
        int                                                      m_transition_source {-1};
        int                                                      m_dragged_state {-1};
        int                                                      m_selected_state {-1};
        int                                                      m_selected_transition {-1};
        int                                                      m_initial_state_id {0};
        float                                                    m_zoom {1.0f};
        float                                                    m_left_panel_width {250.0f};
        float                                                    m_right_panel_width {320.0f};
        ImVec2                                                   m_canvas_offset {40.0f, 40.0f};
        std::string                                              m_file_path;
        std::string                                              m_pending_file_path;
        std::vector<AnimationFSMEditorParameter>                 m_parameters;
        std::vector<AnimationFSMEditorState>                     m_states;
        std::vector<AnimationFSMEditorTransition>                m_transitions;
        std::vector<std::string>                                 m_validation_errors;
        PendingAction                                            m_pending_action {PendingAction::None};
    };
} // namespace Piccolo
