#include <winsock2.h>
#include <ws2tcpip.h>
#include "http.h"
#include "model_delete.h"
#include "model_leftover.h"
#include "model_settings.h"
#include "settings_flow.h"
#include <cassert>
#include <exception>
#include <iostream>
#include <sstream>
#include <string>
#include <thread>

namespace fs = std::filesystem;

static void CloseSocket(SOCKET socket)
{
    if (socket != INVALID_SOCKET)
    {
        shutdown(socket, SD_SEND);
        closesocket(socket);
    }
}

static std::string ReadRequest(SOCKET socket)
{
    std::string request;
    char buffer[2048];
    while (request.size() < 65536 && request.find("\r\n\r\n") == std::string::npos)
    {
        const int count = recv(socket, buffer, sizeof(buffer), 0);
        if (count <= 0) break;
        request.append(buffer, count);
    }
    if (request.find("\r\n\r\n") == std::string::npos)
        throw std::runtime_error("The fixture did not receive a complete HTTP request header.");
    const size_t headerEnd = request.find("\r\n\r\n");
    size_t contentLength = 0;
    std::istringstream headers(request.substr(0, headerEnd));
    std::string line;
    while (std::getline(headers, line))
        if (line.rfind("Content-Length:", 0) == 0)
            contentLength = std::stoul(line.substr(15));
    const size_t total = headerEnd + 4 + contentLength;
    while (request.size() < total)
    {
        const int count = recv(socket, buffer, sizeof(buffer), 0);
        if (count <= 0) throw std::runtime_error("The fixture did not receive the complete HTTP request body.");
        request.append(buffer, count);
    }
    return request;
}

static std::string RequestLine(const std::string &request)
{
    return request.substr(0, request.find("\r\n"));
}

static void Reply(SOCKET socket, unsigned status, const std::string &reason, const std::string &type,
                  const std::string &body)
{
    const auto response = "HTTP/1.1 " + std::to_string(status) + " " + reason +
        "\r\nContent-Type: " + type + "\r\nContent-Length: " + std::to_string(body.size()) +
        "\r\nConnection: close\r\n\r\n" + body;
    size_t sent = 0;
    while (sent < response.size())
    {
        const int count = send(socket, response.data() + sent,
                               static_cast<int>(response.size() - sent), 0);
        if (count <= 0) throw std::runtime_error("The fixture could not send its HTTP response.");
        sent += static_cast<size_t>(count);
    }
}

static void ReplyChunked(SOCKET socket, const std::string &body)
{
    const std::string headers = "HTTP/1.1 200 OK\r\nContent-Type: text/event-stream\r\n"
        "Transfer-Encoding: chunked\r\nConnection: close\r\n\r\n";
    auto sendAll = [&](const std::string &data) {
        size_t sent = 0;
        while (sent < data.size())
        {
            const int count = send(socket, data.data() + sent,
                                   static_cast<int>(data.size() - sent), 0);
            if (count <= 0) throw std::runtime_error("The fixture could not send a chunked response.");
            sent += static_cast<size_t>(count);
        }
    };
    sendAll(headers);
    for (size_t offset = 0; offset < body.size(); offset += 7)
    {
        const auto count = std::min<size_t>(7, body.size() - offset);
        std::ostringstream size;
        size << std::hex << count;
        sendAll(size.str() + "\r\n" + body.substr(offset, count) + "\r\n");
    }
    sendAll("0\r\n\r\n");
}
int main(int argc, char **argv)
{
    assert(argc == 2);
    assert(NativeChatResumePath("fixture id/+?", 3) == "/v1/strata/stream?id=fixture%20id%2F%2B%3F&from=3");
    assert(NativeChatResumePath(u8"caf\u00e9", 0) == "/v1/strata/stream?id=caf%C3%A9&from=0");
    assert(NativeChatKeepPendingJobAfterFailure(true, false, false));
    assert(!NativeChatKeepPendingJobAfterFailure(false, false, false));
    assert(!NativeChatKeepPendingJobAfterFailure(true, true, false));
    assert(!NativeChatKeepPendingJobAfterFailure(true, false, true));
    assert(NativeChatReplaceFailedAnswerOnResume(true, true, true));
    assert(!NativeChatReplaceFailedAnswerOnResume(false, true, true));
    assert(!NativeChatReplaceFailedAnswerOnResume(true, false, true));
    assert(!NativeChatReplaceFailedAnswerOnResume(true, true, false));
    assert(NativeChatResumeCooldownActive(100, 200));
    assert(!NativeChatResumeCooldownActive(200, 200));
    assert(!NativeChatResumeCooldownActive(300, 200));
    assert(!NativeChatResumeCooldownActive(100, 0));
    assert(NativeChatLoadedJobShouldClearPending(true, true, false, false));
    assert(!NativeChatLoadedJobShouldClearPending(true, true, true, false));
    assert(NativeChatLoadedJobShouldClearPending(true, true, true, true));
    assert(!NativeChatLoadedJobShouldClearPending(false, true, false, false));
    assert(!NativeChatLoadedJobShouldClearPending(true, false, false, false));
    for (const auto &value : {Json(nullptr), Json(false), Json(0), Json(0.0), Json("")})
        assert(!NativeStreamChunkHasError(Json{{"error", value}}));
    assert(NativeStreamChunkHasError(Json{{"error", "failed"}}));
    assert(NativeStreamChunkHasError(Json{{"error", Json::object()}}));
    assert(NativeStreamChunkHasError(Json{{"error", Json::array()}}));
    assert(NativeStreamErrorMessage(Json{{"error", {{"message", "stream failed"}}}}) == "stream failed");
    assert(NativeStreamErrorMessage(Json{{"error", {{"message", ""}}}}) == "the engine reported an error");
    assert(NativeStreamErrorMessage(Json{{"error", "stream failed"}}) == "the engine reported an error");
    WSADATA wsa{};
    if (WSAStartup(MAKEWORD(2, 2), &wsa) != 0) return 1;
    SOCKET listener = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (listener == INVALID_SOCKET) { WSACleanup(); return 2; }
    sockaddr_in address{};
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    address.sin_port = 0;
    if (bind(listener, reinterpret_cast<sockaddr *>(&address), sizeof(address)) == SOCKET_ERROR ||
        listen(listener, 4) == SOCKET_ERROR)
    {
        CloseSocket(listener); WSACleanup(); return 3;
    }
    int addressSize = sizeof(address);
    if (getsockname(listener, reinterpret_cast<sockaddr *>(&address), &addressSize) == SOCKET_ERROR)
    {
        CloseSocket(listener); WSACleanup(); return 4;
    }
    const auto port = ntohs(address.sin_port);
    std::exception_ptr serverError;
    std::string resumedPath;
    std::thread server([&] {
        try
        {
            const auto acceptOne = [&]() {
                fd_set set; FD_ZERO(&set); FD_SET(listener, &set);
                timeval timeout{15, 0};
                if (select(0, &set, nullptr, nullptr, &timeout) <= 0)
                    throw std::runtime_error("Timed out waiting for MayaHttp's loopback request.");
                const SOCKET client = accept(listener, nullptr, nullptr);
                if (client == INVALID_SOCKET) throw std::runtime_error("The fixture could not accept a client.");
                return client;
            };
            const auto expectJsonPost = [&](const std::string &path, const Json &expectedBody,
                                            const Json &response) {
                const auto client = acceptOne();
                const auto request = ReadRequest(client);
                if (RequestLine(request).rfind("POST " + path + " ", 0) != 0)
                    throw std::runtime_error("MayaHttp used the wrong method or route for " + path + ".");
                const auto headerEnd = request.find("\r\n\r\n");
                const auto headers = request.substr(0, headerEnd);
                if (headers.find("Content-Type: application/json\r\n") == std::string::npos ||
                    headers.find("Authorization: Bearer fixture-key\r\n") == std::string::npos)
                    throw std::runtime_error("The Models API request omitted JSON or bearer authentication.");
                if (Json::parse(request.substr(headerEnd + 4)) != expectedBody)
                    throw std::runtime_error("MayaHttp changed the Models API body for " + path + ".");
                Reply(client, 200, "OK", "application/json", response.dump());
                CloseSocket(client);
            };
            const auto expectJsonGet = [&](const std::string &path, const Json &response) {
                const auto client = acceptOne();
                const auto request = ReadRequest(client);
                if (RequestLine(request).rfind("GET " + path + " ", 0) != 0)
                    throw std::runtime_error("MayaHttp used the wrong method or route for " + path + ".");
                const auto headers = request.substr(0, request.find("\r\n\r\n"));
                if (headers.find("Authorization: Bearer fixture-key\r\n") == std::string::npos)
                    throw std::runtime_error("The Settings API request omitted bearer authentication.");
                Reply(client, 200, "OK", "application/json", response.dump());
                CloseSocket(client);
            };
            const auto apiError = [&](unsigned status, const std::string &body) {
                const auto client = acceptOne();
                const auto request = ReadRequest(client);
                if (RequestLine(request).rfind("POST /api/fixture ", 0) != 0)
                    throw std::runtime_error("MayaHttp did not send the expected API error fixture request.");
                const auto headerEnd = request.find("\r\n\r\n");
                const auto headers = request.substr(0, headerEnd);
                if (headers.find("Content-Type: application/json\r\n") == std::string::npos ||
                    headers.find("Authorization: Bearer fixture-key\r\n") == std::string::npos)
                    throw std::runtime_error("The native API request omitted JSON or bearer authentication.");
                Reply(client, status, "Fixture Error", "application/json", body);
                CloseSocket(client);
            };
            {
                const auto client = acceptOne();
                const auto request = ReadRequest(client);
                const auto first = RequestLine(request);
                if (first.rfind("POST /v1/chat/completions ", 0) != 0)
                    throw std::runtime_error("The native client did not start the expected chat stream.");
                const auto headerEnd = request.find("\r\n\r\n");
                const auto headers = request.substr(0, headerEnd);
                if (headers.find("Content-Type: application/json\r\n") == std::string::npos ||
                    headers.find("Accept: text/event-stream\r\n") == std::string::npos ||
                    headers.find("Authorization: Bearer fixture-key\r\n") == std::string::npos)
                    throw std::runtime_error("The native chat request omitted its JSON, SSE, or bearer headers.");
                const auto body = Json::parse(request.substr(headerEnd + 4));
                if (body.value("model", std::string()) != "fixture" || !body.value("stream", false) ||
                    !body.contains("messages") || !body["messages"].is_array())
                    throw std::runtime_error("The native chat request body did not preserve the stream payload.");
                const std::string events = "data: {\"id\":\"fixture id/+?\"}\n\n"
                    "data: {\"error\":null}\n\n"
                    "data: {\"choices\":[{\"delta\":{\"reasoning_content\":\"Inspecting \"}}]}\n\n"
                    "data: {\"choices\":[{\"delta\":{\"reasoning_content\":\"the prompt.\"}}]}\n\n"
                    "data: {\"choices\":[{\"delta\":{\"content\":\"alpha\"}}]}\n\n";
                ReplyChunked(client, events);
                CloseSocket(client);
            }
            {
                const auto client = acceptOne();
                resumedPath = RequestLine(ReadRequest(client));
                if (resumedPath.rfind("GET /v1/strata/stream?id=fixture%20id%2F%2B%3F&from=5 ", 0) != 0)
                    throw std::runtime_error("Resume did not continue after the five received SSE events.");
                Reply(client, 503, "Unavailable", "application/json", "{\"error\":{\"message\":\"fixture disconnect\"}}");
                CloseSocket(client);
            }
            {
                const auto client = acceptOne();
                const auto retry = RequestLine(ReadRequest(client));
                if (retry != resumedPath)
                    throw std::runtime_error("Transient failure changed the stream resume offset.");
                const std::string events = "data: {\"choices\":[{\"delta\":{\"content\":\" beta\"}}]}\n\n"
                    "data: [DONE]\n\n";
                ReplyChunked(client, events);
                CloseSocket(client);
            }
            {
                const auto client = acceptOne();
                const auto request = ReadRequest(client);
                if (RequestLine(request).rfind("POST /api/fixture ", 0) != 0)
                    throw std::runtime_error("MayaHttp did not send the successful API fixture request.");
                const auto headerEnd = request.find("\r\n\r\n");
                const auto headers = request.substr(0, headerEnd);
                if (headers.find("Content-Type: application/json\r\n") == std::string::npos ||
                    headers.find("Authorization: Bearer fixture-key\r\n") == std::string::npos)
                    throw std::runtime_error("The successful API request omitted JSON or bearer authentication.");
                const auto body = Json::parse(request.substr(headerEnd + 4));
                if (body != Json{{"test", true}})
                    throw std::runtime_error("MayaHttp changed the successful API request body.");
                Reply(client, 200, "OK", "application/json", R"({"saved":true})");
                CloseSocket(client);
            }
            expectJsonGet("/api/models", Json{{"models", Json::array({
                Json{{"file", "fixture.gguf"}, {"model", "fixture-model"}, {"running", true}}})}});
            const Json settings{{"context", 131072}, {"cpu_threads", 12}};
            const Json requestSettings{{"context", 131072}, {"cpu_threads", 12}, {"env", Json::object()}};
            expectJsonPost("/api/models/settings", Json{{"file", "fixture.gguf"}, {"settings", requestSettings}, {"dry_run", true}}, Json{{"accepted", true}});
            expectJsonPost("/api/models/settings", Json{{"file", "fixture.gguf"}, {"settings", requestSettings}}, Json{{"accepted", true}});
            expectJsonPost("/api/models/delete", Json{{"file", "fixture.gguf"}, {"files", true}, {"dry_run", true}}, Json{{"accepted", true}});
            expectJsonPost("/api/models/delete", Json{{"file", "fixture.gguf"}, {"files", true}, {"then", "fallback.json"}},
                           Json{{"deleted", true}, {"freed", 4096}});
            expectJsonPost("/api/models/leftovers", Json{{"path", "C:/fixture/orphan.bin"}},
                           Json{{"deleted", true}, {"freed", 4096}});
            expectJsonPost("/models/load", Json::object(), Json{{"accepted", true}});
            expectJsonPost("/models/unload", Json::object(), Json{{"accepted", true}});
            expectJsonPost("/api/models/switch", Json{{"file", "maya-ds4.json"}}, Json{{"restarts", true}});
            expectJsonGet("/api/context", Json{{"context", 32768}, {"min", 1024}, {"max", 272000}, {"reload", nullptr}});
            expectJsonGet("/api/port", Json{{"port", 8082}, {"saved", true}, {"restarts", true}});
            expectJsonGet("/api/thinking-budget", Json{{"budget", 1024}, {"supported", true}});
            expectJsonPost("/api/context", Json{{"max_context", 65536}}, Json{{"accepted", true}});
            expectJsonGet("/api/context", Json{{"context", 65536}, {"min", 1024}, {"max", 272000},
                {"reload", {{"state", "done"}, {"error", ""}}}});
            expectJsonPost("/api/thinking-budget", Json{{"budget", 2048}}, Json{{"budget", 2048}, {"supported", true}});
            expectJsonPost("/api/port", Json{{"port", 8182}}, Json{{"port", 8182}, {"saved", true}, {"restarts", true}});
            const Json sharedDefaults{{"reasoning_effort", "max"}, {"temperature", 0.6}};
            expectJsonPost("/settings", Json{{"defaults", sharedDefaults}}, Json{{"shared", true}, {"defaults", sharedDefaults}});
            expectJsonPost("/settings", Json{{"defaults", nullptr}}, Json{{"shared", false}, {"defaults", Json::object()}});
            expectJsonGet("/api/catalog", Json{{"models", Json::array({Json{{"id", "fixture-model"}, {"name", "Fixture"}}})},
                                                {"pull", Json{{"state", "idle"}}}});
            expectJsonGet("/api/pull/status", Json{{"state", "downloading"}, {"received", 256}, {"total", 1024}});
            expectJsonPost("/api/pull", Json{{"model", "fixture-model"}, {"detach", true}}, Json{{"state", "starting"}});
            expectJsonPost("/api/pull/cancel", Json::object(), Json{{"cancelled", true}});
            expectJsonGet("/metrics?requests=all", Json{{"live", {{"state", "idle"}}}, {"requests", Json::array()}});
            expectJsonGet("/api/savings", Json{{"usd", 0.0}, {"models", Json::array()}});
            const Json savedPrices{{"usd", 0.0}, {"models", Json::array()},
                {"price_list", Json::array({Json{{"model", "fixture-model"}, {"custom", true}}})}};
            const Json defaultPrices{{"usd", 0.0}, {"models", Json::array()},
                {"price_list", Json::array({Json{{"model", "fixture-model"}, {"custom", false}}})}};
            expectJsonPost("/api/savings/prices",
                Json{{"model", "fixture-model"}, {"price", Json{{"input", 0.15}, {"cached", 0.003}, {"output", 0.6}}}},
                savedPrices);
            expectJsonPost("/api/savings/prices", Json{{"model", "fixture-model"}, {"price", nullptr}}, defaultPrices);
            expectJsonPost("/api/savings/reset", Json::object(), defaultPrices);
            const Json turboOff{{"on", false}, {"engine", false}, {"plan", Json::object()},
                                {"env", Json::object()}, {"restarts", false}};
            const Json turboOn{{"on", true}, {"engine", true}, {"plan", Json::object()},
                               {"env", Json::object()}, {"restarts", false}};
            expectJsonGet("/api/turbo", turboOff);
            expectJsonPost("/api/turbo", Json{{"on", true}}, turboOn);
            expectJsonPost("/api/turbo", Json{{"on", false}}, turboOff);
            expectJsonPost("/api/tokens",
                Json{{"messages", Json::array({Json{{"role", "user"}, {"content", "hello"}}})},
                     {"reasoning_effort", "high"}},
                Json{{"tokens", 1}, {"approximate", false}});
            expectJsonPost("/v1/strata/cancel", Json{{"id", "fixture-job"}}, Json{{"cancelled", true}});
            {
                const auto client = acceptOne();
                const auto request = ReadRequest(client);
                if (RequestLine(request).rfind("GET /api/report ", 0) != 0)
                    throw std::runtime_error("MayaHttp did not request the troubleshooting report.");
                const auto headers = request.substr(0, request.find("\r\n\r\n"));
                if (headers.find("Authorization: Bearer fixture-key\r\n") == std::string::npos)
                    throw std::runtime_error("The report request omitted bearer authentication.");
                Reply(client, 200, "OK", "text/plain; charset=utf-8", "Maya report fixture");
                CloseSocket(client);
            }
            {
                const auto client = acceptOne();
                const auto request = ReadRequest(client);
                if (RequestLine(request).rfind("POST /attach ", 0) != 0)
                    throw std::runtime_error("MayaHttp did not POST the attachment to /attach.");
                const auto headerEnd = request.find("\r\n\r\n");
                const auto headers = request.substr(0, headerEnd);
                if (headers.find("Content-Type: application/octet-stream\r\n") == std::string::npos ||
                    headers.find("X-File-Name: report%20caf%C3%A9.pdf\r\n") == std::string::npos ||
                    headers.find("Authorization: Bearer fixture-key\r\n") == std::string::npos)
                    throw std::runtime_error("The attachment upload omitted its binary content, filename, or bearer headers.");
                if (request.substr(headerEnd + 4) != std::string("PDF\0bytes", 9))
                    throw std::runtime_error("The attachment upload changed its binary file bytes.");
                Reply(client, 200, "OK", "application/json", R"({"kind":"text","text":"attachment fixture"})");
                CloseSocket(client);
            }
            apiError(400, R"({"error":"model error"})");
            apiError(409, R"({"error":{"message":"busy"}})");
            apiError(502, "not json");
            apiError(401, R"({"error":{"message":"wrong key"}})");
        }
        catch (...) { serverError = std::current_exception(); }
    });

    try
    {
        Configuration config(fs::path(argv[1]), L"maya-ds4.json");
        config.host = L"127.0.0.1";
        config.port = port;
        config.apiKey = L"fixture-key";
        MayaHttp api(config);
        std::atomic<HINTERNET> active{nullptr};
        std::string output;
        std::string reasoning;
        int events = 0;
        api.Stream(Json{{"model", "fixture"}, {"messages", Json::array()}, {"stream", true}}, active,
                   [&](const Json &event) {
                       ++events;
                       if (event.contains("choices") && event["choices"].is_array() && !event["choices"].empty())
                       {
                           const auto delta = event["choices"][0].value("delta", Json::object());
                           reasoning += delta.value("reasoning_content", std::string());
                           output += delta.value("content", std::string());
                       }
                   });
        assert(events == 6);
        assert(reasoning == "Inspecting the prompt.");
        assert(output == "alpha beta");
        const auto saved = api.Post(L"/api/fixture", Json{{"test", true}});
        assert(saved.is_object() && saved.contains("saved") && saved["saved"].is_boolean() && saved["saved"].get<bool>());
        const auto modelList = api.Get(L"/api/models");
        assert(modelList["models"][0].value("file", std::string()) == "fixture.gguf" &&
               modelList["models"][0].value("running", false));
        const Json settings{{"context", 131072}, {"cpu_threads", 12}};
        const auto settingsPreview = api.Post(L"/api/models/settings",
            ModelSettingsRequest("fixture.gguf", settings, true));
        assert(settingsPreview.value("accepted", false));
        const auto settingsSave = api.Post(L"/api/models/settings",
            ModelSettingsRequest("fixture.gguf", settings, false));
        assert(settingsSave.value("accepted", false));
        const auto storagePreview = api.Post(L"/api/models/delete",
            ModelDeleteRequest("fixture.gguf", true));
        assert(storagePreview.value("accepted", false));
        const auto deletedModel = api.Post(L"/api/models/delete",
            ModelDeleteRequest("fixture.gguf", false, true, "fallback.json"));
        assert(deletedModel.value("deleted", false) && deletedModel.value("freed", 0) == 4096);
        const auto deletedLeftover = api.Post(L"/api/models/leftovers", ModelLeftoverRequest("C:/fixture/orphan.bin"));
        assert(deletedLeftover.value("deleted", false) && deletedLeftover.value("freed", 0) == 4096);
        assert(api.Post(L"/models/load", Json::object()).value("accepted", false));
        assert(api.Post(L"/models/unload", Json::object()).value("accepted", false));
        const auto switchedModel = api.Post(L"/api/models/switch", Json{{"file", "maya-ds4.json"}});
        assert(switchedModel.value("restarts", false));
        const auto appliedSettings = ApplySettings(api, 65536, 8182, 2048);
        assert(appliedSettings.value("context", 0) == 65536 && appliedSettings.value("port", 0) == 8182 &&
               appliedSettings.value("budget", 0) == 2048 && appliedSettings.value("port_restarts", false));
        const Json sharedDefaults{{"reasoning_effort", "max"}, {"temperature", 0.6}};
        const auto sharedSaved = SaveSharedDefaults(api, sharedDefaults);
        assert(sharedSaved.value("shared", false) && sharedSaved.value("defaults", Json::object()) == sharedDefaults);
        const auto sharedCleared = SaveSharedDefaults(api, Json(nullptr));
        assert(!sharedCleared.value("shared", true) && sharedCleared.value("defaults", Json::object()).empty());
        const auto catalogFixture = api.Get(L"/api/catalog");
        assert(catalogFixture["models"][0].value("id", std::string()) == "fixture-model");
        const auto pullStatusFixture = api.Get(L"/api/pull/status");
        assert(pullStatusFixture.value("state", std::string()) == "downloading" &&
               pullStatusFixture.value("received", 0) == 256);
        const auto pullStartFixture = api.Post(L"/api/pull", Json{{"model", "fixture-model"}, {"detach", true}});
        assert(pullStartFixture.value("state", std::string()) == "starting");
        const auto pullCancelFixture = api.Post(L"/api/pull/cancel", Json::object());
        assert(pullCancelFixture.value("cancelled", false));
        const auto allRequestsMetrics = api.Get(L"/metrics?requests=all");
        assert(allRequestsMetrics["live"].value("state", std::string()) == "idle" &&
               allRequestsMetrics["requests"].is_array());
        const auto savings = api.Get(L"/api/savings");
        assert(savings.value("usd", -1.0) == 0.0 && savings["models"].is_array());
        const Json editedPrices{{"model", "fixture-model"},
            {"price", Json{{"input", 0.15}, {"cached", 0.003}, {"output", 0.6}}}};
        const auto priceSaved = api.Post(L"/api/savings/prices", editedPrices);
        assert(priceSaved["price_list"][0].value("model", std::string()) == "fixture-model" &&
               priceSaved["price_list"][0].value("custom", false));
        const auto priceDefaulted = api.Post(L"/api/savings/prices",
            Json{{"model", "fixture-model"}, {"price", nullptr}});
        assert(!priceDefaulted["price_list"][0].value("custom", true));
        const auto savingsReset = api.Post(L"/api/savings/reset", Json::object());
        assert(savingsReset.value("usd", -1.0) == 0.0 && savingsReset["models"].is_array());
        const auto turboBefore = api.Get(L"/api/turbo");
        assert(!turboBefore.value("on", true));
        const auto turboEnabled = api.Post(L"/api/turbo", Json{{"on", true}});
        assert(turboEnabled.value("on", false) && turboEnabled.value("engine", false));
        const auto turboDisabled = api.Post(L"/api/turbo", Json{{"on", false}});
        assert(!turboDisabled.value("on", true));
        const Json tokenRequest{{"messages", Json::array({Json{{"role", "user"}, {"content", "hello"}}})},
                                {"reasoning_effort", "high"}};
        const auto tokenCount = api.Post(L"/api/tokens", tokenRequest);
        assert(tokenCount.value("tokens", 0) == 1 && !tokenCount.value("approximate", true));
        const auto cancelledChat = api.Post(L"/v1/strata/cancel", Json{{"id", "fixture-job"}});
        assert(cancelledChat.value("cancelled", false));
        const auto report = api.Request(L"GET", L"/api/report");
        assert(report.status == 200 && report.body == "Maya report fixture");
        std::atomic_bool cancelled{false};
        const auto attachment = api.ExtractAttachment("report%20caf%C3%A9.pdf", std::string("PDF\0bytes", 9), active, cancelled);
        assert(attachment.is_object() && attachment.value("kind", std::string()) == "text" &&
               attachment.value("text", std::string()) == "attachment fixture");
        assert(active.load() == nullptr);
        const auto expectApiError = [&](unsigned status, const std::string &message) {
            try
            {
                api.Post(L"/api/fixture", Json{{"test", true}});
            }
            catch (const MayaHttpError &error)
            {
                assert(error.status == status);
                assert(error.what() == message);
                return;
            }
            throw std::runtime_error("MayaHttp accepted a non-success API response.");
        };
        expectApiError(400, "model error");
        expectApiError(409, "busy");
        expectApiError(502, "HTTP 502");
        expectApiError(401, NativeChatApiKeyRequiredMessage);
        server.join();
        if (serverError) std::rethrow_exception(serverError);
        CloseSocket(listener);
        WSACleanup();
        std::cout << "Chat stream, model management, catalog, and switch HTTP fixtures passed\n";
        return 0;
    }
    catch (const std::exception &error)
    {
        if (server.joinable()) server.join();
        CloseSocket(listener);
        WSACleanup();
        std::cerr << error.what() << "\n";
        return 5;
    }
}
