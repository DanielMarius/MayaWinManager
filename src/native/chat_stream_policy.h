#pragma once
#include <cstddef>
#include <cstdint>
#include <string>

inline std::string NativeChatResumePath(const std::string &jobId, std::size_t received)
{
    static constexpr char hex[] = "0123456789ABCDEF";
    std::string encoded;
    encoded.reserve(jobId.size());
    for (const unsigned char c : jobId)
    {
        if ((c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') ||
            c == '-' || c == '_' || c == '.' || c == '!' || c == '~' || c == '*' || c == '\'' || c == '(' || c == ')')
            encoded.push_back(static_cast<char>(c));
        else
        {
            encoded.push_back('%');
            encoded.push_back(hex[c >> 4]);
            encoded.push_back(hex[c & 0x0F]);
        }
    }
    return "/v1/strata/stream?id=" + encoded + "&from=" + std::to_string(received);
}

inline bool NativeChatShouldPersistJobOnClose(bool explicitlyStopped,
                                              const std::string &jobId) noexcept
{
    return !explicitlyStopped && !jobId.empty();
}

inline constexpr bool NativeChatAnswerShouldPersist(bool hasText, bool hasReasoning, bool hasTools,
                                                    int toolLimit, bool hasJobId, bool hasError,
                                                    bool stopped) noexcept
{
    return hasText || hasReasoning || hasTools || toolLimit > 0 || hasJobId || hasError || stopped;
}

inline constexpr char NativeChatResumeMissingMessage[] =
    "This answer was cut off when the page was reloaded, and the server no longer has it.";

inline constexpr char NativeChatApiKeyRequiredMessage[] =
    "This server needs an API key: add it in Settings › Connect your tools.";

inline constexpr bool NativeChatNeedsApiKeyHint(unsigned status, bool initialRequest) noexcept
{
    return initialRequest && status == 401;
}

inline constexpr bool NativeChatResumeShouldStop(unsigned status) noexcept
{
    return status == 404;
}

inline constexpr bool NativeChatKeepPendingJobAfterFailure(bool hasJobId, bool stopped, bool jobMissing) noexcept
{
    return hasJobId && !stopped && !jobMissing;
}

inline constexpr bool NativeChatReplaceFailedAnswerOnResume(bool lastIsAssistant, bool sameJob, bool failed) noexcept
{
    return lastIsAssistant && sameJob && failed;
}

inline constexpr bool NativeChatResumeCooldownActive(std::uint64_t now, std::uint64_t retryAt) noexcept
{
    return retryAt != 0 && now < retryAt;
}

inline constexpr bool NativeChatLoadedJobShouldClearPending(bool lastIsAssistant, bool sameJob,
                                                             bool failed, bool stopped) noexcept
{
    return lastIsAssistant && sameJob && (!failed || stopped);
}

inline bool NativeStreamChunkHasError(const Json &j)
{
    if (!j.is_object() || !j.contains("error")) return false;
    const auto &error = j["error"];
    if (error.is_null()) return false;
    if (error.is_boolean()) return error.get<bool>();
    if (error.is_number()) return error.get<double>() != 0.0;
    if (error.is_string()) return !error.get_ref<const std::string &>().empty();
    return true;
}

inline std::string NativeStreamErrorMessage(const Json &j)
{
    if (j.is_object() && j.contains("error") && j["error"].is_object() &&
        j["error"].contains("message") && j["error"]["message"].is_string() &&
        !j["error"]["message"].get_ref<const std::string &>().empty())
        return j["error"]["message"].get<std::string>();
    return "the engine reported an error";
}
