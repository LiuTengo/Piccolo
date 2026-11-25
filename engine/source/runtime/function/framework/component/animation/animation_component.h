#pragma once

#include "runtime/function/animation/skeleton.h"
#include "runtime/function/animation/pose.h"
#include "runtime/function/framework/component/component.h"
#include "runtime/resource/res_type/components/animation.h"
#include "runtime/function/animation/animation_FSM.h"
#include "json11.hpp"
namespace Piccolo
{
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
        void                   animateBasicClip(float ratio, BasicClip* basic_clip);
        void                   blend(float desired_ratio, BlendState* blend_state);
        void                   blend1D(float desired_ratio, BlendSpace1D* blend_state);
		
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
        float                 m_elapsed_time{0};
    };
} // namespace Piccolo
