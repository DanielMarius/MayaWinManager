#pragma once
#include "../client.h"
#include <algorithm>
#include <iomanip>
#include <sstream>
#include <string>
#include <vector>

inline std::string MayaAboutFacts(const Json &health, const Json &metrics, const std::string &apiAddress)
{
    const auto engine = metrics.is_object() && metrics.contains("engine") && metrics["engine"].is_object()
                            ? metrics["engine"] : Json::object();
    const auto hardware = metrics.is_object() && metrics.contains("hardware") && metrics["hardware"].is_object()
                              ? metrics["hardware"] : Json::object();
    const auto fixed = [](double value, int digits)
    {
        std::ostringstream out;
        out << std::fixed << std::setprecision(digits) << value;
        auto text = out.str();
        while (text.find('.') != std::string::npos && text.back() == '0') text.pop_back();
        if (!text.empty() && text.back() == '.') text.pop_back();
        return text;
    };
    const auto count = [](const Json &object, const char *key, double fallback = 0.0)
    {
        return object.is_object() && object.contains(key) && object[key].is_number()
                   ? object[key].get<double>() : fallback;
    };
    const auto text = [](const Json &object, const char *key, const std::string &fallback = std::string())
    {
        return object.is_object() && object.contains(key) && object[key].is_string()
                   ? object[key].get<std::string>() : fallback;
    };
    const auto gigabytes = [&](const Json &object, const char *key, int digits = 1)
    {
        return fixed(count(object, key) / 1073741824.0, digits);
    };
    const auto healthString = [&](const char *key, const std::string &fallback = std::string())
    {
        return text(health, key, fallback);
    };
    std::vector<std::string> facts;
    const auto version = text(engine, "maya_version", healthString("version", "?"));
    const auto engineVersion = text(engine, "version");
    facts.push_back("Version: Project Maya v" + version + (engineVersion.empty() ? "" : " · engine " + engineVersion));
    facts.push_back("Model: " + text(engine, "model", healthString("model_label", healthString("model", "unknown"))));
    const auto kind = text(engine, "engine_kind");
    if (!kind.empty())
    {
        const std::pair<const char *, const char *> names[] = {
            {"glm-fast", "GLM fast path"}, {"ds4", "DeepSeek-V4.1 (maya-ds4)"},
            {"mm3", "MiniMax-M3.1 (maya-mm3)"}};
        std::string name = kind;
        for (const auto &[id, label] : names) if (kind == id) name = label;
        facts.push_back("Engine: " + name);
    }
    const double maxContext = count(engine, "max_context", count(health, "max_context"));
    if (maxContext > 0) facts.push_back("Context: " + fixed(maxContext, 0) + " tokens");
    const double trained = count(engine, "trained_context");
    if (trained > 0) facts.push_back("Trained for: up to " + fixed(trained, 0) + " tokens");

    std::string kv;
    const auto kvFormat = text(engine, "kv");
    const auto kvWindow = count(engine, "kv_resident");
    const double kvGb = count(engine, "kv_gb"), kvContext = count(engine, "kv_ctx");
    if (kind == "glm-fast")
    {
        const auto label = kvFormat == "int8" ? "8-bit (INT8)" : "16-bit";
        if (kvWindow > 0)
            kv = std::string(label) + " attention cache, streamed: " + fixed(kvWindow, 0) + " positions per layer in VRAM, the rest in RAM";
        else
            kv = std::string(label) + " attention cache, in VRAM" +
                 ((kvGb > 0 && kvContext > 0) ? " (about " + fixed(kvGb * 1024.0 / kvContext, 1) + " KB a token, " + fixed(kvGb, 1) + " GB at this size)" : "");
    }
    else if (!kvFormat.empty())
    {
        if (kind == "mm3")
        {
            const std::pair<const char *, const char *> formats[] = {
                {"bf16", "16-bit (BF16)"}, {"int8", "8-bit (INT8)"}, {"fp8", "8-bit (FP8)"},
                {"fp4", "4-bit (FP4)"}, {"nvfp4", "4-bit (NVFP4)"}, {"mxfp4", "4-bit (MXFP4)"}, {"q4", "4-bit"}};
            kv = kvFormat;
            for (const auto &[id, label] : formats) if (kvFormat == id) kv = label;
        }
        else
        {
            const std::pair<const char *, const char *> formats[] = {
                {"int8", "8-bit"}, {"q4_0", "4-bit (Hadamard-rotated)"}, {"fp16", "16-bit"},
                {"bf16", "16-bit"}, {"f32", "32-bit"}, {"fp4", "4-bit FP4 as DeepSeek stores it (the recent-token window FP8)"}};
            kv = kvFormat;
            for (const auto &[id, label] : formats) if (kvFormat == id) kv = label;
        }
        const bool streamed = kvWindow > 0 && (maxContext <= 0 || kvWindow < maxContext);
        kv += streamed ? ", streamed: " + fixed(kvWindow, 0) + " positions per layer in VRAM, the rest in RAM" : ", all in VRAM";
    }
    if (!kv.empty()) facts.push_back("KV cache: " + kv);

    const double experts = count(engine, "experts"), expertSlots = count(engine, "expert_slots"), ramSlots = count(engine, "ram_slots");
    if (experts > 0)
        facts.push_back("Experts: " + fixed(experts, 0) + " routed; room for " + fixed(expertSlots, 0) + " in VRAM" +
                        (ramSlots > 0 ? " and " + fixed(ramSlots, 0) + " in RAM" : "") +
                        (expertSlots + ramSlots >= experts ? "" : ", the rest read from the SSD"));
    if (engine.contains("dspark") && (engine["dspark"].is_number() || engine["dspark"].is_boolean()))
    {
        const bool enabled = engine["dspark"].is_boolean() ? engine["dspark"].get<bool>() : engine["dspark"].get<double>() != 0;
        std::string speculation = enabled
                                     ? "DSpark: its drafter proposes a few tokens at a time, the full model checks them"
                                     : "off (the config's --dspark, or no DSpark drafter in this pack)";
        const auto tiers = metrics.is_object() && metrics.contains("tiers") && metrics["tiers"].is_object()
                               ? metrics["tiers"] : Json::object();
        const auto now = tiers.value("now", Json::object());
        if (enabled && now.is_object() && now.contains("drafts") && now["drafts"].is_number())
            speculation += " (" + fixed(100.0 * now["drafts"].get<double>(), 0) + "% of them kept in the last answer)";
        facts.push_back("Speculation: " + speculation);
    }
    else if (engine.contains("mtp") && (engine["mtp"].is_boolean() || engine["mtp"].is_number()))
    {
        const bool enabled = engine["mtp"].is_boolean() ? engine["mtp"].get<bool>() : engine["mtp"].get<double>() != 0;
        facts.push_back(std::string("Speculation: ") + (enabled
            ? "MTP: the model's draft block proposes the next token, the full model checks it"
            : "off (one GPU, or no draft block in this model file)"));
    }

    const bool images = engine.contains("images") && engine["images"].is_boolean()
                            ? engine["images"].get<bool>() : health.value("images", false);
    facts.push_back(std::string("Images: ") + (images ? "On" : "Off"));
    const auto staticHardware = metrics.is_object() && metrics.contains("hardware_static") && metrics["hardware_static"].is_object()
                                    ? metrics["hardware_static"] : Json::object();
    const auto gpus = hardware.value("gpus", Json::array());
    const auto gpuName = text(staticHardware, "gpu_name");
    if (!gpuName.empty())
    {
        std::vector<std::string> names;
        size_t start = 0;
        while (start < gpuName.size())
        {
            const auto end = gpuName.find(" + ", start);
            names.push_back(gpuName.substr(start, end == std::string::npos ? end : end - start));
            if (end == std::string::npos) break;
            start = end + 3;
        }
        std::vector<std::string> groupedNames;
        for (size_t i = 0; i < names.size(); ++i)
        {
            if (std::find(names.begin(), names.begin() + i, names[i]) != names.begin() + i) continue;
            const auto repeated = (size_t)std::count(names.begin(), names.end(), names[i]);
            groupedNames.push_back((repeated > 1 ? std::to_string(repeated) + " × " : "") + names[i]);
        }
        std::string grouped;
        for (size_t i = 0; i < groupedNames.size(); ++i) grouped += (i ? " + " : "") + groupedNames[i];
        if (names.size() == 1 && gpus.is_array() && gpus.size() > 1)
            grouped = std::to_string(gpus.size()) + " × " + grouped;
        facts.push_back("GPU: " + grouped + (count(hardware, "gpu_mem_total") > 0
                          ? " · " + fixed(count(hardware, "gpu_mem_total") / 1073741824.0, 0) + " GB" : ""));
    }
    else facts.push_back("GPU: not readable (NVML)");

    const auto pcieLink = [&](const Json &gpu)
    {
        const auto generation = gpu.is_object() && gpu.contains("pcie_gen_max") && gpu["pcie_gen_max"].is_number()
                                    ? gpu["pcie_gen_max"].get<double>()
                                    : gpu.is_object() && gpu.contains("pcie_gen") && gpu["pcie_gen"].is_number()
                                          ? gpu["pcie_gen"].get<double>() : 0.0;
        std::string link = "Gen" + (generation == 0 ? std::string("?") : fixed(generation, 0));
        if (gpu.is_object() && gpu.contains("pcie_width") && gpu["pcie_width"].is_number())
            link += " x" + fixed(gpu["pcie_width"].get<double>(), 0);
        return link;
    };
    if (gpus.is_array() && gpus.size() > 1)
    {
        bool same = true;
        for (size_t i = 1; i < gpus.size(); ++i) if (pcieLink(gpus[i]) != pcieLink(gpus[0])) same = false;
        if (same) facts.push_back("PCIe: " + pcieLink(gpus[0]) + " on all " + std::to_string(gpus.size()) + " GPUs");
        else
        {
            std::string links;
            for (size_t i = 0; i < gpus.size(); ++i)
            {
                if (i) links += " · ";
                links += "GPU " + fixed(count(gpus[i], "index", (double)i), 0) + ": " + pcieLink(gpus[i]);
            }
            facts.push_back("PCIe: " + links);
        }
    }
    else if (count(hardware, "gpu_pcie_gen_max", count(hardware, "gpu_pcie_gen")) > 0)
    {
        std::string link = "Gen" + fixed(count(hardware, "gpu_pcie_gen_max", count(hardware, "gpu_pcie_gen")), 0);
        if (count(hardware, "gpu_pcie_width") > 0) link += " x" + fixed(count(hardware, "gpu_pcie_width"), 0);
        facts.push_back("PCIe: " + link);
    }
    const auto cpu = text(staticHardware, "cpu_name");
    if (!cpu.empty()) facts.push_back("CPU: " + cpu + (count(staticHardware, "threads") > 0
                                                       ? ", " + fixed(count(staticHardware, "threads"), 0) + " threads" : ""));
    if (count(hardware, "ram_total") > 0) facts.push_back("RAM: " + fixed(count(hardware, "ram_total") / 1073741824.0, 0) + " GB");
    const auto storage = metrics.is_object() && metrics.contains("storage") && metrics["storage"].is_object()
                             ? metrics["storage"] : Json::object();
    if (!text(storage, "path").empty())
        facts.push_back("Model folder: " + text(storage, "path") + " · " + gigabytes(storage, "free", 0) +
                        " GB free of " + gigabytes(storage, "total", 0) + " GB");
    facts.push_back("API: " + apiAddress);
    facts.emplace_back("See Monitor for live engine readings. Use Check updates above to check the latest release.");
    std::ostringstream out;
    for (size_t i = 0; i < facts.size(); ++i) out << (i ? "\r\n" : "") << facts[i];
    return out.str();
}
