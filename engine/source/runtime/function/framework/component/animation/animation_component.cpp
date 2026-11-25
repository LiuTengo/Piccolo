#include "runtime/function/framework/component/animation/animation_component.h"

#include "runtime/function/animation/animation_system.h"
#include "runtime/function/framework/object/object.h"
#include "runtime/function/framework/component/motor/motor_component.h"
#include <runtime/engine.h>

#include "runtime/core/base/macro.h"
#include "runtime/function/global/global_context.h"
#include "runtime/resource/asset_manager/asset_manager.h"

#include <fstream>
#include <sstream>

namespace Piccolo
{
    void AnimationComponent::postLoadResource(std::weak_ptr<GObject> parent_object)
    {
        m_parent_object = parent_object;

        auto skeleton_res = AnimationManager::tryLoadSkeleton(m_animation_res.m_skeleton_file_path);
        m_skeleton.buildSkeleton(*skeleton_res);

        // 使用 AssetManager 将资源相对路径解析为引擎配置的完整路径。
        if (loadAnimationFSM())
        {
            if (!AnimationFSM::DeserializeFSM(m_animation_fsm_json, m_animation_fsm))
            {
                LOG_ERROR("Animation FSM could not be initialized.");
            }
        }
    }

    bool AnimationComponent::loadAnimationFSM()
    {
        m_animation_fsm_json = json11::Json();

        if (m_animation_res.m_fsm_file_path.empty())
        {
            LOG_ERROR("Animation FSM file path is empty.");
            return false;
        }

        const std::filesystem::path fsm_file_path =
            g_runtime_global_context.m_asset_manager->getFullPath(m_animation_res.m_fsm_file_path);

        std::ifstream fsm_file(fsm_file_path);
        if (!fsm_file.is_open())
        {
            LOG_ERROR("Open animation FSM file failed: {}", fsm_file_path.generic_string());
            return false;
        }

        std::stringstream buffer;
        buffer << fsm_file.rdbuf();

        std::string   parse_error;
        json11::Json parsed_json = json11::Json::parse(buffer.str(), parse_error);
        if (!parse_error.empty())
        {
            LOG_ERROR("Parse animation FSM file failed: {} ({})", fsm_file_path.generic_string(), parse_error);
            return false;
        }

        if (!parsed_json.is_object())
        {
            LOG_ERROR("Animation FSM root must be a JSON object: {}", fsm_file_path.generic_string());
            return false;
        }

        m_animation_fsm_json = parsed_json;
        LOG_INFO("Animation FSM loaded: {}, states={}",
                 fsm_file_path.generic_string(),
                 m_animation_fsm_json["states"].array_items().size());
        return true;
    }

    void AnimationComponent::blend1D(float desired_ratio, BlendSpace1D* blend_state)
    {
        if (blend_state->m_values.size() < 2)
        {
            // no need to interpolate
            return;
        }
        const auto& key_it    = m_signal.find(blend_state->m_key);
        double      key_value = 0;
        if (key_it != m_signal.end())
        {
            if (key_it->second.is_number())
            {
                key_value = key_it->second.number_value();
            }
        }
        int max_smaller = -1;
        for (auto value : blend_state->m_values)
        {
            if (value <= key_value)
            {
                max_smaller++;
            }
            else
            {
                break;
            }
        }

        for (auto& weight : blend_state->m_blend_weight)
        {
            weight = 0;
        }
        if (max_smaller == -1)
        {
            blend_state->m_blend_weight[0] = 1.0f;
        }
        else if (max_smaller == blend_state->m_values.size() - 1)
        {
            blend_state->m_blend_weight[max_smaller] = 1.0f;
        }
        else
        {
            auto l = blend_state->m_values[max_smaller];
            auto r = blend_state->m_values[max_smaller + 1];

            if (l == r)
            {
                // some error
            }

            float weight = (key_value - l) / (r - l);

            blend_state->m_blend_weight[max_smaller + 1] = weight;
            blend_state->m_blend_weight[max_smaller]     = 1 - weight;
        }
        blend(desired_ratio, blend_state);
    }


    AnimationPose evaluateStatePose(Piccolo::Reflection::ReflectionPtr<ClipBase> clip, float ratio, AnimationPose& out_pose)
    {
        //计算当前动画资产的pose
        AnimationPose res_pose;

        return res_pose;
    }
    void crossfade(const AnimationPose& src_pose, const AnimationPose& target_pose, float alpha, AnimationPose& out_pose)
    {

    }
    void AnimationComponent::tick(float delta_time)
    {
        if ((m_tick_in_editor_mode == false) && g_is_editor_mode)
            return;
        if (!m_animation_fsm.hasValidState())
            return;

        // Deprecated Tick Logic
        //std::string name = m_animation_fsm.getCurrentClipBaseName();
        //for (auto blend_state : m_animation_res.m_clips)
        //{
        //    if (blend_state->m_name == name)
        //    {
        //        float length        = blend_state->getLength();
        //        float delta_ratio   = delta_time / length;
        //        float desired_ratio = delta_ratio + m_ratio;
        //        if (desired_ratio >= 1.f)
        //        {
        //            desired_ratio = desired_ratio - floor(desired_ratio);
        //            updateSignal("clip_finish", true);
        //        }
        //        else
        //        {
        //            updateSignal("clip_finish", false);
        //        }
        //        bool restart = m_animation_fsm.update(m_signal);
        //        if (!restart)
        //        {
        //            m_ratio = desired_ratio;
        //        }
        //        else
        //        {
        //            m_ratio = 0;
        //        }
        //        break;
        //    }
        //}
        //name = m_animation_fsm.getCurrentClipBaseName();
        //for (auto clip : m_animation_res.m_clips)
        //{
        //    if (clip->m_name == name)
        //    {
        //        if (clip.getTypeName() == "BlendSpace1D")
        //        {
        //            auto blend_state_1d_pre = static_cast<BlendSpace1D*>(clip);
        //            blend1D(m_ratio, blend_state_1d_pre);
        //        }
        //        else if (clip.getTypeName() == "BlendState")
        //        {
        //            auto blend_state = static_cast<BlendState*>(clip);
        //            blend(m_ratio, blend_state);
        //        }
        //        else if (clip.getTypeName() == "BasicClip")
        //        {
        //            auto basic_clip = static_cast<BasicClip*>(clip);
        //            animateBasicClip(m_ratio, basic_clip);
        //        }
        //        break;
        //    }
        //}
    
        // 参数更新 update parameters
		parameter_update(delta_time);

        //region 暂时的方法 更新一下播放进度
        auto cur_clip = m_animation_fsm.getCurrentClipBase(m_animation_res);
		auto cur_node = m_animation_fsm.getCurrentNode();
        if (!cur_clip)
            return;

        // 3. 更新播放进度
        float length = cur_clip->getLength();

        if (length <= 0.0f)
            return;

        float desired_ratio = m_ratio + delta_time / length;

        bool clip_finished = false;
        if (cur_node.loop)
        {
			// 循环时 ratio 超过1.0f时，取余数
            m_ratio = std::fmod(desired_ratio, 1.0f);
            clip_finished = false;
        }
        else
        {
            // 非循环时 ratio 超过1.0f时，动画结束
            clip_finished = desired_ratio >= 1.0f;
            m_ratio = std::min(desired_ratio, 1.0f);
        }

        updateSignal("clip_finish", clip_finished);
		//endregion 更新播放进度

		// 更新动画状态机 update animation state machine
        FSMUpdateRes fsm_res = FSMUpdateRes();
        bool state_change = m_animation_fsm.update_fsm(m_signal, fsm_res);

        // 5. 根据是否切换状态更新播放进度
        m_ratio = (state_change) ? 0.0f : m_ratio;

        // 拿到当前的Clip 根据ratio获取clip的pose并和当前pose做blend
        auto clip = m_animation_fsm.getCurrentClipBase(m_animation_res);

        AnimationPose src_pose;
        m_skeleton.extractPose(src_pose);
        AnimationPose target_pose;
        evaluateStatePose(clip, m_ratio, target_pose);

        //FIXME: sss
		float duration = m_animation_fsm.m_transitions[fsm_res.transition_id].transition_duration;
		float elapsed_time = delta_time;
		float alpha = std::clamp(elapsed_time/duration, 0.0f, 1.0f);

        AnimationPose final_pose;
        crossfade(src_pose, target_pose, alpha, final_pose);

        m_skeleton.applyPose(final_pose);

        if (clip) {
            if (clip.getTypeName() == "BlendSpace1D")
            {
                auto blend_state_1d_pre = static_cast<BlendSpace1D*>(clip);
                blend1D(m_ratio, blend_state_1d_pre);
            }
            else if (clip.getTypeName() == "BlendState")
            {
                auto blend_state = static_cast<BlendState*>(clip);
                blend(m_ratio, blend_state);
            }
            else if (clip.getTypeName() == "BasicClip")
            {
                auto basic_clip = static_cast<BasicClip*>(clip);
                animateBasicClip(m_ratio, basic_clip);
            }
        }
    
    }

    void AnimationComponent::parameter_update(float delta_tick)
    {
    }

    const AnimationResult& AnimationComponent::getResult() const { return m_animation_result; }

    void AnimationComponent::animateBasicClip(float desired_ratio, BasicClip* basic_clip)
    {
        auto                       clip_data = AnimationManager::getClipData(*basic_clip);

            AnimationPose pose(clip_data.m_clip,
                               desired_ratio,
                               clip_data.m_anim_skel_map);
            m_skeleton.resetSkeleton();
            m_skeleton.applyAdditivePose(pose);
            m_skeleton.extractPose(pose);
        

        m_skeleton.applyPose(pose);
        //m_animation_result = m_skeleton.outputAnimationResult();
    }
    void AnimationComponent::blend(float desired_ratio, BlendState* blend_state)
    {

        for (auto& ratio : blend_state->m_blend_ratio)
        {
            ratio = desired_ratio;
        }
        auto                       blendStateData = AnimationManager::getBlendStateWithClipData(*blend_state);
        std::vector<AnimationPose> poses;
        for (int i = 0; i < blendStateData.m_clip_count; i++)
        {
            AnimationPose pose(blendStateData.m_blend_clip[i],
                               blendStateData.m_blend_weight[i],
                               blendStateData.m_blend_ratio[i],
                               blendStateData.m_blend_anim_skel_map[i]);
            m_skeleton.resetSkeleton();
            m_skeleton.applyAdditivePose(pose);
            m_skeleton.extractPose(pose);
            poses.push_back(pose);
        }
        for (int i = 1; i < blendStateData.m_clip_count; i++)
        {
            for (auto& pose : poses[i].m_weight.m_blend_weight)
            {
                pose = blend_state->m_blend_weight[i];
            }
            poses[0].blend(poses[i]);
        }

        m_skeleton.applyPose(poses[0]);
        //m_animation_result = m_skeleton.outputAnimationResult();
    }
} // namespace Pilot
