#pragma once
#include "http.h"
#include <cstddef>
#include <vector>

inline std::vector<std::string> ModelDeleteTargets(const Json &models)
{
    std::vector<std::string> targets;
    if (!models.is_array()) return targets;
    for (const auto &model : models)
        if (model.is_object() && !model.value("running", false) && model.value("ok", false) &&
            model.contains("file") && model["file"].is_string() && !model["file"].get<std::string>().empty())
            targets.push_back(model["file"].get<std::string>());
    return targets;
}

struct ModelDeleteReviewSelection
{
    bool accepted = false, deleteFiles = false;
    std::string then;
};

inline ModelDeleteReviewSelection ResolveModelDeleteReview(bool accepted, bool running, bool canDeleteFiles,
                                                            bool deleteFiles,
                                                            const std::vector<std::string> &targets,
                                                            std::size_t selected)
{
    if (!accepted || (running && selected >= targets.size())) return {};
    return {true, canDeleteFiles && deleteFiles, running ? targets[selected] : std::string()};
}

inline std::string ModelDeleteOutcomeKey(const Json &outcome)
{
    if (!outcome.is_object() || !outcome.contains("time") || outcome["time"].is_null()) return {};
    return outcome["time"].is_string() ? outcome["time"].get<std::string>() : outcome["time"].dump();
}

inline bool ModelDeleteOutcomeIncludesFiles(const Json &outcome)
{
    return outcome.is_object() && outcome.contains("files") && outcome["files"].is_boolean() &&
           outcome["files"].get<bool>();
}

inline uint64_t ModelDeleteOutcomeFreed(const Json &outcome)
{
    if (!outcome.is_object() || !outcome.contains("freed")) return 0;
    const auto &freed = outcome["freed"];
    if (freed.is_number_unsigned()) return freed.get<uint64_t>();
    return freed.is_number_integer() && freed.get<int64_t>() > 0 ? (uint64_t)freed.get<int64_t>() : 0;
}

inline std::string ModelDeleteOutcomeText(const Json &outcome)
{
    if (!outcome.is_object()) return {};
    const auto stringField = [&](const char *key) {
        return outcome.contains(key) && outcome[key].is_string() ? outcome[key].get<std::string>() : std::string();
    };
    const auto file = stringField("file");
    const auto error = stringField("error");
    if (!error.empty()) return (file.empty() ? "Model" : file) + " was not deleted: " + error;
    auto name = stringField("model");
    const auto quant = stringField("quant");
    if (name.empty())
    {
        name = stringField("deleted");
        if (name.empty()) name = file;
    }
    if (name.empty()) return {};
    else if (!quant.empty()) name += " " + quant;
    std::string text = name + " deleted.";
    if (ModelDeleteOutcomeIncludesFiles(outcome))
        text += " Its downloaded files were also removed.";
    if (outcome.contains("drafter_users") && outcome["drafter_users"].is_array() && !outcome["drafter_users"].empty())
    {
        text += " Models now using MTP: ";
        bool first = true;
        for (const auto &user : outcome["drafter_users"])
            if (user.is_string())
            {
                if (!first) text += ", ";
                text += user.get<std::string>();
                first = false;
            }
        if (!first) text += ".";
    }
    return text;
}

inline bool ModelDeleteEnabled(const Json &model, const Json &models, bool restarts)
{
    return !model.value("running", false) || (restarts && !ModelDeleteTargets(models).empty());
}

inline std::string ModelDeleteItemLabel(const Json &item)
{
    if (!item.is_object() || !item.contains("what") || !item["what"].is_string()) return "Model file";
    const auto what = item["what"].get<std::string>();
    if (what == "folder") return "Model folder with its weights and generated pack";
    if (what == "pack") return "Maya-built model pack";
    if (what == "gguf") return "GGUF model file";
    if (what == "vision") return "Image encoder";
    if (what == "drafter") return "DFlash2 drafter";
    return "Model file";
}

inline uint64_t ModelDeletePlanByteCount(const Json &value)
{
    if (value.is_number_unsigned()) return value.get<uint64_t>();
    if (value.is_number_integer() && value.get<int64_t>() > 0) return (uint64_t)value.get<int64_t>();
    return 0;
}

inline Json ModelDeletePlanFailure(const std::string &why)
{
    return Json{{"ok", false}, {"why", why}, {"items", Json::array()}};
}

inline std::string ModelDeletePlanReport(const Json &plan)
{
    if (!plan.is_object()) return "Maya returned an invalid storage plan.";
    if (!plan.contains("ok") || !plan["ok"].is_boolean() || !plan["ok"].get<bool>())
    {
        const auto why = plan.contains("why") && plan["why"].is_string()
            ? plan["why"].get<std::string>() : std::string("Maya could not verify them.");
        return "The model config can be removed, but its files stay: " +
               why;
    }

    const auto items = plan.value("items", Json::array());
    std::string text;
    if (!items.is_array() || items.empty())
        text = "No downloaded model files were found to remove. Deleting the model only takes its config off the list.";
    else
    {
        const auto totalBytes = plan.contains("bytes") ? ModelDeletePlanByteCount(plan["bytes"]) : 0;
        text = "Files Maya would remove (" + std::to_string(totalBytes) + " bytes):";
        for (const auto &item : items)
        {
            const auto bytes = item.is_object() && item.contains("bytes") ? ModelDeletePlanByteCount(item["bytes"]) : 0;
            const auto path = item.is_object() && item.contains("path") && item["path"].is_string()
                ? item["path"].get<std::string>() : std::string("path not provided");
            text += "\n\n" + ModelDeleteItemLabel(item) + " · " + std::to_string(bytes) + " bytes\n" + path;
        }
    }
    const auto shared = plan.value("shared", Json::array());
    if (shared.is_array() && !shared.empty())
    {
        text += "\n\nKept because another model uses these:";
        for (const auto &item : shared)
        {
            text += "\n" + ModelDeleteItemLabel(item);
            if (item.is_object() && item.contains("path") && item["path"].is_string())
                text += "\n" + item["path"].get<std::string>();
            if (item.is_object() && item.contains("users") && item["users"].is_array())
            {
                text += "\nUsed by: ";
                bool first = true;
                for (const auto &user : item["users"])
                    if (user.is_string())
                    {
                        if (!first) text += ", ";
                        text += user.get<std::string>();
                        first = false;
                    }
            }
        }
    }
    if (plan.contains("linked") && ModelDeletePlanByteCount(plan["linked"]) > 0)
        text += "\n\nSome files are hard links to another model's copy; deleting them frees no additional space.";
    const auto kept = plan.value("kept", Json::array());
    if (kept.is_array())
        for (const auto &item : kept)
            if (item.is_string()) text += "\n\nKept: " + item.get<std::string>();
    return text;
}

inline Json ModelDeleteRequest(const std::string &file, bool dryRun, bool files = false,
                               const std::string &then = {})
{
    Json request{{"file", file}, {"files", dryRun || files}};
    if (dryRun)
        request["dry_run"] = true;
    else
        request["then"] = then.empty() ? Json(nullptr) : Json(then);
    return request;
}
