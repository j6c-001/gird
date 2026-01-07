#pragma once
#include <cstdint>
#include <functional>
#include <optional>
#include <string>
#include <variant>
#include <vector>

struct ImFont; // forward decl (keeps this header mostly UI-agnostic)

namespace gird
{
class IPersistence;

// ---- Value / typing ----
enum class ValueType
{
    String,
    Int64,
    Double,
    Bool
};

using Value = std::variant<std::string, int64_t, double, bool>;

using SimpleRow = std::vector<Value>;

inline std::string ValueToString(const Value& v) {
    if (std::holds_alternative<std::string>(v)) {
        return std::get<std::string>(v);
    } else if (std::holds_alternative<int64_t>(v)) {
        return std::to_string(std::get<int64_t>(v));
    } else if (std::holds_alternative<double>(v)) {
        char buf[64];
        snprintf(buf, sizeof(buf), "%.2f", std::get<double>(v));
        return std::string(buf);
    }
    return "";
}

struct IRowSource
{
    virtual ~IRowSource() = default;
    virtual int RowCount() const = 0;
    virtual const SimpleRow &RowAt(int row_index) const = 0;
};

// ---- Column definition (dictionary entry) ----
struct CellStyle
{
    std::optional<uint32_t> bg_rgba;   // packed ImU32 later
    std::optional<uint32_t> text_rgba; // packed ImU32 later
    ImFont *font = nullptr;            // optional
};

struct ColumnDef
{
    std::string id;    // stable key for persistence
    std::string label; // user-facing
    ValueType type = ValueType::String;

    bool visible = true;
    bool sortable = true;
    bool groupable = true;

    // Access raw value from row (typed when you’re ready).
    // For now you can just parse from row strings.
    std::function<Value(const SimpleRow &)> getValue;

    std::function<std::string(const SimpleRow &)> getGroupKey;
    // Formatting: value -> display string
    std::function<std::string(const Value &)> format;

    // Optional conditional styling
    std::function<CellStyle(const SimpleRow &, const Value &)> style;
};

// ---- Sort/group/filter configuration ----
enum class SortDir
{
    Asc,
    Desc
};

struct SortKey
{
    std::string column_id;
    SortDir dir = SortDir::Asc;

    // Optional: named custom comparator hook (later)
    std::string custom_cmp_id;
};

struct SortConfig
{
    std::string name;
    std::vector<SortKey> keys;
};

enum class AggType
{
    Count,
    Min,
    Max,
    Sum,
    Avg,
    Custom
};

struct AggDef
{
    std::string column_id;
    AggType type = AggType::Count;
    std::string customAggId; // later
};

struct GroupConfig
{
    std::string name;
    std::string groupByColumnId; // empty = none
    std::vector<AggDef> aggs;
};

struct FilterState
{
    std::string quickText; // simplest first: substring match over formatted row
};

struct GridPreferences
{
    // Named defaults (later: persisted)
    std::string defaultSort;
    std::string defaultGroup;

    // UX preferences (later)
    bool zebra = true;
    bool allowMultiSelect = false;
};

// ---- Grid "document" (what the user is looking at) ----
struct GridDocument
{
    IRowSource *source = nullptr;   // not owning
    std::vector<ColumnDef> columns; // dictionary

    std::vector<SortConfig> savedSorts;
    std::vector<GroupConfig> savedGroups;

    FilterState filter;
    GridPreferences prefs;
};

// ---- Computed view model (indices, grouping, caches) ----

struct AggResult
{
    // start simple: count only
    int64_t count = 0;
    int64_t sumValue = 0;
};

struct GroupSpan
{
    std::string groupKeyText;
    int begin = 0; // index into indices[]
    int end = 0;   // exclusive
    AggResult agg;
};

enum class RenderRowKind
{
    GroupHeader,
    DataRow
};

struct RenderRow
{
    RenderRowKind kind;
    int indent = 0;            // 0=year, 1=month...
    int srcRrowIndex = -1;    // valid for DataRow
    int groupNodeIndex = -1; // index into vm.group_nodes (GroupHeader)
};

struct GroupNode
{
    int indent = 0;
    std::string label;                       // what shows in col0 (e.g. "Year=2026", "Month=01")
    int begin = 0, end = 0;                  // range in vm.indices
    std::vector<std::string> summaryByCol; // aligned to doc.columns
};

struct ViewColumn
{
    enum class Kind
    {
        Doc,
        Agg
    } kind = Kind::Doc;

    // For Kind::Doc
    int docColIndex = -1;

    AggDef agg;                         // column_id + agg type
    std::string label;                  // e.g. "Sum(amount)"
    ValueType type = ValueType::Double; // display type (optional)
    bool visible = true;
    bool sortable = false;
};

struct GridViewModel
{
    // Derived
    std::vector<int> indices;      // maps visible row order -> source row index
    std::vector<GroupSpan> groups; // optional

    // State
    std::string activeSortName;
    std::string activeGroupName;

    std::vector<SortKey> activeSortKeys; // ad-hoc sort coming from ImGui header

    std::unordered_map<std::string, gird::AggType> colSummary; // per-column summary
    std::vector<std::string> groupByColumnIds;
    bool showDetailRows = true; // summary-only mode: false

    std::vector<ViewColumn> viewColumns;
    std::unordered_map<std::string, bool> colVisible; // key -> visible
    std::vector<AggDef> active_aggs;

    std::vector<GroupNode> groupNodes;
    std::vector<RenderRow> renderRows;
    bool dirtyRenderRows = true;

    bool showGroupHeaders = true;
    bool showGrandTotal = false;

    // Dirty flags, so dirty!
    bool dirtyIndices = true;
    bool dirtyGroups = true;
    bool dirtyViewColumns = true;

    std::string persistenceKey = "";
};

// ---- Controller: applies configs -> view model ----
struct GridController
{
    GridDocument *doc = nullptr; // not owning
    GridViewModel *vm = nullptr; // not owning

    IPersistence *persistence = nullptr;

    // Selection (start simple)
    int selected_view_row = -1;

    // Helpers
   [[nodiscard]] const ColumnDef *FindCol(const std::string &id) const;
    [[nodiscard]] static int FindColumn(const GridDocument &doc, const std::string &id);
    [[nodiscard]] int ColumnIndexByUserId(int userId) const;
    [[nodiscard]] static std::string GetGroupKey(const GridDocument &doc, int colIdx,
                                            const SimpleRow &row);
    std::vector<std::string> ComputeSummaries(int begin, int end) const;
    // Pipeline steps (we’ll implement next)
    void RebuildIndices() const; // filter + sort -> vm.indices
    void RebuildGroups();  // group -> vm.groups
    void RebuildViewColumns() const;
    void BuildGroupLevel(int level, int begin, int end, int indent);
};

} // namespace gird
