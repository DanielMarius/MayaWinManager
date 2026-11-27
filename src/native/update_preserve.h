#pragma once
#include <algorithm>
#include <climits>
#include <filesystem>
#include <iterator>
#include <string>
#include <string_view>
#include <utility>
#include <vector>
#include <windows.h>

inline std::vector<std::string> MayaUpdatePathList(const std::string &output)
{
    std::vector<std::string> paths;
    for (size_t at = 0; at < output.size();)
    {
        const auto end = output.find('\0', at);
        const auto count = (end == std::string::npos ? output.size() : end) - at;
        if (count) paths.emplace_back(output, at, count);
        if (end == std::string::npos) break;
        at = end + 1;
    }
    return paths;
}

inline bool MayaUpdatePathToWide(const std::string &path, std::wstring &wide)
{
    if (path.empty() || path.size() > static_cast<size_t>(INT_MAX)) return false;
    const int length = MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, path.data(),
                                           static_cast<int>(path.size()), nullptr, 0);
    if (length <= 0) return false;
    wide.resize(length);
    if (MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, path.data(), static_cast<int>(path.size()),
                            wide.data(), length) != length)
    {
        wide.clear();
        return false;
    }
    for (auto &c : wide) if (c == L'/') c = L'\\';
    while (!wide.empty() && wide.back() == L'\\') wide.pop_back();
    return !wide.empty();
}

inline bool MayaUpdatePathsCollide(const std::wstring &a, const std::wstring &b)
{
    auto equal = [](std::wstring_view x, std::wstring_view y) {
        if (x.size() > static_cast<size_t>(INT_MAX) || y.size() > static_cast<size_t>(INT_MAX)) return true;
        const int result = CompareStringOrdinal(x.data(), static_cast<int>(x.size()), y.data(),
                                                static_cast<int>(y.size()), TRUE);
        return result == CSTR_EQUAL || result == 0;
    };
    auto contains = [&](const std::wstring &parent, const std::wstring &child) {
        return child.size() > parent.size() && child[parent.size()] == L'\\' &&
               equal(parent, std::wstring_view(child.data(), parent.size()));
    };
    return equal(a, b) || contains(a, b) || contains(b, a);
}

inline bool MayaUpdatePathsCollide(const std::string &a, const std::string &b)
{
    std::wstring left, right;
    return !MayaUpdatePathToWide(a, left) || !MayaUpdatePathToWide(b, right) ||
           MayaUpdatePathsCollide(left, right);
}

inline std::string MayaUpdatePathConflict(const std::vector<std::string> &tracked,
                                          const std::vector<std::string> &untracked,
                                          const std::vector<std::string> &incoming)
{
    auto decode = [](const std::vector<std::string> &paths, std::vector<std::wstring> &widePaths) {
        widePaths.reserve(paths.size());
        for (const auto &path : paths)
        {
            std::wstring wide;
            if (!MayaUpdatePathToWide(path, wide)) return false;
            widePaths.push_back(std::move(wide));
        }
        return true;
    };
    std::vector<std::wstring> trackedWide, untrackedWide, incomingWide;
    if (!decode(tracked, trackedWide) || !decode(untracked, untrackedWide) || !decode(incoming, incomingWide))
        return "Git reported a path that is not valid UTF-8; refusing the update.";
    for (size_t i = 0; i < tracked.size(); ++i)
        for (const auto &remote : incomingWide)
            if (MayaUpdatePathsCollide(trackedWide[i], remote))
                return "A locally changed file overlaps the release: " + tracked[i];
    for (size_t i = 0; i < untracked.size(); ++i)
        for (const auto &remote : incomingWide)
            if (MayaUpdatePathsCollide(untrackedWide[i], remote))
                return "An untracked file would be overwritten by the release: " + untracked[i];
    return {};
}

inline std::string MayaUpdateExistingPathConflict(const std::filesystem::path &root,
                                                  const std::vector<std::string> &incomingAdds)
{
    for (const auto &name : incomingAdds)
    {
        const auto relative = std::filesystem::u8path(name);
        if (relative.is_absolute() || relative.has_root_path()) return "The release contains an unsafe file path.";
        auto path = root;
        for (auto part = relative.begin(); part != relative.end(); ++part)
        {
            if (*part == L".." || *part == L".") return "The release contains an unsafe file path.";
            path /= *part;
            std::error_code error;
            const auto status = std::filesystem::symlink_status(path, error);
            if (error == std::errc::no_such_file_or_directory) break;
            if (error) return "Could not verify existing files before the update: " + name;
            if (status.type() == std::filesystem::file_type::not_found) break;
            const bool last = std::next(part) == relative.end();
            if (last || !std::filesystem::is_directory(status))
                return "An existing local file or directory would be overwritten by the release: " + name;
        }
    }
    return {};
}
