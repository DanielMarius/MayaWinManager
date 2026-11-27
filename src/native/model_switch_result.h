#pragma once
#include "http.h"

inline bool ModelSwitchEnabled(const Json &model, bool blockedByWork, bool inFlight)
{
    return model.is_object() && model.value("ok", false) && !model.value("running", false) &&
           !blockedByWork && !inFlight;
}

inline bool ModelSwitchRestarts(const Json &response)
{
    return response.value("restarts", false);
}

inline std::string ModelSwitchConfirmationText(const Json &target, const std::string &current,
                                               bool restarts, bool answerActive)
{
    if (!target.is_object()) return "Select a valid model before switching.";
    const auto field = [&](const char *key) {
        return target.contains(key) && target[key].is_string() ? target[key].get<std::string>() : std::string();
    };
    auto name = field("model");
    const auto quant = field("quant");
    if (!quant.empty()) name += (name.empty() ? "" : " ") + quant;
    if (name.empty()) name = "the selected model";
    auto message = "Switch to " + name + "?";
    if (!restarts)
        return message + " Maya saved it to apply at next start because this server cannot restart itself. Win-Manager will restart Maya now to load it; API clients may need to retry while Maya returns.";

    message += " Maya stops " + (current.empty() ? std::string("the model running now") : current) +
        " and loads " + name;
    const auto file = field("file");
    if (!file.empty()) message += " from " + file;
    message += ". That takes a few minutes. The Manager waits for Maya to return, then refreshes this list.\n\nYour chats stay. Chat settings start at " + name +
        "'s own standard parameters and thinking level; one model's settings do not carry over to another.";
    message += "\n\nApps using the API may get a retry error until Maya is back, then the model name will be " +
        (field("model").empty() ? name : field("model")) + ".";
    if (answerActive) message += "\n\nThe answer being written finishes first.";
    return message;
}
