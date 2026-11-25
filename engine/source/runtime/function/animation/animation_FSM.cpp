#include "runtime/resource/res_type/components/animation.h"
#include "runtime/function/animation/animation_FSM.h"
#include "runtime/function/animation/animation_fsm_validation.h"

#include "runtime/core/base/macro.h"

#include <iostream>
namespace Piccolo
{
    bool AnimationFSM::DeserializeFSM(const json11::Json& fsm_json, AnimationFSM& fsm)
    {
       const AnimationFSMValidationResult validation = validateAnimationFSMJson(fsm_json);
	   if (!validation.isValid())
	   {
		   for (const std::string& error : validation.m_errors)
		   {
			   LOG_ERROR("Animation FSM validation: {}", error);
		   }
		   fsm.m_nodes.clear();
		   fsm.m_transitions.clear();
		   fsm.m_current_state_idx = 0;
		   return false;
	   }

	   fsm.m_nodes.clear();
	   fsm.m_transitions.clear();
	   fsm.m_current_state_idx = fsm_json["initial_state_id"].int_value();

	   const auto& nodes = fsm_json["states"].array_items();
	   const auto& transitions = fsm_json["transitions"].array_items();

	   fsm.m_nodes.resize(nodes.size());
       fsm.m_transitions.resize(transitions.size());

       for (size_t i = 0; i < nodes.size(); ++i) {
           auto& node = fsm.m_nodes[i];
           node.state_id = nodes[i]["state_id"].int_value();
           node.state_name = nodes[i]["state_name"].string_value();
           node.clip_name = nodes[i]["clip_name"].string_value();
           node.loop = nodes[i]["loop"].bool_value();
           auto& node_trans = nodes[i]["transitions"].array_items(); 
		   node.transitions.resize(node_trans.size());
           for (size_t k = 0; k < node_trans.size(); ++k) {
               node.transitions[k] = node_trans[k].int_value();
           }
       }

       for (size_t i = 0; i < transitions.size(); ++i) {
		   auto& transition = fsm.m_transitions[i];
           transition.source = transitions[i]["source"].int_value();
           transition.target = transitions[i]["target"].int_value();
		   transition.transition_duration = transitions[i]["transition_duration"].number_value();
		   auto& trans_conditions = transitions[i]["conditions"].array_items();
           transition.conditions.resize(trans_conditions.size());
           for (size_t k = 0; k < trans_conditions.size(); ++k) {
               auto& condition_json = trans_conditions[k];
               FSMCondition condition;
               condition.parameter_name = condition_json["parameter_name"].string_value();
               std::string op_str = condition_json["op"].string_value();
               if (op_str == "Equal") {
                   condition.op = CompareOperator::Equal;
               } else if (op_str == "NotEqual") {
                   condition.op = CompareOperator::NotEqual;
               } else if (op_str == "Greater") {
                   condition.op = CompareOperator::Greater;
               } else if (op_str == "Less") {
                   condition.op = CompareOperator::Less;
               } else if (op_str == "GreaterEqual") {
                   condition.op = CompareOperator::GreaterEqual;
               } else if (op_str == "LessEqual") {
                   condition.op = CompareOperator::LessEqual;
               }
               condition.value = condition_json["value"];
               transition.conditions[k] = condition;
		   }
       }
	   return true;
    }

    AnimationFSM::AnimationFSM() {}
    float tryGetFloat(const json11::Json::object& json, const std::string& key, float default_value)
    {
        auto found_iter = json.find(key);
        if (found_iter != json.end() && found_iter->second.is_number())
        {
            return found_iter->second.number_value();
        }
        return default_value;
    }
    bool tryGetBool(const json11::Json::object& json, const std::string& key, float default_value)
    {
        auto found_iter = json.find(key);
        if (found_iter != json.end() && found_iter->second.is_bool())
        {
            return found_iter->second.bool_value();
        }
        return default_value;
    }
    bool AnimationFSM::update(const json11::Json::object& signals)
    {
        //States last_state     = m_state;
        bool   is_clip_finish = tryGetBool(signals, "clip_finish", false);
        bool   is_jumping     = tryGetBool(signals, "jumping", false);
        float  speed          = tryGetFloat(signals, "speed", 0);
        bool   is_moving      = speed > 0.01f;
        bool   start_walk_end = false;

        //switch (m_state)
        //{
        //    case States::_idle:
        //        /**** [0] ****/
        //        if (is_jumping) {
        //            m_state = States::_jump_start_from_idle;
        //        }
        //        else if (is_moving) {
        //            m_state = States::_walk_start;
        //        }
        //        break;
        //    case States::_walk_start:
        //        /**** [1] ****/
        //        if (is_clip_finish) {
        //            m_state = States::_walk_run;
        //        }
        //        break;
        //    case States::_walk_run:
        //        /**** [2] ****/
        //        if (is_jumping) {
        //            m_state = States::_jump_start_from_walk_run;
        //        }
        //        else if (start_walk_end && is_clip_finish) {
        //            m_state = States::_walk_stop;
        //        }
        //        break;
        //    case States::_walk_stop:
        //        /**** [3] ****/
        //        if (!is_moving && is_clip_finish) {
        //            m_state = States::_idle;
        //        }
        //        break;
        //    case States::_jump_start_from_idle:
        //        /**** [4] ****/
        //        if (is_clip_finish) {
        //            m_state = States::_jump_loop_from_idle;
        //        }
        //        break;
        //    case States::_jump_loop_from_idle:
        //        /**** [5] ****/
        //        if (!is_jumping) {
        //            m_state = States::_jump_end_from_idle;
        //        }
        //        break;
        //    case States::_jump_end_from_idle:
        //        /**** [6] ****/
        //        if (is_clip_finish) {
        //            m_state = States::_idle;
        //        }
        //        break;
        //    case States::_jump_start_from_walk_run:
        //        /**** [7] ****/
        //        if (is_clip_finish) {
        //            m_state = States::_jump_loop_from_walk_run;
        //        }
        //        break;
        //    case States::_jump_loop_from_walk_run:
        //        /**** [8] ****/
        //        if (!is_jumping) {
        //            m_state = States::_jump_end_from_idle;
        //        }
        //        break;
        //    case States::_jump_end_from_walk_run:
        //        /**** [9] ****/
        //        if (is_clip_finish) {
        //            m_state = States::_walk_run;
        //        }
        //        break;
        //    default:
        //        break;
        //}
        return false;
    }

    bool AnimationFSM::update_fsm(const json11::Json::object& signals, FSMUpdateRes& out_res)
    {
        const FSMNode& cur_node = getCurrentNode();
        
        out_res.src_id = cur_node.state_id;
        out_res.target_id = cur_node.state_id;

        for (const auto& transition_id : cur_node.transitions) {
            if (transition_id >= m_transitions.size()) {
                throw std::out_of_range("Transition ID is out of range of transitions.");
            }

			const FSMTransition& transition = m_transitions[transition_id];
            if (transition.checkCondition(signals)) {

				//cur_node.ExitNode(); // Exit the current node
                m_current_state_idx = transition.target;
                
                //getCurrentNode().EnterNode(); // Enter the new node

                out_res.target_id = m_current_state_idx;
				out_res.transition_id = transition_id;
                
                return true; // Transition occurred
			}
            else {
				//cur_node.UpdateNode(); // Update the current node if no transition occurs
            }
        }

        return false;
    }

    Reflection::ReflectionPtr<ClipBase> AnimationFSM::getCurrentClipBase(const AnimationComponentRes& animation_res) const
    {
        const auto& cur_node = getCurrentNode();
        for (auto clip : animation_res.m_clips) {
            if (cur_node.clip_name == clip->m_name)
            {
                return clip;
            }
        }
        return Reflection::ReflectionPtr<ClipBase>();
    }

    std::string AnimationFSM::getCurrentClipBaseName() const
    {
        if (!hasValidState())
        {
            return {};
        }
        return getCurrentNode().clip_name;
        //switch (m_state)
        //{
        //    case States::_idle:
        //        return "idle_walk_run";
        //    case States::_walk_start:
        //        return "walk_start";
        //    case States::_walk_run:
        //        return "idle_walk_run";
        //    case States::_walk_stop:
        //        return "walk_stop";
        //    case States::_jump_start_from_walk_run:
        //    case States::_jump_start_from_idle:
        //        return "jump_start";
        //    case States::_jump_loop_from_walk_run:
        //    case States::_jump_loop_from_idle:
        //        return "jump_loop";
        //    case States::_jump_end_from_walk_run:
        //    case States::_jump_end_from_idle:
        //        return "jump_stop";
        //    default:
        //        return "idle_walk_run";
        //}
    }
}

