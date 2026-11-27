#pragma once
#include "runtime/core/meta/reflection/reflection.h"
#include "runtime/resource/res_type/data/blend_state.h"

#include <functional>
#include <json11.hpp>
#include <unordered_map>
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

        bool evaluate(const json11::Json& signal_value) const {
            switch (op) {
            case CompareOperator::Equal:
                return signal_value == value;
            case CompareOperator::NotEqual:
                return signal_value != value;
            case CompareOperator::Greater:
                return signal_value.is_number() && value.is_number() &&
                       signal_value.number_value() > value.number_value();
            case CompareOperator::Less:
                return signal_value.is_number() && value.is_number() &&
                       signal_value.number_value() < value.number_value();
            case CompareOperator::GreaterEqual:
                return signal_value.is_number() && value.is_number() &&
                       signal_value.number_value() >= value.number_value();
            case CompareOperator::LessEqual:
                return signal_value.is_number() && value.is_number() &&
                       signal_value.number_value() <= value.number_value();
            }
            return false;
        }
    };

    struct FSMTransition{
		uint16_t transition_id {0};
        uint16_t source {0};
        uint16_t target {0};
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

                if (!condition.evaluate(found_iter->second)) return false;
            }
            return true;
        }
    };

    struct FSMNode {
        std::string state_name; //暂时不考虑子节点的事，先做一个简单的FSM
        std::string clip_name;
        uint16_t state_id {0};
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
        std::unordered_map<uint16_t, FSMNode> m_nodes;
        std::unordered_map<uint16_t, FSMTransition> m_transitions;

	protected:
		uint16_t m_current_state_id = 0;

    public:
		//从json文件中反序列化FSM
		static bool DeserializeFSM(const json11::Json& fsm_json, AnimationFSM& fsm);

        AnimationFSM();
        //deprecated function
        bool        update(const json11::Json::object& signals);
        std::string getCurrentClipBaseName() const;

        bool update_fsm(const json11::Json::object& signals, FSMUpdateRes& res);
        Reflection::ReflectionPtr<ClipBase> getCurrentClipBase(const AnimationComponentRes& animation_res) const;
        bool hasValidState() const { return !m_nodes.empty() && m_nodes.find(m_current_state_id) != m_nodes.end(); }

        const FSMNode& getCurrentNode() const {
            if (m_nodes.find(m_current_state_id) == m_nodes.end()) {
                throw std::out_of_range("Current state ID is out of range of nodes.");
			}
            return m_nodes.at(m_current_state_id);
        }
    };
} // namespace Piccolo
