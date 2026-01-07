#pragma once

#include <string>
#include <vector>
#include <unordered_map>
#include "GridFramework.h"

namespace gird {

// ---- Serializable State (what we persist) ----

struct GridState
{
    // Column visibility: column_id -> visible
    std::unordered_map<std::string, bool> columnVisibility;
    
    // Grouping: list of column IDs (in order)
    std::vector<std::string> groupByColumnIds;
    
    // Sorting: list of {column_id, direction}
    std::vector<SortKey> sortKeys;
    
    // Aggregation: list of active aggs
    std::vector<AggDef> activeAggs;
    
    // Display preferences
    bool showDetailRows = true;
    bool showGrandTotal = false;
    bool showGroupHeaders = true;
    
    // Serialization methods
    std::string ToJson() const;
    bool FromJson(const std::string &json);
};

// ---- Persistence Interface (abstract for multiple backends) ----

class IPersistence
{
public:
    virtual ~IPersistence() = default;
    
    // Save state to storage
    virtual bool Save(const std::string &key, const GridState &state) = 0;
    
    // Load state from storage (returns empty state if not found)
    virtual bool Load(const std::string &key, GridState &outState) = 0;
    
    // Clear saved state
    virtual bool Clear(const std::string &key) = 0;

    // List all presets matching a prefix (e.g., "main_grid" returns all "main_grid_*" presets)
    virtual std::vector<std::string> ListPresets(const std::string &baseKey) = 0;
};

// ---- JSON Persistence (works on native + web via JSON export/import) ----

class JsonPersistence : public IPersistence
{
public:
    // Native: saves to file path
    // Web: uses localStorage via JavaScript binding
    explicit JsonPersistence(const std::string &storagePath = "");

    bool Save(const std::string &key, const GridState &state) override;
    bool Load(const std::string &key, GridState &outState) override;
    bool Clear(const std::string &key) override;
    std::vector<std::string> ListPresets(const std::string &baseKey) override;
    
private:
    std::string storagePath;
    
    std::string StateToJson(const GridState &state) const;
    bool JsonToState(const std::string &json, GridState &outState);
};

// ---- Helper: Extract state from GridViewModel + GridDocument ----

GridState ExtractGridState(const GridDocument &doc, const GridViewModel &vm);

// ---- Helper: Apply saved state to GridViewModel ----

void ApplyGridState(const GridState &state, GridDocument &doc, GridViewModel &vm);

} // namespace gird