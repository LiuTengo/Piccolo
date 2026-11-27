#pragma once

#include "runtime/function/animation/skeleton.h"
#include "runtime/function/animation/pose.h"
#include "runtime/function/framework/component/component.h"
#include "runtime/resource/res_type/components/animation.h"
#include "runtime/function/animation/animation_FSM.h"
#include "json11.hpp"

#include <string>
#include <utility>
#include <vector>

namespace Piccolo
{
    struct AnimationConditionDebugInfo
    {
        std::string m_parameter_name;
        std::string m_operator_name;
        std::string m_expected_value;
        std::string m_actual_value;
        bool        m_parameter_exists {false};
        bool        m_matched {false};
    };

    struct AnimationTransitionDebugInfo
    {
        int         m_transition_id {0};
        uint16_t    m_source_state_id {0};
        uint16_t    m_target_state_id {0};
        std::string m_target_state_name;
        float       m_transition_duration {0.0f};
        bool        m_matched {false};
        std::vector<AnimationConditionDebugInfo> m_conditions;
    };

    struct AnimationDebugInfo
    {
        bool        m_valid {false};
        uint16_t    m_state_id {0};
        std::string m_state_name;
        std::string m_clip_name;
        bool        m_loop {false};
        float       m_playback_ratio {0.0f};
        bool        m_transition_active {false};
        float       m_transition_alpha {1.0f};
        float       m_transition_elapsed {0.0f};
        float       m_transition_duration {0.0f};
        std::vector<std::pair<std::string, std::string>> m_signals;
        std::vector<AnimationTransitionDebugInfo>        m_transitions;
    };

    REFLECTION_TYPE(AnimationComponent)
    CLASS(AnimationComponent : public Component, WhiteListFields)
    {
        REFLECTION_BODY(AnimationComponent)

    public:
        AnimationComponent() = default;

        void postLoadResource(std::weak_ptr<GObject> parent_object) override;

        void tick(float delta_time) override;

        void parameter_update(float delta_tick);

        const AnimationResult& getResult() const;
        AnimationDebugInfo     buildDebugInfo() const;
        AnimationPose                   animateBasicClip(float ratio, BasicClip* basic_clip);
        AnimationPose                   blend(float desired_ratio, BlendState* blend_state);
        AnimationPose                   blend1D(float desired_ratio, BlendSpace1D* blend_state);
		
        void evaluateStatePose(Piccolo::Reflection::ReflectionPtr<ClipBase> clip, float ratio, AnimationPose& out_pose);
        void crossfade(const AnimationPose& src_pose, const AnimationPose& target_pose, float alpha, AnimationPose& out_pose);

        template<typename T>
        void updateSignal(const std::string& key, const T& value)
        {
            m_signal[key] = value;
        }

        const Skeleton& getSkeleton() const { return m_skeleton; }

    protected:
        bool loadAnimationFSM();

        META(Enable)
        AnimationComponentRes m_animation_res;

        Skeleton m_skeleton;
        AnimationResult       m_animation_result;
        AnimationFSM          m_animation_fsm;
        json11::Json          m_animation_fsm_json;
        json11::Json::object  m_signal;
        float                 m_ratio {0};
        AnimationPose         src_pose; 
        bool                  m_transition_active{false};

        float                 m_elapsed_time{1000.0f};
		float 			      m_cross_duration{ 0.15f };
        bool                  m_debug_crossfade_active {false};
        uint16_t              m_debug_crossfade_source_state_id {0};
        uint16_t              m_debug_crossfade_target_state_id {0};
    };
} // namespace Piccolo
