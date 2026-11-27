#pragma once
#include "http.h"
#include <cctype>
#include <iomanip>
#include <sstream>

inline std::string AnswerMetadata(const Json &usage, const Json &timings, double elapsed, bool stopped,
                                 const Json &tools, int toolLimit)
{
    std::ostringstream out;
    if (usage.is_object() && usage.contains("completion_tokens") && usage["completion_tokens"].is_number())
    {
        const auto tokens = usage["completion_tokens"].get<int64_t>();
        if (tokens > 0)
        {
            out << tokens << " tokens";
            auto rate = timings.is_object() && timings.contains("predicted_per_second") &&
                                timings["predicted_per_second"].is_number()
                            ? timings["predicted_per_second"].get<double>() : 0.0;
            if (rate == 0 && elapsed > 0.25) rate = (double)tokens / elapsed;
            if (rate != 0)
                out << " · " << std::fixed << std::setprecision(1) << rate << " tok/s";
            if (timings.is_object() && timings.contains("prompt_n") && timings["prompt_n"].is_number() &&
                timings.contains("prompt_ms") && timings["prompt_ms"].is_number() &&
                timings["prompt_ms"].get<double>() > 0)
            {
                out << " · read " << timings["prompt_n"].get<int64_t>() << " new prompt tokens in "
                    << std::fixed << std::setprecision(1) << timings["prompt_ms"].get<double>() / 1000.0 << " s";
                const auto cached = timings.value("cache_n", int64_t(0));
                if (cached > 0)
                    out << " (" << cached << " reused)";
            }
        }
    }
    size_t completed = 0;
    if (tools.is_array())
        for (const auto &tool : tools)
            if (tool.is_object() && (tool.value("state", std::string()) == "done" ||
                                     tool.value("state", std::string()) == "error"))
                ++completed;
    if (completed)
        out << (out.tellp() > 0 ? " · " : "") << completed << " tool call" << (completed == 1 ? "" : "s");
    if (toolLimit > 0)
        out << (out.tellp() > 0 ? " · " : "") << "stopped at the limit of " << toolLimit << " tool rounds (mcp.max_rounds)";
    if (stopped)
        out << (out.tellp() > 0 ? " · " : "") << "stopped";
    auto result = out.str();
    if (!result.empty())
        result[0] = (char)std::toupper((unsigned char)result[0]);
    return result;
}
