#pragma once
#include "runtime/core/meta/reflection/reflection.h"
#include "runtime/resource/res_type/data/blend_state.h"

#include <functional>
#include <json11.hpp>
#include <vector>
namespace Piccolo
{
    class AnimationComponentRes;

    enum class CompareOperator
    {
        Equal,
        NotEqual,
        Greater,
        Less,
        GreaterEqual,
        LessEqual
	};

    struct FSMCondition {
        std::string parameter_name;
        CompareOperator op {CompareOperator::Equal};
        json11::Json value;
    };

    struct FSMTransition{
        uint16_t source;
        uint16_t target;
		float transition_duration{ 0.15f };

		std::vector<FSMCondition> conditions;

        bool checkCondition(const json11::Json::object& signals) const {
            for (const auto& condition : conditions) {
				// Check if the parameter exists in the signals
                // If it doesn't exist, the condition fails
                auto found_iter = signals.find(condition.parameter_name);
                if (found_iter == signals.end()) {
                    return false;
				}

                const json11::Json& signal_value = found_iter->second;
                switch (condition.op) {
                case CompareOperator::Equal:
                    if (signal_value != condition.value) return false;
                    break;
                case CompareOperator::NotEqual:
                    if (signal_value == condition.value) return false;
                    break;
                case CompareOperator::Greater:
                    if (!signal_value.is_number() || !condition.value.is_number() || signal_value.number_value() <= condition.value.number_value()) return false;
                    break;
                case CompareOperator::Less:
                    if (!signal_value.is_number() || !condition.value.is_number() || signal_value.number_value() >= condition.value.number_value()) return false;
                    break;
                case CompareOperator::GreaterEqual:
                    if (!signal_value.is_number() || !condition.value.is_number() || signal_value.number_value() < condition.value.number_value()) return false;
                    break;
                case CompareOperator::LessEqual:
                    if (!signal_value.is_number() || !condition.value.is_number() || signal_value.number_value() > condition.value.number_value()) return false;
                    break;
                }
            }
            return true;
        }
    };

    struct FSMNode {
        std::string state_name; //暂时不考虑子节点的事，先做一个简单的FSM
        std::string clip_name;
        uint16_t state_id;
        bool loop = true;
        std::vector<uint16_t> transitions;

		// Index of the associated clip in the AnimationFSM's clip list
        int32_t clip_index = -1;
    };

    struct FSMUpdateRes {
        uint16_t src_id {0};
        uint16_t target_id {0};
		uint16_t transition_id{ 0 };
    };

    class AnimationFSM
    {
    public:
        std::vector<FSMNode> m_nodes;
        std::vector<FSMTransition> m_transitions;

	protected:
		uint16_t m_current_state_idx = 0;

    public:
		//从json文件中反序列化FSM
		static bool DeserializeFSM(const json11::Json& fsm_json, AnimationFSM& fsm);

        AnimationFSM();
        //deprecated function
        bool        update(const json11::Json::object& signals);
        std::string getCurrentClipBaseName() const;

        bool update_fsm(const json11::Json::object& signals, FSMUpdateRes& res);
        Reflection::ReflectionPtr<ClipBase> getCurrentClipBase(const AnimationComponentRes& animation_res) const;
        bool hasValidState() const { return !m_nodes.empty() && m_current_state_idx < m_nodes.size(); }

        const FSMNode& getCurrentNode() const {
            if (m_current_state_idx >= m_nodes.size()) {
                throw std::out_of_range("Current state ID is out of range of nodes.");
			}
            return m_nodes[m_current_state_idx];
        }
    };
} // namespace Piccolo
