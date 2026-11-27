#pragma once
#include "http.h"
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <iomanip>
#include <sstream>

inline std::wstring NativeContextReloadWarning(const Json &info, int next, uint64_t chatTokens, bool approximate)
{
    const auto number = [&](const char *key) {
        return info.is_object() && info.contains(key) && info[key].is_number() ? info[key].get<double>() : 0.0;
    };
    const int current = (int)number("context");
    const auto gbText = [](double value) {
        std::wostringstream out;
        out << std::fixed << std::setprecision(1) << value;
        return out.str();
    };
    std::wstring text = L"Maya will reload with " + std::to_wstring(next) + L" tokens of context (currently " +
        std::to_wstring(current) + L"). Your chats stay saved. Other API requests may need to retry while Maya reloads; "
        L"if the new size does not fit, Maya restores the current size.";
    const double loadSeconds = number("load_s");
    if (loadSeconds > 0.0)
    {
        const int minutes = (int)std::round(loadSeconds / 60.0);
        text += L" The last model load took about " + std::to_wstring(std::max(1, minutes)) + L" minute" +
            (minutes > 1 ? L"s." : L".");
    }
    else text += L" Model loading often takes 1–3 minutes.";

    const double resident = number("kv_resident");
    const double kvRam = number("kv_ram_gb");
    const double kvGb = number("kv_gb");
    const double kvCtx = number("kv_ctx");
    const double vram = number("vram_gb");
    const double slots = number("vram_slots");
    if (current > 0 && resident > 0 && kvRam > 0)
    {
        const double estimate = kvRam * next / current;
        text += L" Estimated context cache: about " + gbText(estimate) + L" GB pinned in RAM (" +
            std::to_wstring((int)resident) + L" positions also in VRAM).";
        const double free = number("ram_free_gb");
        if (free > 0 && estimate - kvRam > free - 8.0)
            text += L" This leaves little free RAM for experts and may make answers slower or prevent the reload.";
    }
    else if (!resident && kvGb > 0 && kvCtx > 0)
    {
        const double estimate = kvGb / kvCtx * next;
        text += L" Estimated context cache: about " + gbText(estimate) + L" GB of GPU memory.";
        if (vram > 0 && estimate > vram * 0.6)
            text += L" This leaves little VRAM for experts and may make answers slower or prevent the reload.";
        if (vram > 0 && slots > 0)
        {
            const double experts = std::round(((estimate - kvGb / kvCtx * current) / (vram / slots)));
            if (experts != 0)
                text += L" Compared with now, about " + std::to_wstring((int)std::abs(experts)) +
                    (experts > 0 ? L" fewer experts" : L" more experts") + L" fit in VRAM.";
        }
    }
    if (next > (number("tested") > 0 ? number("tested") : 262144))
        text += L" This context is beyond the largest size tested on this engine.";
    if (chatTokens > (uint64_t)std::max(0, next))
        text += approximate ? L" The open chat is estimated to exceed the new context; start a new chat after reload."
                            : L" The open chat currently exceeds the new context; start a new chat after reload.";
    return text;
}
