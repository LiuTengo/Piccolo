#include "runtime/function/animation/animation_fsm_validation.h"

#include <algorithm>
#include <cstdint>
#include <limits>
#include <map>
#include <set>
#include <utility>

namespace Piccolo
{
    namespace
    {
        void addError(AnimationFSMValidationResult& result, std::string message)
        {
            result.m_errors.push_back(std::move(message));
        }

        bool isKnownOperator(const std::string& op)
        {
            return op == "Equal" || op == "NotEqual" || op == "Greater" || op == "Less" ||
                   op == "GreaterEqual" || op == "LessEqual";
        }

        bool isBoolOperator(const std::string& op)
        {
            return op == "Equal" || op == "NotEqual";
        }

        bool isUint16Integer(const json11::Json& value)
        {
            if (!value.is_number())
                return false;

            const double number = value.number_value();
            return number >= 0.0 &&
                   number <= static_cast<double>(std::numeric_limits<uint16_t>::max()) &&
                   static_cast<double>(static_cast<uint16_t>(number)) == number;
        }
    } // namespace

    AnimationFSMValidationResult validateAnimationFSMJson(const json11::Json& fsm_json)
    {
        AnimationFSMValidationResult result;

        if (!fsm_json.is_object())
        {
            addError(result, "FSM root must be a JSON object.");
            return result;
        }

        if (fsm_json["version"].is_number() && fsm_json["version"].int_value() < 1)
        {
            addError(result, "FSM version must be greater than or equal to 1.");
        }

        if (!fsm_json["states"].is_array())
        {
            addError(result, "FSM field 'states' must be an array.");
            return result;
        }
        if (!fsm_json["transitions"].is_array())
        {
            addError(result, "FSM field 'transitions' must be an array.");
            return result;
        }

        const auto& states      = fsm_json["states"].array_items();
        const auto& transitions = fsm_json["transitions"].array_items();

        if (states.empty())
        {
            addError(result, "FSM must contain at least one state.");
            return result;
        }

        const int initial_state_id = fsm_json["initial_state_id"].int_value();

        std::map<std::string, std::string> parameter_types {
            {"speed", "Float"},
            {"jumping", "Bool"},
            {"clip_finish", "Bool"},
        };

        if (!fsm_json["parameters"].is_null() && !fsm_json["parameters"].is_array())
        {
            addError(result, "FSM field 'parameters' must be an array.");
        }
        else if (fsm_json["parameters"].is_array())
        {
            std::set<std::string> parameter_names;
            const auto&           parameters = fsm_json["parameters"].array_items();
            for (size_t parameter_index = 0; parameter_index < parameters.size(); ++parameter_index)
            {
                const auto& parameter = parameters[parameter_index];
                if (!parameter.is_object())
                {
                    addError(result,
                             "Parameter " + std::to_string(parameter_index) + " must be a JSON object.");
                    continue;
                }

                const std::string name = parameter["name"].string_value();
                const std::string type = parameter["type"].string_value();
                if (!parameter["name"].is_string() || name.empty())
                {
                    addError(result, "Parameter " + std::to_string(parameter_index) + " has an empty name.");
                    continue;
                }
                if (!parameter_names.insert(name).second)
                {
                    addError(result, "Duplicate parameter name '" + name + "'.");
                }
                if (type != "Bool" && type != "Float" && type != "Int")
                {
                    addError(result, "Parameter '" + name + "' has unsupported type '" + type + "'.");
                    continue;
                }
                parameter_types[name] = type;

                const json11::Json& default_value = parameter["default_value"];
                if (!default_value.is_null())
                {
                    if ((type == "Bool" && !default_value.is_bool()) ||
                        ((type == "Float" || type == "Int") && !default_value.is_number()))
                    {
                        addError(result, "Parameter '" + name + "' has a default value with the wrong type.");
                    }
                }
            }
        }

        std::set<std::string>              state_names;
        std::set<int>                      state_ids;
        std::map<int, std::set<int>>       expected_transitions;

        for (size_t state_index = 0; state_index < states.size(); ++state_index)
        {
            const auto& state = states[state_index];
            if (!state.is_object())
            {
                addError(result, "State " + std::to_string(state_index) + " must be a JSON object.");
                continue;
            }

            int  state_id       = -1;
            bool state_id_valid = false;
            if (!isUint16Integer(state["state_id"]))
            {
                addError(result,
                         "State " + std::to_string(state_index) +
                             " requires an integer state_id in the range 0..65535.");
            }
            else
            {
                state_id = state["state_id"].int_value();
                state_id_valid = true;
                if (!state_ids.insert(state_id).second)
                {
                    addError(result, "Duplicate state_id " + std::to_string(state_id) + ".");
                }
            }

            const std::string state_name = state["state_name"].string_value();
            if (!state["state_name"].is_string() || state_name.empty())
            {
                addError(result, "State " + std::to_string(state_index) + " has an empty name.");
            }
            else if (!state_names.insert(state_name).second)
            {
                addError(result, "Duplicate state name '" + state_name + "'.");
            }

            if (!state["clip_name"].is_string() || state["clip_name"].string_value().empty())
            {
                addError(result, "State '" + state_name + "' has an empty clip_name.");
            }

            if (!state["loop"].is_bool())
            {
                addError(result, "State '" + state_name + "' field 'loop' must be a bool.");
            }

            if (!state["transitions"].is_array())
            {
                addError(result, "State '" + state_name + "' field 'transitions' must be an array.");
                continue;
            }

            for (const json11::Json& transition_id_json : state["transitions"].array_items())
            {
                if (!isUint16Integer(transition_id_json))
                {
                    addError(result,
                             "State '" + state_name +
                                 "' contains a transition ID that is not an integer in the range 0..65535.");
                    continue;
                }

                const int transition_id = transition_id_json.int_value();
                if (!state_id_valid)
                    continue;

                if (!expected_transitions[state_id].insert(transition_id).second)
                {
                    addError(result,
                             "State '" + state_name + "' contains duplicate transition ID " +
                                 std::to_string(transition_id) + ".");
                }
            }
        }

        if (!isUint16Integer(fsm_json["initial_state_id"]))
        {
            addError(result,
                     "FSM field 'initial_state_id' is required and must be an integer in the range 0..65535.");
        }
        else if (state_ids.find(initial_state_id) == state_ids.end())
        {
            addError(result, "Initial state ID " + std::to_string(initial_state_id) + " does not exist.");
        }

        std::set<int>        transition_ids;
        std::map<int, int>   transition_sources;

        for (size_t transition_index = 0; transition_index < transitions.size(); ++transition_index)
        {
            const auto& transition = transitions[transition_index];
            if (!transition.is_object())
            {
                addError(result,
                         "Transition " + std::to_string(transition_index) + " must be a JSON object.");
                continue;
            }

            if (!isUint16Integer(transition["transition_id"]) ||
                !isUint16Integer(transition["source"]) ||
                !isUint16Integer(transition["target"]))
            {
                addError(result,
                         "Transition " + std::to_string(transition_index) +
                             " requires integer transition_id, source and target fields "
                             "in the range 0..65535.");
                continue;
            }

            if (transition["transition_duration"].is_number() &&
                transition["transition_duration"].number_value() < 0.0)
            {
                addError(result,
                         "Transition " + std::to_string(transition_index) +
                             " has a negative transition_duration.");
            }

            const int transition_id = transition["transition_id"].int_value();
            if (!transition_ids.insert(transition_id).second)
            {
                addError(result, "Duplicate transition_id " + std::to_string(transition_id) + ".");
            }

            const int source = transition["source"].int_value();
            const int target = transition["target"].int_value();
            if (state_ids.find(source) == state_ids.end())
            {
                addError(result,
                         "Transition " + std::to_string(transition_index) + " has invalid source " +
                             std::to_string(source) + ".");
                continue;
            }
            transition_sources[transition_id] = source;
            if (state_ids.find(target) == state_ids.end())
            {
                addError(result,
                         "Transition " + std::to_string(transition_index) + " has invalid target " +
                             std::to_string(target) + ".");
            }

            if (expected_transitions[source].find(transition_id) == expected_transitions[source].end())
            {
                addError(result,
                         "Transition " + std::to_string(transition_index) + " has source " +
                             std::to_string(source) + " but is not listed in that state's transitions.");
            }

            if (!transition["conditions"].is_array())
            {
                addError(result,
                         "Transition " + std::to_string(transition_index) +
                             " field 'conditions' must be an array.");
                continue;
            }

            const auto& conditions = transition["conditions"].array_items();
            for (size_t condition_index = 0; condition_index < conditions.size(); ++condition_index)
            {
                const auto& condition = conditions[condition_index];
                if (!condition.is_object())
                {
                    addError(result,
                             "Transition " + std::to_string(transition_index) + " condition " +
                                 std::to_string(condition_index) + " must be an object.");
                    continue;
                }

                const std::string parameter_name = condition["parameter_name"].string_value();
                const std::string op             = condition["op"].string_value();
                if (!condition["parameter_name"].is_string() || parameter_name.empty())
                {
                    addError(result,
                             "Transition " + std::to_string(transition_index) + " condition " +
                                 std::to_string(condition_index) + " has an empty parameter name.");
                    continue;
                }

                const auto parameter_type = parameter_types.find(parameter_name);
                if (parameter_type == parameter_types.end())
                {
                    addError(result,
                             "Transition " + std::to_string(transition_index) + " references unknown parameter '" +
                                 parameter_name + "'.");
                }

                if (!isKnownOperator(op))
                {
                    addError(result,
                             "Condition for parameter '" + parameter_name + "' has unknown operator '" + op + "'.");
                    continue;
                }

                if (condition["value"].is_null())
                {
                    addError(result, "Condition for parameter '" + parameter_name + "' is missing value.");
                    continue;
                }

                if (parameter_type != parameter_types.end())
                {
                    if (parameter_type->second == "Bool")
                    {
                        if (!condition["value"].is_bool())
                        {
                            addError(result,
                                     "Condition for bool parameter '" + parameter_name +
                                         "' requires a bool value.");
                        }
                        if (!isBoolOperator(op))
                        {
                            addError(result,
                                     "Bool parameter '" + parameter_name + "' only supports Equal and NotEqual.");
                        }
                    }
                    else if (!condition["value"].is_number())
                    {
                        addError(result,
                                 "Condition for numeric parameter '" + parameter_name +
                                     "' requires a numeric value.");
                    }
                }
            }
        }

        for (const auto& state_transitions : expected_transitions)
        {
            for (int transition_id : state_transitions.second)
            {
                if (transition_ids.find(transition_id) == transition_ids.end())
                {
                    addError(result,
                             "State ID " + std::to_string(state_transitions.first) +
                                 " references missing transition ID " + std::to_string(transition_id) + ".");
                    continue;
                }

                const auto source_iter = transition_sources.find(transition_id);
                if (source_iter != transition_sources.end() &&
                    source_iter->second != state_transitions.first)
                {
                    addError(result,
                             "Transition ID " + std::to_string(transition_id) + " is listed by state ID " +
                                 std::to_string(state_transitions.first) + " but its source is state ID " +
                                 std::to_string(source_iter->second) + ".");
                }
            }
        }

        return result;
    }
} // namespace Piccolo
