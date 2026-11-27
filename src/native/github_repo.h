#pragma once
#include <string>

inline std::string MayaGitHubRepo(std::string url)
{
    const auto first = url.find_first_not_of(" \t\r\n");
    if (first == std::string::npos) return {};
    url = url.substr(first, url.find_last_not_of(" \t\r\n") - first + 1);
    auto position = url.find("github.com");
    if (position == std::string::npos) return {};
    position += 10;
    while (position < url.size() && (url[position] == '/' || url[position] == ':')) ++position;
    const auto end = url.find_first_of("?#", position);
    auto repo = url.substr(position, end == std::string::npos ? end : end - position);
    while (!repo.empty() && repo.back() == '/') repo.pop_back();
    if (repo.size() > 4 && repo.substr(repo.size() - 4) == ".git") repo.resize(repo.size() - 4);
    return repo.find('/') == std::string::npos ? std::string() : repo;
}
