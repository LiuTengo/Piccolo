#pragma once

#include <json11.hpp>

#include <string>
#include <vector>

namespace Piccolo
{
    struct AnimationFSMValidationResult
    {
        std::vector<std::string> m_errors;

        bool isValid() const { return m_errors.empty(); }
    };

    AnimationFSMValidationResult validateAnimationFSMJson(const json11::Json& fsm_json);
} // namespace Piccolo
