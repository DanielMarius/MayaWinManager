#pragma once
#include "http.h"
#include <chrono>
#include <limits>
#include <stdexcept>
#include <string>
#include <thread>

inline int NativeSettingsInteger(const std::wstring &raw)
{
    const auto first = raw.find_first_not_of(L" \t\r\n");
    if (first == std::wstring::npos)
        throw std::runtime_error("Enter a whole number in each settings field.");
    const auto last = raw.find_last_not_of(L" \t\r\n");
    int value = 0;
    for (size_t i = first; i <= last; ++i)
    {
        const wchar_t c = raw[i];
        if (c < L'0' || c > L'9')
            throw std::runtime_error("Settings fields must contain unsigned whole numbers only.");
        const int digit = c - L'0';
        if (value > (std::numeric_limits<int>::max() - digit) / 10)
            throw std::runtime_error("Settings number is too large.");
        value = value * 10 + digit;
    }
    return value;
}

inline bool NativeBudgetSupported(const Json &budget) noexcept
{
    return budget.is_object() && budget.contains("supported") && budget["supported"].is_boolean() &&
           budget["supported"].get<bool>();
}

inline bool NativeThinkingBudgetValid(int budget) noexcept
{
    return budget == 0 || (budget >= 256 && budget <= 262144);
}

template <class Api> Json SaveThinkingBudget(Api &api, int budget, const Json &budgetInfo)
{
    if (!budgetInfo.is_object() || !budgetInfo.contains("budget") || !budgetInfo["budget"].is_number_integer())
        throw std::runtime_error("Maya returned an invalid thinking budget.");
    if (!NativeThinkingBudgetValid(budget))
        throw std::runtime_error("Thinking budget must be 0 or 256 to 262144 tokens.");
    const int current = budgetInfo["budget"].get<int>();
    if (budget == current) return budgetInfo;
    if (!NativeBudgetSupported(budgetInfo))
        throw std::runtime_error("This model does not support a thinking budget.");
    return api.Post(L"/api/thinking-budget", {{"budget", budget}});
}

template <class Api> Json SaveSharedDefaults(Api &api, const Json &defaults)
{
    return api.Post(L"/settings", {{"defaults", defaults}});
}

template <class Api> Json ApplySettings(Api &api, int context, int port, int budget)
{
    const auto ctx = api.Get(L"/api/context");
    const auto portInfo = api.Get(L"/api/port");
    const auto budgetInfo = api.Get(L"/api/thinking-budget");
    auto integer = [](const Json &j, const char *key) {
        if (!j.is_object() || !j.contains(key) || !j[key].is_number_integer())
            throw std::runtime_error(std::string("Maya returned an invalid ") + key + " value.");
        return j[key].get<int>();
    };
    const int currentContext = integer(ctx, "context");
    const int minContext = integer(ctx, "min");
    const int maxContext = integer(ctx, "max");
    const int currentPort = integer(portInfo, "port");
    const int currentBudget = integer(budgetInfo, "budget");
    if (context < minContext || context > maxContext)
        throw std::runtime_error("Context is outside Maya's current supported range.");
    if (port < 1024 || port > 65535)
        throw std::runtime_error("Port must be from 1024 to 65535.");
    if (!NativeThinkingBudgetValid(budget))
        throw std::runtime_error("Thinking budget must be 0 or 256 to 262144 tokens.");
    if (budget != currentBudget && !NativeBudgetSupported(budgetInfo))
        throw std::runtime_error("This model does not support a thinking budget.");
    auto reloadActive = [](const Json &j) {
        if (!j.contains("reload") || !j["reload"].is_object())
            return false;
        const auto state = j["reload"].value("state", std::string());
        return state == "waiting" || state == "running";
    };
    if (reloadActive(ctx))
        throw std::runtime_error("Maya is already reloading; wait for it to finish before applying settings.");

    if (context != currentContext)
    {
        api.Post(L"/api/context", {{"max_context", context}});
        const auto until = std::chrono::steady_clock::now() + std::chrono::minutes(12);
        bool finished = false;
        while (std::chrono::steady_clock::now() < until)
        {
            const auto state = api.Get(L"/api/context");
            if (!state.contains("reload") || !state["reload"].is_object())
                throw std::runtime_error("Maya did not report the context reload state.");
            const auto reload = state["reload"];
            const auto name = reload.value("state", std::string());
            if (name == "failed")
                throw std::runtime_error(reload.value("error", std::string("Maya failed to reload the context.")));
            if (name == "done")
            {
                if (integer(state, "context") != context)
                    throw std::runtime_error("Maya finished the reload at an unexpected context size.");
                finished = true;
                break;
            }
            if (name != "waiting" && name != "running")
                throw std::runtime_error("Maya returned an unknown context reload state.");
            std::this_thread::sleep_for(std::chrono::seconds(2));
        }
        if (!finished)
            throw std::runtime_error("Timed out waiting for Maya to finish the context reload.");
    }
    if (budget != currentBudget)
        SaveThinkingBudget(api, budget, budgetInfo);

    Json portResult = portInfo;
    if (port != currentPort)
    {
        if (!portInfo.value("saved", false))
            throw std::runtime_error("Maya cannot save a port for this launch configuration.");
        portResult = api.Post(L"/api/port", {{"port", port}});
    }
    return {{"context", context},
            {"port", port},
            {"budget", budget},
            {"port_restarts", portResult.value("restarts", false)}};
}
