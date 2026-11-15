#pragma once
#include "runtime/core/meta/reflection/reflection.h"
#include "runtime/resource/res_type/data/blend_state.h"

#include <functional>
#include <json11.hpp>
#include <vector>
namespace Piccolo
{
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
        CompareOperator op;
        json11::Json value;
    };

    struct FSMTransition{
        uint16_t source;
        uint16_t target;

		std::vector<FSMCondition> conditions;
    };

    struct FSMNode {
        std::string state_name; //暂时不考虑子节点的事，先做一个简单的FSM
        std::string clip_path;
        uint16_t state_id;
        std::vector<uint16_t> transitions;
    };

    class AnimationFSM
    {
        std::vector<FSMNode> m_nodes;
        std::vector<FSMTransition> m_transitions;

    public:
		//从json文件中反序列化FSM
		static void DeserializeFSM(const json11::Json& fsm_json, AnimationFSM& fsm);

        AnimationFSM();
        bool        update(const json11::Json::object& signals);
        std::string getCurrentClipBaseName() const;

        bool update_fsm(const json11::Json::object& signals);
        Reflection::ReflectionPtr<ClipBase> getCurrentClipBase() const;
    
    private:
        bool checkCondition(const json11::Json::object& signals,const FSMCondition& condition) const;
    };
} // namespace Piccolo