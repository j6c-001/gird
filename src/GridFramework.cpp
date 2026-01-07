#include "GridFramework.h"
#include <algorithm>
#include <numeric>

namespace gird
{

const ColumnDef *GridController::FindCol(const std::string &id) const
{
    if (!doc)
        return nullptr;
    for (const auto &c : doc->columns)
        if (c.id == id)
            return &c;
    return nullptr;
}

int GridController::ColumnIndexByUserId(int user_id) const
{
    // In DrawGridImGui we set ColumnUserID = column index.
    return user_id;
}

static int cmp_values_typed(ValueType t, const Value &a, const Value &b)
{
    switch (t)
    {
    case ValueType::Int64:
    {
        const auto *pa = std::get_if<int64_t>(&a);
        const auto *pb = std::get_if<int64_t>(&b);
        const int64_t va = pa ? *pa : 0;
        const int64_t vb = pb ? *pb : 0;
        if (va < vb)
            return -1;
        if (va > vb)
            return 1;
        return 0;
    }
    case ValueType::Double:
    {
        const auto *pa = std::get_if<double>(&a);
        const auto *pb = std::get_if<double>(&b);
        const double va = pa ? *pa : 0.0;
        const double vb = pb ? *pb : 0.0;
        if (va < vb)
            return -1;
        if (va > vb)
            return 1;
        return 0;
    }
    case ValueType::Bool:
    {
        const auto *pa = std::get_if<bool>(&a);
        const auto *pb = std::get_if<bool>(&b);
        const bool va = pa ? *pa : false;
        const bool vb = pb ? *pb : false;
        if (va == vb)
            return 0;
        return va ? 1 : -1;
    }
    case ValueType::String:
    default:
    {
        const auto *pa = std::get_if<std::string>(&a);
        const auto *pb = std::get_if<std::string>(&b);
        const std::string va = pa ? *pa : std::string{};
        const std::string vb = pb ? *pb : std::string{};
        if (va < vb)
            return -1;
        if (va > vb)
            return 1;
        return 0;
    }
    }
}
void GridController::RebuildIndices() const
{
    if (!doc || !vm || !doc->source)
        return;

    const int n = doc->source->RowCount();
    vm->indices.resize(n);
    std::iota(vm->indices.begin(), vm->indices.end(), 0);

    std::vector<SortKey> effective = vm->activeSortKeys;

    // Prepend all group-by columns as leading sort keys (in order)
    for (auto it = vm->groupByColumnIds.rbegin(); it != vm->groupByColumnIds.rend(); ++it)
    {
        bool already = false;
        for (auto &k : effective)
            if (k.column_id == *it)
            {
                already = true;
                break;
            }
        if (!already)
            effective.insert(effective.begin(), SortKey{*it, SortDir::Asc, {}});
    }

    // (Filtering will go here later)

    if (!effective.empty())
    {
        std::ranges::stable_sort(vm->indices,
                                 [&](int ra, int rb)
                                 {
                                     const auto &rowA = doc->source->RowAt(ra);
                                     const auto &rowB = doc->source->RowAt(rb);

                                     for (const auto &key : effective)
                                     {
                                         const ColumnDef *col = FindCol(key.column_id);
                                         if (!col || !col->getValue)
                                             continue;

                                         Value va = col->getValue(rowA);
                                         Value vb = col->getValue(rowB);

                                         int c = cmp_values_typed(col->type, va, vb);
                                         if (c == 0)
                                             continue;

                                         const bool asc = (key.dir == SortDir::Asc);
                                         return asc ? (c < 0) : (c > 0);
                                     }
                                     return ra < rb;
                                 });

        vm->dirtyIndices = false;
        vm->dirtyGroups = true;
    }
}
void GridController::RebuildGroups()
{
    vm->groupNodes.clear();
    vm->renderRows.clear();

    // Optional grand total header at very top
    if (vm->showGrandTotal)
    {
        GroupNode total;
        total.indent = 0;
        total.label = "Grand total";
        total.begin = 0;
        total.end = static_cast<int>(vm->indices.size());
        total.summaryByCol = ComputeSummaries(total.begin, total.end); // must align to view_columns

        int idx = static_cast<int>(vm->groupNodes.size());
        vm->groupNodes.push_back(std::move(total));
        vm->renderRows.push_back({RenderRowKind::GroupHeader, 0, -1, idx});
    }

    if (vm->groupByColumnIds.empty())
    {
        vm->renderRows.reserve(vm->renderRows.size() + vm->indices.size());
        for (int src : vm->indices)
            vm->renderRows.push_back({RenderRowKind::DataRow, 0, src, -1});
        vm->dirtyGroups = false;
        vm->dirtyRenderRows = false;
        return;
    }

    BuildGroupLevel(0, 0, static_cast<int>(vm->indices.size()), 0);

    vm->dirtyGroups = false;
    vm->dirtyRenderRows = false;
}

static const char *AggTypeName(gird::AggType t)
{
    switch (t)
    {
    case gird::AggType::Count:
        return "Count";
    case gird::AggType::Min:
        return "Min";
    case gird::AggType::Max:
        return "Max";
    case gird::AggType::Sum:
        return "Sum";
    case gird::AggType::Avg:
        return "Avg";
    case gird::AggType::Custom:
        return "Custom";
    default:
        return "?";
    }
}
static std::string DocKey(const std::string &col_id) { return "doc:" + col_id; }

static std::string AggKey(const AggDef &a)
{
    return "agg:" + std::string(AggTypeName(a.type)) + ":" + a.column_id;
}
void GridController::RebuildViewColumns() const
{
    vm->viewColumns.clear();

    auto get_vis = [&](const std::string &key, bool def) -> bool
    {
        auto it = vm->colVisible.find(key);
        return (it == vm->colVisible.end()) ? def : it->second;
    };

    // Doc columns
    for (int c = 0; c < static_cast<int>(doc->columns.size()); ++c)
    {
        const auto &col = doc->columns[c];

        ViewColumn vc;
        vc.kind = ViewColumn::Kind::Doc;
        vc.docColIndex = c;
        vc.sortable = col.sortable;
        vc.visible = get_vis(DocKey(col.id), /*default*/ col.visible);

        vm->viewColumns.push_back(std::move(vc));
    }

    // Agg columns
    for (const auto &a : vm->active_aggs)
    {
        ViewColumn vc;
        vc.kind = ViewColumn::Kind::Agg;
        vc.agg = a;

        const ColumnDef *base = FindCol(a.column_id);
        const std::string baseLabel = base ? base->label : a.column_id;
        vc.label = std::string(AggTypeName(a.type)) + "(" + baseLabel + ")";

        vc.visible = get_vis(AggKey(a), /*default*/ true);
        vc.sortable = false;

        vm->viewColumns.push_back(std::move(vc));
    }

    vm->dirtyViewColumns = false;
}

// Recursive helper: processes one grouping level
void GridController::BuildGroupLevel(int level, int begin, int end, int indent)
{
    if (level >= static_cast<int>(vm->groupByColumnIds.size()))
    {
        // Leaf: emit detail rows (if enabled)
        if (vm->showDetailRows)
        {
            for (int i = begin; i < end; ++i)
            {
                const int src = vm->indices[i];
                vm->renderRows.push_back({RenderRowKind::DataRow, indent, src, -1});
            }
        }
        return;
    }

    // Find a column for this level
    const std::string &col_id = vm->groupByColumnIds[level];
    const int col_idx = FindColumn(*doc, col_id);
    if (col_idx < 0)
    {
        // Column not found: treat as ungrouped at this level
        BuildGroupLevel(level + 1, begin, end, indent);
        return;
    }

    // Split into runs by this column's value
    int i = begin;
    while (i < end)
    {
        const int src_first = vm->indices[i];
        const auto key = GetGroupKey(*doc, col_idx, doc->source->RowAt(src_first));

        // Find run with same key
        int run_end = i + 1;
        while (run_end < end)
        {
            const int src = vm->indices[run_end];
            const auto k = GetGroupKey(*doc, col_idx, doc->source->RowAt(src));
            if (k != key)
                break;
            ++run_end;
        }

        // Emit group header for this run
        {
            GroupNode node;
            node.indent = indent;
            node.label = doc->columns[col_idx].label + "=" + key;
            node.begin = i;
            node.end = run_end;
            node.summaryByCol = ComputeSummaries(i, run_end);

            const int node_idx = static_cast<int>(vm->groupNodes.size());
            vm->groupNodes.push_back(std::move(node));
            vm->renderRows.push_back({RenderRowKind::GroupHeader, indent, -1, node_idx});
        }

        // Recurse into next level
        BuildGroupLevel(level + 1, i, run_end, indent + 1);

        i = run_end;
    }
}

// Find column index by ID
int GridController::FindColumn(const GridDocument &doc, const std::string &id)
{
    for (int c = 0; c < static_cast<int>(doc.columns.size()); ++c)
        if (doc.columns[c].id == id)
            return c;
    return -1;
}

// Extract group key as string (for comparison + display)
std::string GridController::GetGroupKey(const GridDocument &doc, int col_idx,
                                        const SimpleRow &row)
{
    const auto &col = doc.columns[col_idx];
    const Value v = col.getValue(row);
    return col.getGroupKey(row); // reuse your existing format function
}

// Compute summaries for range [begin, end) in vm.indices
std::vector<std::string> GridController::ComputeSummaries(int begin, int end) const
{
    std::vector<std::string> out;
    if (!doc || !vm)
        return out;

    out.resize(vm->viewColumns.size());

    const int count = end - begin;

    // Always show count in col0 (or leave this out if you prefer)
    if (!out.empty())
        out[0] = "Count: " + std::to_string(count);

    auto format_i64 = [](int64_t v) { return std::to_string(v); };
    auto format_f64 = [](double v) { return std::to_string(v); };

    auto agg_on_doc_col = [&](int doc_col_index, AggType type) -> std::string
    {
        if (doc_col_index < 0 || doc_col_index >= static_cast<int>(doc->columns.size()))
            return {};
        const ColumnDef &col = doc->columns[doc_col_index];
        if (!col.getValue)
            return {};

        // Count works for any type
        if (type == AggType::Count)
            return std::to_string(count);

        // Numeric aggregations: Int64 / Double
        if (col.type == ValueType::Int64)
        {
            bool have = false;
            int64_t mn = 0, mx = 0;
            int64_t sum = 0;

            for (int i = begin; i < end; ++i)
            {
                const int src = vm->indices[i];
                const auto &row = doc->source->RowAt(src);
                Value v = col.getValue(row);
                auto p = std::get_if<int64_t>(&v);
                if (!p)
                    continue;

                const int64_t x = *p;
                if (!have)
                {
                    mn = mx = x;
                    have = true;
                }
                mn = std::min(mn, x);
                mx = std::max(mx, x);
                sum += x;
            }

            if (!have)
                return {};

            switch (type)
            {
            case AggType::Min:
                return format_i64(mn);
            case AggType::Max:
                return format_i64(mx);
            case AggType::Sum:
                return format_i64(sum);
            case AggType::Avg:
                return format_f64(static_cast<double>(sum) / static_cast<double>(count));
            default:
                return {};
            }
        }
        else if (col.type == ValueType::Double)
        {
            bool have = false;
            double mn = 0, mx = 0;
            double sum = 0.0;

            for (int i = begin; i < end; ++i)
            {
                const int src = vm->indices[i];
                const auto &row = doc->source->RowAt(src);
                Value v = col.getValue(row);
                auto p = std::get_if<double>(&v);
                if (!p)
                    continue;

                const double x = *p;
                if (!have)
                {
                    mn = mx = x;
                    have = true;
                }
                mn = std::min(mn, x);
                mx = std::max(mx, x);
                sum += x;
            }

            if (!have)
                return {};

            switch (type)
            {
            case AggType::Min:
                return format_f64(mn);
            case AggType::Max:
                return format_f64(mx);
            case AggType::Sum:
                return format_f64(sum);
            case AggType::Avg:
                return format_f64(sum / static_cast<double>(count));
            default:
                return {};
            }
        }

        // Non-numeric: only Min/Max by formatted string could be added later
        return {};
    };

    // Fill aggregate view columns
    for (int vc = 0; vc < static_cast<int>(vm->viewColumns.size()); ++vc)
    {
        const auto &vcol = vm->viewColumns[vc];
        if (vcol.kind != ViewColumn::Kind::Agg)
            continue;

        // Find source doc column by id
        int doc_col_index = -1;
        for (int c = 0; c < static_cast<int>(doc->columns.size()); ++c)
        {
            if (doc->columns[c].id == vcol.agg.column_id)
            {
                doc_col_index = c;
                break;
            }
        }

        out[vc] = agg_on_doc_col(doc_col_index, vcol.agg.type);
    }

    return out;
}

} // namespace gird
