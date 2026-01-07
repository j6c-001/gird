#include "GridPersistence.h"
#include <fstream>
#include <sstream>
#include <algorithm>
#include <filesystem>

#ifdef EMSCRIPTEN
#include <emscripten.h>
#endif

namespace gird {

// ============================================================
// JSON Serialization / Deserialization
// ============================================================

std::string GridState::ToJson() const
{
    // Simple JSON construction (in production, use a proper JSON library)
    std::ostringstream oss;
    oss << "{";

    // Column visibility
    oss << "\"columnVisibility\":{";
    bool first = true;
    for (const auto &[colId, visible] : columnVisibility)
    {
        if (!first) oss << ",";
        oss << "\"" << colId << "\":" << (visible ? "true" : "false");
        first = false;
    }
    oss << "},";

    // Group by columns
    oss << "\"groupByColumnIds\":[";
    for (int i = 0; i < (int)groupByColumnIds.size(); ++i)
    {
        if (i > 0) oss << ",";
        oss << "\"" << groupByColumnIds[i] << "\"";
    }
    oss << "],";

    // Sort keys
    oss << "\"sortKeys\":[";
    for (int i = 0; i < (int)sortKeys.size(); ++i)
    {
        if (i > 0) oss << ",";
        oss << "{\"columnId\":\"" << sortKeys[i].column_id << "\",";
        oss << "\"dir\":" << (sortKeys[i].dir == SortDir::Asc ? "0" : "1") << "}";
    }
    oss << "],";

    // Active aggs
    oss << "\"activeAggs\":[";
    for (int i = 0; i < (int)activeAggs.size(); ++i)
    {
        if (i > 0) oss << ",";
        oss << "{\"columnId\":\"" << activeAggs[i].column_id << "\",";
        oss << "\"type\":" << (int)activeAggs[i].type << "}";
    }
    oss << "],";

    // Display preferences
    oss << "\"showDetailRows\":" << (showDetailRows ? "true" : "false") << ",";
    oss << "\"showGrandTotal\":" << (showGrandTotal ? "true" : "false") << ",";
    oss << "\"showGroupHeaders\":" << (showGroupHeaders ? "true" : "false");

    oss << "}";
    return oss.str();
}

// Simple JSON parser (for production, use nlohmann/json or similar)
bool GridState::FromJson(const std::string &json)
{
    // This is a simplified version. In production, use a proper JSON library!
    // For now, we parse just the essential fields with string operations.

    // Parse columnVisibility
    size_t colVisStart = json.find("\"columnVisibility\":{");
    if (colVisStart != std::string::npos)
    {
        size_t start = json.find('{', colVisStart);
        size_t end = json.find('}', start);
        std::string colVisJson = json.substr(start + 1, end - start - 1);

        // Simple parsing: "colId":true,"colId2":false
        size_t pos = 0;
        while (pos < colVisJson.size())
        {
            size_t keyStart = colVisJson.find('"', pos);
            if (keyStart == std::string::npos) break;
            size_t keyEnd = colVisJson.find('"', keyStart + 1);
            if (keyEnd == std::string::npos) break;

            std::string key = colVisJson.substr(keyStart + 1, keyEnd - keyStart - 1);

            size_t valStart = colVisJson.find(':', keyEnd);
            if (valStart == std::string::npos) break;

            bool val = colVisJson.substr(valStart + 1, 4) == "true";
            columnVisibility[key] = val;

            pos = colVisJson.find(',', valStart);
            if (pos == std::string::npos) break;
            pos++;
        }
    }

    // Parse groupByColumnIds
    size_t groupStart = json.find("\"groupByColumnIds\":[");
    if (groupStart != std::string::npos)
    {
        size_t start = json.find('[', groupStart);
        size_t end = json.find(']', start);
        std::string groupJson = json.substr(start + 1, end - start - 1);

        size_t pos = 0;
        while (pos < groupJson.size())
        {
            size_t keyStart = groupJson.find('"', pos);
            if (keyStart == std::string::npos) break;
            size_t keyEnd = groupJson.find('"', keyStart + 1);
            if (keyEnd == std::string::npos) break;

            groupByColumnIds.push_back(groupJson.substr(keyStart + 1, keyEnd - keyStart - 1));

            pos = groupJson.find(',', keyEnd);
            if (pos == std::string::npos) break;
            pos++;
        }
    }

    // Parse showDetailRows, showGrandTotal, showGroupHeaders
    showDetailRows = (json.find("\"showDetailRows\":true") != std::string::npos);
    showGrandTotal = (json.find("\"showGrandTotal\":true") != std::string::npos);
    showGroupHeaders = (json.find("\"showGroupHeaders\":true") != std::string::npos);

    return true;
}

// ============================================================
// JsonPersistence Implementation
// ============================================================

JsonPersistence::JsonPersistence(const std::string &path)
    : storagePath(path)
{
}

bool JsonPersistence::Save(const std::string &key, const GridState &state)
{
#ifdef EMSCRIPTEN
    // Web: use localStorage via JavaScript
    std::string json = state.ToJson();

    // JavaScript interop: save to localStorage
    EM_ASM_ARGS({
        const key = UTF8ToString($0);
        const json = UTF8ToString($1);
        try {
            localStorage.setItem('grid_' + key, json);
            return 1;
        } catch (e) {
            console.error("Failed to save grid state:", e);
            return 0;
        }
    }, key.c_str(), json.c_str());

    return true;
#else
    // Native: save to file (e.g., ~/.config/app/grid_state.json)
    if (storagePath.empty())
        return false;

    std::string filepath = storagePath + "/grid_" + key + ".json";
    std::string json = state.ToJson();

    std::ofstream file(filepath);
    if (!file.is_open())
        return false;

    file << json;
    file.close();
    return true;
#endif
}

bool JsonPersistence::Load(const std::string &key, GridState &outState)
{
#ifdef EMSCRIPTEN
    const char* json_str = (const char*)EM_ASM_INT({
        const key = UTF8ToString($0);
        const json = localStorage.getItem('grid_' + key);

        if (!json) return 0;

        // Allocate memory and copy string from JS to WASM
        const len = lengthBytesUTF8(json) + 1;
        const ptr = _malloc(len);
        stringToUTF8(json, ptr, len);
        return ptr;
    }, key.c_str());

    if (!json_str || json_str == (const char*)0)
        return false;

    std::string json(json_str);
    free((void*)json_str);  // Clean up

    return outState.FromJson(json);
#else
    // Native: load from file
    if (storagePath.empty())
        return false;

    std::string filepath = storagePath + "/grid_" + key + ".json";
    std::ifstream file(filepath);
    if (!file.is_open())
        return false;

    std::ostringstream buffer;
    buffer << file.rdbuf();
    std::string json = buffer.str();
    file.close();

    return outState.FromJson(json);
#endif
}

bool JsonPersistence::Clear(const std::string &key)
{
#ifdef EMSCRIPTEN
    EM_ASM_ARGS({
        const key = UTF8ToString($0);
        try {
            localStorage.removeItem('grid_' + key);
            return 1;
        } catch (e) {
            console.error("Failed to clear grid state:", e);
            return 0;
        }
    }, key.c_str());

    return true;
#else
    if (storagePath.empty())
        return false;

    std::string filepath = storagePath + "/grid_" + key + ".json";
    return std::remove(filepath.c_str()) == 0;
#endif
}

// ============================================================
// State Extraction & Application
// ============================================================

GridState ExtractGridState(const GridDocument &doc, const GridViewModel &vm)
{
    GridState state;

    // Extract column visibility
    state.columnVisibility = vm.colVisible;

    // Extract grouping
    state.groupByColumnIds = vm.groupByColumnIds;

    // Extract sorting
    state.sortKeys = vm.activeSortKeys;

    // Extract active aggregations
    state.activeAggs = vm.active_aggs;

    // Extract display preferences
    state.showDetailRows = vm.showDetailRows;
    state.showGrandTotal = vm.showGrandTotal;
    state.showGroupHeaders = vm.showGroupHeaders;

    return state;
}

void ApplyGridState(const GridState &state, GridDocument &doc, GridViewModel &vm)
{
    // Apply column visibility
    vm.colVisible = state.columnVisibility;
    vm.dirtyViewColumns = true;

    // Apply grouping
    vm.groupByColumnIds = state.groupByColumnIds;
    vm.dirtyGroups = true;

    // Apply sorting
    vm.activeSortKeys = state.sortKeys;
    vm.dirtyIndices = true;

    // Apply aggregations
    vm.active_aggs = state.activeAggs;
    vm.dirtyViewColumns = true;  // Rebuild view columns to include aggs

    // Apply display preferences
    vm.showDetailRows = state.showDetailRows;
    vm.showGrandTotal = state.showGrandTotal;
    vm.showGroupHeaders = state.showGroupHeaders;
    vm.dirtyGroups = true;
    vm.dirtyRenderRows = true;
}


std::vector<std::string> JsonPersistence::ListPresets(const std::string &baseKey)
{
    std::vector<std::string> presets;

#ifdef EMSCRIPTEN
    // Web: enumerate localStorage keys matching pattern
    const char* presets_json = (const char*)EM_ASM_INT({
        const baseKey = UTF8ToString($0);
        const presets = [];
        const prefix = 'grid_' + baseKey + '_';

        for (let i = 0; i < localStorage.length; i++) {
            const key = localStorage.key(i);
            if (key.startsWith(prefix)) {
                // Extract preset name (remove 'grid_' + baseKey + '_' prefix)
                const presetName = key.substring(prefix.length);
                presets.push(presetName);
            }
        }

        // Convert array to JSON and return as string
        const json = JSON.stringify(presets);
        const len = lengthBytesUTF8(json) + 1;
        const ptr = _malloc(len);
        stringToUTF8(json, ptr, len);
        return ptr;
    }, baseKey.c_str());

    if (presets_json && presets_json != (const char*)0)
    {
        // Parse simple JSON array: ["preset1", "preset2", ...]
        std::string json_str(presets_json);

        // Extract preset names from JSON array
        size_t pos = 0;
        while (pos < json_str.size())
        {
            size_t start = json_str.find('"', pos);
            if (start == std::string::npos) break;

            size_t end = json_str.find('"', start + 1);
            if (end == std::string::npos) break;

            std::string preset = json_str.substr(start + 1, end - start - 1);
            if (!preset.empty())
            {
                presets.push_back(preset);
            }

            pos = end + 1;
        }

        free((void*)presets_json);  // Clean up allocated memory
    }

    return presets;
#else    // Native: scan filesystem for grid_[baseKey]_*.json files
    if (storagePath.empty())
        return presets;

    try {
        for (const auto& entry : std::filesystem::directory_iterator(storagePath))
        {
            if (entry.is_regular_file())
            {
                std::string filename = entry.path().filename().string();
                std::string prefix = "grid_" + baseKey + "_";
                std::string suffix = ".json";

                // Check if filename matches pattern: grid_[baseKey]_*.json
                if (filename.substr(0, prefix.length()) == prefix &&
                    filename.substr(filename.length() - suffix.length()) == suffix)
                {
                    // Extract preset name (remove prefix and suffix)
                    std::string presetName = filename.substr(
                        prefix.length(),
                        filename.length() - prefix.length() - suffix.length()
                    );
                    presets.push_back(presetName);
                }
            }
        }

        // Sort alphabetically
        std::sort(presets.begin(), presets.end());
    }
    catch (const std::exception& e)
    {
        // Silently ignore errors (directory might not exist)
    }

    return presets;
#endif
}
} // namespace gird