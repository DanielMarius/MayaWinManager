#pragma once
#include "http.h"
#include <urlmon.h>
#include <wincrypt.h>
#include <algorithm>
#include <array>
#include <cwchar>
#include <filesystem>
#include <fstream>
#include <limits>
#include <string_view>
#include <utility>
#include <vector>

enum class MayaAttachmentKind { IMAGE, VIDEO, TEXT, SERVER, UNSUPPORTED };

inline std::string MayaAttachmentHeaderFilename(const std::wstring &filename)
{
    static constexpr char hex[] = "0123456789ABCDEF";
    const auto utf8 = Utf8(filename);
    std::string encoded;
    encoded.reserve(utf8.size() * 3);
    for (unsigned char c : utf8)
    {
        if ((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') ||
            c == '-' || c == '_' || c == '.' || c == '~')
            encoded.push_back(static_cast<char>(c));
        else
        {
            encoded.push_back('%');
            encoded.push_back(hex[c >> 4]);
            encoded.push_back(hex[c & 15]);
        }
    }
    return encoded;
}

inline bool MayaChatDropTarget(int x, int y, int contentLeft, int contentTop) noexcept
{
    return x >= contentLeft && y >= contentTop;
}

inline std::string MayaTextFileBlock(const std::string &name, const std::string &text)
{
    size_t run = 0, longest = 0;
    for (char c : text)
        if (c == '`') longest = std::max(longest, ++run);
        else run = 0;
    const std::string fence(std::max<size_t>(3, longest + 1), '`');
    return "File: " + name + "\n" + fence + "\n" + text + "\n" + fence;
}

inline Json MayaUserMessageContent(const std::string &text, const Json &images)
{
    if (!images.is_array() || images.empty()) return text;
    Json parts = Json::array({Json{{"type", "text"}, {"text", text}}});
    for (const auto &image : images) parts.push_back(image);
    return parts;
}

inline std::string MayaAttachmentDataUrl(const std::string &bytes, const std::wstring &mime)
{
    if (bytes.size() > std::numeric_limits<DWORD>::max())
        throw std::runtime_error("Attachment is too large to encode.");
    const auto *data = reinterpret_cast<const BYTE *>(bytes.data());
    const DWORD length = static_cast<DWORD>(bytes.size());
    constexpr DWORD flags = CRYPT_STRING_BASE64 | CRYPT_STRING_NOCRLF;
    DWORD chars = 0;
    if (!CryptBinaryToStringA(data, length, flags, nullptr, &chars) || !chars)
        throw std::runtime_error("Could not encode attachment as base64.");
    std::string encoded(chars, '\0');
    if (!CryptBinaryToStringA(data, length, flags, encoded.data(), &chars) || chars > encoded.size())
        throw std::runtime_error("Could not encode attachment as base64.");
    encoded.resize(chars);
    return "data:" + Utf8(mime) + ";base64," + encoded;
}

inline Json MayaMediaContentPart(MayaAttachmentKind kind, const std::string &url)
{
    if (kind == MayaAttachmentKind::VIDEO)
        return {{"type", "video_url"}, {"video_url", {{"url", url}}}};
    return {{"type", "image_url"}, {"image_url", {{"url", url}}}};
}

inline void MayaAppendServerAttachment(const std::string &name, const Json &result,
                                       std::string &text, Json &images)
{
    if (!result.is_object())
        throw std::runtime_error("Maya returned an invalid attachment response for " + name + ".");
    if (result.contains("error"))
    {
        const auto &error = result["error"];
        std::string message;
        if (error.is_object() && error.contains("message") && error["message"].is_string())
            message = error["message"].get<std::string>();
        else if (error.is_string())
            message = error.get<std::string>();
        throw std::runtime_error("The attached file " + name + " could not be read: " +
                                 (message.empty() ? "Maya did not give a reason." : message));
    }
    if (!result.contains("kind") || !result["kind"].is_string())
        throw std::runtime_error("Maya returned an invalid attachment response for " + name + ".");
    const auto kind = result["kind"].get<std::string>();
    auto appendText = [&](const std::string &block) {
        text = text.empty() ? block : text + "\n\n" + block;
    };
    if (kind == "text" && result.contains("text") && result["text"].is_string())
    {
        appendText(MayaTextFileBlock(name, result["text"].get<std::string>()));
        return;
    }
    if (kind == "images" && result.contains("images") && result["images"].is_array() &&
        !result["images"].empty())
    {
        appendText("File: " + name + " (its first " + std::to_string(result["images"].size()) +
                   " pages as pictures)");
        for (const auto &image : result["images"])
        {
            if (!image.is_object() || !image.contains("data") || !image["data"].is_string())
                throw std::runtime_error("Maya returned an invalid page image for " + name + ".");
            const auto url = image["data"].get<std::string>();
            if (url.rfind("data:image/", 0) != 0 || url.find(";base64,") == std::string::npos)
                throw std::runtime_error("Maya returned an invalid page image for " + name + ".");
            images.push_back({{"type", "image_url"}, {"image_url", {{"url", url}}}});
        }
        return;
    }
    throw std::runtime_error("Maya returned an unsupported attachment response for " + name + ".");
}

inline std::string MayaDecodeTextBytes(const std::string &bytes)
{
    size_t offset = 0;
    UINT codePage = CP_UTF8;
    if (bytes.size() >= 3 && (BYTE)bytes[0] == 0xEF && (BYTE)bytes[1] == 0xBB && (BYTE)bytes[2] == 0xBF)
        offset = 3;
    else if (bytes.size() >= 2 && (BYTE)bytes[0] == 0xFF && (BYTE)bytes[1] == 0xFE)
    {
        offset = 2;
        codePage = 1200;
    }
    else if (bytes.size() >= 2 && (BYTE)bytes[0] == 0xFE && (BYTE)bytes[1] == 0xFF)
    {
        offset = 2;
        codePage = 1201;
    }

    std::wstring wide;
    if (codePage == 1200 || codePage == 1201)
    {
        const bool littleEndian = codePage == 1200;
        size_t i = offset;
        for (; i + 1 < bytes.size(); i += 2)
        {
            const auto unitAt = [&](size_t at) {
                const BYTE a = (BYTE)bytes[at], b = (BYTE)bytes[at + 1];
                return (wchar_t)(littleEndian ? (a | (b << 8)) : ((a << 8) | b));
            };
            const wchar_t unit = unitAt(i);
            if (unit >= 0xD800 && unit <= 0xDBFF)
            {
                if (i + 3 < bytes.size())
                {
                    const wchar_t low = unitAt(i + 2);
                    if (low >= 0xDC00 && low <= 0xDFFF)
                    {
                        wide.push_back(unit);
                        wide.push_back(low);
                        i += 2;
                        continue;
                    }
                }
                wide.push_back(0xFFFD);
            }
            else if (unit >= 0xDC00 && unit <= 0xDFFF)
                wide.push_back(0xFFFD);
            else
                wide.push_back(unit);
        }
        if (i < bytes.size()) wide.push_back(0xFFFD);
    }
    else
    {
        const int length = (int)(bytes.size() - offset);
        if (!length) return {};
        const int count = MultiByteToWideChar(CP_UTF8, 0, bytes.data() + offset, length, nullptr, 0);
        if (!count) throw std::runtime_error("Could not decode attached text as UTF-8.");
        wide.resize(count);
        if (!MultiByteToWideChar(CP_UTF8, 0, bytes.data() + offset, length, wide.data(), count))
            throw std::runtime_error("Could not decode attached text as UTF-8.");
    }

    if (wide.empty()) return {};
    const int count = WideCharToMultiByte(CP_UTF8, WC_ERR_INVALID_CHARS, wide.data(), (int)wide.size(), nullptr, 0, nullptr, nullptr);
    if (!count) throw std::runtime_error("Could not encode attached text as UTF-8.");
    std::string text(count, '\0');
    if (!WideCharToMultiByte(CP_UTF8, WC_ERR_INVALID_CHARS, wide.data(), (int)wide.size(), text.data(), count, nullptr, nullptr))
        throw std::runtime_error("Could not encode attached text as UTF-8.");
    return text;
}

inline bool MayaAttachmentFileSizeAllowed(uint64_t bytes, uint64_t limit) noexcept
{
    return bytes <= limit;
}

inline bool MayaAttachmentFileSize(const std::filesystem::path &path, uint64_t &bytes)
{
    std::error_code error;
    const auto size = std::filesystem::file_size(path, error);
    if (error || size > std::numeric_limits<uint64_t>::max()) return false;
    bytes = static_cast<uint64_t>(size);
    return true;
}

inline bool MayaReadFileBytes(const std::filesystem::path &path, uint64_t maxBytes, std::string &bytes)
{
    bytes.clear();
    uint64_t size = 0;
    if (!MayaAttachmentFileSize(path, size) || !MayaAttachmentFileSizeAllowed(size, maxBytes) ||
        size > bytes.max_size() || size > static_cast<uint64_t>(std::numeric_limits<std::streamsize>::max()))
        return false;
    std::ifstream in(path, std::ios::binary);
    if (!in) return false;
    bytes.resize(static_cast<size_t>(size));
    if (size && !in.read(bytes.data(), static_cast<std::streamsize>(size))) { bytes.clear(); return false; }
    if (in.peek() != std::char_traits<char>::eof()) { bytes.clear(); return false; }
    return !in.bad();
}

inline bool MayaAttachmentData(const std::filesystem::path &path, const std::vector<std::string> &snapshots,
                               size_t index, uint64_t maxBytes, std::string &bytes)
{
    bytes.clear();
    if (index < snapshots.size())
    {
        if (!MayaAttachmentFileSizeAllowed(snapshots[index].size(), maxBytes)) return false;
        bytes = snapshots[index];
        return true;
    }
    return MayaReadFileBytes(path, maxBytes, bytes);
}

inline std::filesystem::path MayaStoredAttachmentPath(const std::filesystem::path &root,
                                                       const std::wstring &chatId,
                                                       const std::wstring &attachmentId,
                                                       const std::filesystem::path &filename)
{
    return root / L"attachments" / chatId / attachmentId / filename.filename();
}

inline bool MayaPathIsWithin(const std::filesystem::path &root, const std::filesystem::path &path)
{
    const auto normalizedRoot = root.lexically_normal();
    const auto normalizedPath = path.lexically_normal();
    auto rootPart = normalizedRoot.begin();
    auto pathPart = normalizedPath.begin();
    for (; rootPart != normalizedRoot.end() && pathPart != normalizedPath.end() && *rootPart == *pathPart;
         ++rootPart, ++pathPart)
    {
    }
    return rootPart == normalizedRoot.end() && pathPart != normalizedPath.end();
}

inline std::vector<std::filesystem::path> MayaParseFileSelection(const wchar_t *buffer)
{
    std::vector<std::filesystem::path> files;
    if (!buffer || !*buffer) return files;
    const wchar_t *next = buffer + wcslen(buffer) + 1;
    if (!*next)
    {
        files.emplace_back(buffer);
        return files;
    }
    const std::filesystem::path folder(buffer);
    while (*next)
    {
        files.push_back(folder / next);
        next += wcslen(next) + 1;
    }
    return files;
}

inline std::wstring MayaAttachmentMimeType(const std::filesystem::path &path)
{
    auto ext = path.extension().wstring();
    std::transform(ext.begin(), ext.end(), ext.begin(), towlower);
    static constexpr std::array<std::pair<std::wstring_view, std::wstring_view>, 8> imageTypes{{
        {L".png", L"image/png"}, {L".jpg", L"image/jpeg"}, {L".jpeg", L"image/jpeg"},
        {L".webp", L"image/webp"}, {L".gif", L"image/gif"}, {L".bmp", L"image/bmp"},
        {L".tif", L"image/tiff"}, {L".tiff", L"image/tiff"}}};
    for (const auto &[suffix, mime] : imageTypes)
        if (ext == suffix) return std::wstring(mime);
    static constexpr std::array<std::pair<std::wstring_view, std::wstring_view>, 10> videoTypes{{
        {L".mp4", L"video/mp4"}, {L".m4v", L"video/mp4"}, {L".mov", L"video/quicktime"},
        {L".webm", L"video/webm"}, {L".mkv", L"video/x-matroska"}, {L".avi", L"video/x-msvideo"},
        {L".mpeg", L"video/mpeg"}, {L".mpg", L"video/mpeg"}, {L".3gp", L"video/3gpp"},
        {L".ogv", L"video/ogg"}}};
    for (const auto &[suffix, mime] : videoTypes)
        if (ext == suffix) return std::wstring(mime);

    std::array<BYTE, 512> sample{};
    std::ifstream in(path, std::ios::binary);
    if (!in) return {};
    in.read(reinterpret_cast<char *>(sample.data()), sample.size());
    LPWSTR mime = nullptr;
    if (FAILED(FindMimeFromData(nullptr, path.c_str(), sample.data(), (DWORD)in.gcount(), nullptr,
                                FMFD_URLASFILENAME, &mime, 0)))
        return {};
    std::wstring type = mime ? mime : L"";
    if (mime) CoTaskMemFree(mime);
    std::transform(type.begin(), type.end(), type.begin(), towlower);
    return type;
}

inline MayaAttachmentKind MayaAttachmentType(const std::filesystem::path &path)
{
    auto ext = path.extension().wstring();
    std::transform(ext.begin(), ext.end(), ext.begin(), towlower);
    static constexpr std::array imageExts{L".png", L".jpg", L".jpeg", L".webp", L".gif", L".bmp", L".tif", L".tiff"};
    if (std::find(imageExts.begin(), imageExts.end(), ext) != imageExts.end()) return MayaAttachmentKind::IMAGE;
    static constexpr std::array textExts{
        L".text",
        L".txt", L".md", L".markdown", L".rst", L".tex", L".py", L".pyi", L".ipynb", L".js", L".mjs", L".cjs",
        L".ts", L".tsx", L".jsx", L".vue", L".svelte", L".json", L".jsonl", L".csv", L".tsv", L".log", L".yml",
        L".yaml", L".toml", L".ini", L".cfg", L".conf", L".env", L".xml", L".htm", L".html", L".css", L".scss",
        L".less", L".c", L".cc", L".cpp", L".cxx", L".h", L".hh", L".hpp", L".cu", L".cuh", L".rs", L".go",
        L".java", L".kt", L".kts", L".swift", L".rb", L".php", L".pl", L".lua", L".r", L".jl", L".scala",
        L".sql", L".sh", L".bash", L".zsh", L".fish", L".ps1", L".psm1", L".bat", L".cmd", L".diff", L".patch",
        L".gradle", L".cmake", L".mk", L".dockerfile", L".gitignore", L".proto", L".graphql"};
    if (std::find(textExts.begin(), textExts.end(), ext) != textExts.end()) return MayaAttachmentKind::TEXT;
    auto name = path.filename().wstring();
    std::transform(name.begin(), name.end(), name.begin(), towlower);
    if (name == L"makefile" || name == L"dockerfile" || name == L".dockerfile" || name == L"readme" || name == L"license" ||
        name == L".env" || name == L".gitignore")
        return MayaAttachmentKind::TEXT;
    static constexpr std::array videoExts{L".mp4", L".m4v", L".mov", L".webm", L".mkv", L".avi",
                                          L".mpeg", L".mpg", L".3gp", L".ogv"};
    if (std::find(videoExts.begin(), videoExts.end(), ext) != videoExts.end()) return MayaAttachmentKind::VIDEO;
    static constexpr std::array audioExts{L".mp3", L".wav", L".ogg", L".flac", L".m4a", L".aac"};
    if (std::find(audioExts.begin(), audioExts.end(), ext) != audioExts.end()) return MayaAttachmentKind::UNSUPPORTED;
    auto type = MayaAttachmentMimeType(path);
    std::transform(type.begin(), type.end(), type.begin(), towlower);
    if (type.rfind(L"image/", 0) == 0) return MayaAttachmentKind::IMAGE;
    if (type.rfind(L"video/", 0) == 0) return MayaAttachmentKind::VIDEO;
    if (type.rfind(L"text/", 0) == 0 || type.find(L"json") != std::wstring::npos ||
        type.find(L"xml") != std::wstring::npos || type.find(L"javascript") != std::wstring::npos ||
        type.find(L"yaml") != std::wstring::npos || type.find(L"toml") != std::wstring::npos ||
        type.find(L"x-sh") != std::wstring::npos || type.find(L"x-python") != std::wstring::npos)
        return MayaAttachmentKind::TEXT;
    if (type.rfind(L"audio/", 0) == 0) return MayaAttachmentKind::UNSUPPORTED;
    return MayaAttachmentKind::SERVER;
}

inline std::vector<std::filesystem::path> MayaClipboardImages(const std::vector<std::filesystem::path> &files,
                                                              bool imagesEnabled)
{
    std::vector<std::filesystem::path> images;
    if (!imagesEnabled) return images;
    for (const auto &file : files)
        if (MayaAttachmentType(file) == MayaAttachmentKind::IMAGE) images.push_back(file);
    return images;
}

inline uint64_t MayaTextFileLimit(const Json &health)
{
    constexpr uint64_t fallback = 512 * 1024;
    if (!health.is_object() || !health.contains("max_text_file")) return fallback;
    const auto &value = health["max_text_file"];
    if (value.is_number_unsigned())
    {
        const auto limit = value.get<uint64_t>();
        return limit ? limit : fallback;
    }
    if (value.is_number_integer())
    {
        const auto limit = value.get<int64_t>();
        return limit > 0 ? static_cast<uint64_t>(limit) : limit == 0 ? fallback : 0;
    }
    return fallback;
}

inline uint64_t MayaAttachmentReadLimit(MayaAttachmentKind kind, const Json &health)
{
    if (kind == MayaAttachmentKind::IMAGE) return 20000000ULL;
    if (kind == MayaAttachmentKind::VIDEO) return 100ULL * 1024 * 1024;
    if (kind == MayaAttachmentKind::TEXT) return MayaTextFileLimit(health);
    if (kind == MayaAttachmentKind::SERVER) return 64ULL * 1024 * 1024;
    return 0;
}

inline std::pair<unsigned, unsigned> MayaInlineThumbnailSize(unsigned width, unsigned height) noexcept
{
    if (!width || !height) return {0, 0};
    const double scale = std::min(1.0, std::min(144.0 / width, 84.0 / height));
    return {std::max(1u, (unsigned)(width * scale)), std::max(1u, (unsigned)(height * scale))};
}

inline std::string MayaAttachmentSizeError(MayaAttachmentKind kind, uint64_t bytes, const Json &health)
{
    if (kind == MayaAttachmentKind::UNSUPPORTED)
        return "This Maya Manager does not support audio attachments.";
    if (kind == MayaAttachmentKind::VIDEO)
    {
        if (!health.value("videos", false))
            return health.value("images", false) ? "This Maya model reads pictures, not videos."
                                                  : "Videos are not enabled for the current Maya model.";
        if (bytes > 100ULL * 1024 * 1024) return "Video is over 100 MB.";
        return {};
    }
    if (kind == MayaAttachmentKind::SERVER)
        return bytes > 64ULL * 1024 * 1024 ? "File is over 64 MB." : std::string();
    if (kind == MayaAttachmentKind::IMAGE)
    {
        if (!health.value("images", false)) return "Images are off for the current Maya model.";
        if (bytes > 20000000ULL) return "Picture is over 20 MB.";
        return {};
    }
    if (bytes > MayaTextFileLimit(health)) return "Text file is over this model's attachment limit.";
    return {};
}

inline bool MayaAttachmentHasNul(const std::string &bytes)
{
    return MayaDecodeTextBytes(bytes).find('\0') != std::string::npos;
}
