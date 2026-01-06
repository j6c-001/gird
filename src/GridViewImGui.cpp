#include "GridViewImGui.h"
#include "GridFramework.h"

#include <imgui.h>

namespace gird
{

static std::string default_format(const Value &v)
{
    if (auto p = std::get_if<std::string>(&v))
        return *p;
    if (auto p = std::get_if<int64_t>(&v))
        return std::to_string(*p);
    if (auto p = std::get_if<double>(&v))
        return std::to_string(*p);
    if (auto p = std::get_if<bool>(&v))
        return *p ? "true" : "false";
    return {};
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

static std::string AggKey(const gird::AggDef &a)
{
    return "agg:" + std::string(AggTypeName(a.type)) + ":" + a.column_id;
}

static bool DrawGroupingConfig(const gird::GridDocument &doc, gird::GridViewModel &vm)
{
    bool changed = false;

    // Summary-only toggle
    bool details = vm.showDetailRows;
    if (ImGui::Checkbox("Details", &details))
    {
        vm.showDetailRows = details;
        changed = true;
    }
    ImGui::SameLine();

    if (ImGui::Checkbox("Grand total row", &vm.showGrandTotal))
    {
        vm.dirtyGroups = true;
        vm.dirtyRenderRows = true;
    }

    ImGui::SameLine();
    if (ImGui::Button("Grouping..."))
        ImGui::OpenPopup("##GroupingPopup");

    // Collect groupable column indices once
    std::vector<int> groupable;
    groupable.reserve(doc.columns.size());
    for (int c = 0; c < (int)doc.columns.size(); ++c)
        if (doc.columns[c].groupable)
            groupable.push_back(c);

    auto label_for_id = [&](const std::string &id) -> const char *
    {
        if (id.empty())
            return "(none)";
        for (const auto &c : doc.columns)
            if (c.id == id)
                return c.label.c_str();
        return "(missing)";
    };

    auto id_is_valid = [&](const std::string &id) -> bool
    {
        for (const auto &c : doc.columns)
            if (c.id == id)
                return true;
        return false;
    };

    if (ImGui::BeginPopup("##GroupingPopup"))
    {
        ImGui::TextUnformatted("Group by (top to bottom):");
        ImGui::Separator();

        // Remove invalid ids (e.g. column deleted)
        for (int i = 0; i < (int)vm.groupByColumnIds.size();)
        {
            if (!id_is_valid(vm.groupByColumnIds[i]))
            {
                vm.groupByColumnIds.erase(vm.groupByColumnIds.begin() + i);
                changed = true;
            }
            else
            {
                ++i;
            }
        }

        // Existing levels list
        for (int i = 0; i < (int)vm.groupByColumnIds.size(); ++i)
        {
            ImGui::PushID(i);

            // Reorder buttons
            if (ImGui::ArrowButton("##up", ImGuiDir_Up))
            {
                if (i > 0)
                {
                    std::swap(vm.groupByColumnIds[i], vm.groupByColumnIds[i - 1]);
                    changed = true;
                }
            }
            ImGui::SameLine();
            if (ImGui::ArrowButton("##down", ImGuiDir_Down))
            {
                if (i + 1 < (int)vm.groupByColumnIds.size())
                {
                    std::swap(vm.groupByColumnIds[i], vm.groupByColumnIds[i + 1]);
                    changed = true;
                }
            }

            ImGui::SameLine();

            // Column picker for this level
            const char *preview = label_for_id(vm.groupByColumnIds[i]);
            if (ImGui::BeginCombo("##col", preview))
            {
                for (int gi = 0; gi < (int)groupable.size(); ++gi)
                {
                    const int c = groupable[gi];
                    const bool selected = (doc.columns[c].id == vm.groupByColumnIds[i]);
                    if (ImGui::Selectable(doc.columns[c].label.c_str(), selected))
                    {
                        vm.groupByColumnIds[i] = doc.columns[c].id;
                        changed = true;
                    }
                }
                ImGui::EndCombo();
            }

            ImGui::SameLine();
            if (ImGui::Button("Remove"))
            {
                vm.groupByColumnIds.erase(vm.groupByColumnIds.begin() + i);
                changed = true;
                ImGui::PopID();
                break; // vector changed; restart next frame
            }

            ImGui::PopID();
        }

        ImGui::Separator();

        // Add new level (cannot add duplicates)
        auto id_used = [&](const std::string &id) -> bool
        {
            for (auto &x : vm.groupByColumnIds)
                if (x == id)
                    return true;
            return false;
        };

        if (ImGui::BeginCombo("Add level", "Select column..."))
        {
            for (int gi = 0; gi < (int)groupable.size(); ++gi)
            {
                const int c = groupable[gi];
                const std::string &id = doc.columns[c].id;
                if (id_used(id))
                    continue;

                if (ImGui::Selectable(doc.columns[c].label.c_str(), false))
                {
                    vm.groupByColumnIds.push_back(id);
                    changed = true;
                }
            }
            ImGui::EndCombo();
        }

        ImGui::SameLine();
        if (ImGui::Button("Clear"))
        {
            vm.groupByColumnIds.clear();
            changed = true;
        }

        ImGui::EndPopup();
    }

    ImGui::TextUnformatted("Aggregate columns:");
    ImGui::Separator();

    for (int i = 0; i < (int)vm.active_aggs.size(); ++i)
    {
        ImGui::PushID(i);
        auto &a = vm.active_aggs[i];

        // Reorder
        if (ImGui::ArrowButton("##upAgg", ImGuiDir_Up))
        {
            if (i > 0)
            {
                std::swap(vm.active_aggs[i], vm.active_aggs[i - 1]);
                changed = true;
            }
        }
        ImGui::SameLine();
        if (ImGui::ArrowButton("##dnAgg", ImGuiDir_Down))
        {
            if (i + 1 < (int)vm.active_aggs.size())
            {
                std::swap(vm.active_aggs[i], vm.active_aggs[i + 1]);
                changed = true;
            }
        }

        ImGui::SameLine();

        // Pick source column (any non-group column allowed too—your choice)
        const char *preview_col = "(none)";
        for (auto &c : doc.columns)
            if (c.id == a.column_id)
            {
                preview_col = c.label.c_str();
                break;
            }

        if (ImGui::BeginCombo("##aggCol", preview_col))
        {
            for (int c = 0; c < (int)doc.columns.size(); ++c)
            {
                const auto &col = doc.columns[c];
                // optional: skip group-by columns if you want
                const bool selected = (col.id == a.column_id);
                if (ImGui::Selectable(col.label.c_str(), selected))
                {
                    a.column_id = col.id;
                    changed = true;
                }
            }
            ImGui::EndCombo();
        }

        ImGui::SameLine();

        // Pick aggregation function
        const char *preview_agg = AggTypeName(a.type);
        if (ImGui::BeginCombo("##aggFn", preview_agg))
        {
            for (int t = (int)gird::AggType::Count; t <= (int)gird::AggType::Custom; ++t)
            {
                auto at = (gird::AggType)t;
                bool sel = (a.type == at);
                if (ImGui::Selectable(AggTypeName(at), sel))
                {
                    a.type = at;
                    changed = true;
                }
            }
            ImGui::EndCombo();
        }

        ImGui::SameLine();
        if (ImGui::Button("Delete"))
        {
            vm.active_aggs.erase(vm.active_aggs.begin() + i);
            changed = true;
            ImGui::PopID();
            break;
        }

        ImGui::PopID();
    }

    if (ImGui::Button("Add aggregate"))
    {
        vm.active_aggs.push_back(gird::AggDef{.column_id = "", .type = gird::AggType::Count});
        changed = true;
    }

    ImGui::Separator();
    ImGui::TextUnformatted("Columns:");
    ImGui::TextUnformatted("Toggle visibility; agg cols can be reordered/deleted.");
    ImGui::Separator();

    for (int i = 0; i < (int)vm.viewColumns.size(); ++i)
    {
        ImGui::PushID(i);
        const auto &vc = vm.viewColumns[i];

        // Determine label + stable key
        std::string key;
        const char *label = "";

        if (vc.kind == ViewColumn::Kind::Doc)
        {
            const auto &base = doc.columns[vc.docColIndex];
            label = base.label.c_str();
            key = DocKey(base.id);
        }
        else
        {
            label = vc.label.c_str();
            key = AggKey(vc.agg);
        }

        // Visibility toggle (persist to vm.col_visible)
        bool vis = vc.visible;
        if (ImGui::Checkbox(label, &vis))
        {
            vm.colVisible[key] = vis;
            changed = true;
        }

        // Agg-only: reorder/delete must operate on vm.active_aggs (the source), not vm.view_columns
        if (vc.kind == ViewColumn::Kind::Agg)
        {
            // find agg index in vm.active_aggs (match by column_id + type)
            int agg_idx = -1;
            for (int k = 0; k < (int)vm.active_aggs.size(); ++k)
            {
                if (vm.active_aggs[k].column_id == vc.agg.column_id &&
                    vm.active_aggs[k].type == vc.agg.type)
                {
                    agg_idx = k;
                    break;
                }
            }

            if (agg_idx >= 0)
            {
                ImGui::SameLine();
                if (ImGui::ArrowButton("##upAggCol", ImGuiDir_Up))
                {
                    if (agg_idx > 0)
                    {
                        std::swap(vm.active_aggs[agg_idx], vm.active_aggs[agg_idx - 1]);
                        changed = true;
                    }
                }
                ImGui::SameLine();
                if (ImGui::ArrowButton("##dnAggCol", ImGuiDir_Down))
                {
                    if (agg_idx + 1 < (int)vm.active_aggs.size())
                    {
                        std::swap(vm.active_aggs[agg_idx], vm.active_aggs[agg_idx + 1]);
                        changed = true;
                    }
                }
                ImGui::SameLine();
                if (ImGui::Button("Delete"))
                {
                    // also drop visibility state for this key (optional cleanup)
                    vm.colVisible.erase(key);
                    vm.active_aggs.erase(vm.active_aggs.begin() + agg_idx);
                    changed = true;
                }
            }
        }

        ImGui::PopID();
    }

    return changed;
}

void DrawGridImGui(GridDocument &doc, GridViewModel &vm, GridController &ctl, ImVec2 size)
{
    ctl.doc = &doc;
    ctl.vm = &vm;

    if (!doc.source)
        return;

    ImGui::SetNextItemOpen(false, ImGuiCond_Once); // default collapsed [web:545]
    if (ImGui::CollapsingHeader("Grid config", ImGuiTreeNodeFlags_DefaultOpen == 0))
    {
        if (DrawGroupingConfig(doc, vm))
        {
            vm.dirtyViewColumns = true;
            vm.dirtyIndices = true;
            vm.dirtyGroups = true;
            vm.dirtyRenderRows = true;
        }
        ImGui::Separator();
    }

    ImGui::Separator();

    // ----- Build view column projection -----
    if (vm.dirtyViewColumns)
    {
        ctl.RebuildViewColumns(); // fills vm.view_columns
        vm.dirtyGroups = true;     // summaries must realign to view cols
        vm.dirtyRenderRows = true;
    }

    // ----- Sorting / indices -----
    if (vm.dirtyIndices)
        ctl.RebuildIndices();

    ImGuiTableFlags flags = ImGuiTableFlags_Resizable | ImGuiTableFlags_Reorderable |
                            ImGuiTableFlags_Hideable | ImGuiTableFlags_Sortable |
                            ImGuiTableFlags_RowBg | ImGuiTableFlags_BordersOuter |
                            ImGuiTableFlags_BordersV | ImGuiTableFlags_ScrollX |
                            ImGuiTableFlags_ScrollY | ImGuiTableFlags_SortMulti;

    const int col_count = (int)vm.viewColumns.size();
    if (col_count <= 0)
        return;

    if (ImGui::BeginTable("##gird_table", col_count, flags, size))
    {
        // ----- Table columns from vm.view_columns -----
        for (int vc = 0; vc < col_count; ++vc)
        {
            const auto &vcol = vm.viewColumns[vc];

            const char *label = "";
            ImGuiTableColumnFlags cflags = ImGuiTableColumnFlags_None;

            if (vcol.kind == ViewColumn::Kind::Doc)
            {
                const auto &col = doc.columns[vcol.docColIndex];
                label = col.label.c_str();
                if (!col.visible)
                    cflags |= ImGuiTableColumnFlags_DefaultHide;
                if (!col.sortable)
                    cflags |= ImGuiTableColumnFlags_NoSort;
            }
            else
            {
                // virtual aggregate column
                label = vcol.label.c_str();
                cflags |= ImGuiTableColumnFlags_NoSort; // start simple
            }

            // ColumnUserID == view column index (NOT doc index)
            ImGui::TableSetupColumn(label, cflags, 0.0f, (ImGuiID)vc);
        }

        ImGui::TableHeadersRow();

        // ----- Read ImGui sort specs -> vm.active_sort_keys -----
        if (ImGuiTableSortSpecs *sort_specs = ImGui::TableGetSortSpecs())
        {
            if (sort_specs->SpecsDirty)
            {
                vm.activeSortKeys.clear();

                for (int i = 0; i < sort_specs->SpecsCount; ++i)
                {
                    const ImGuiTableColumnSortSpecs &s = sort_specs->Specs[i];
                    const int view_col_index = (int)s.ColumnUserID;
                    if (view_col_index < 0 || view_col_index >= col_count)
                        continue;

                    const auto &vcol = vm.viewColumns[view_col_index];

                    // Only doc columns participate in sorting (for now)
                    if (vcol.kind != ViewColumn::Kind::Doc)
                        continue;

                    const auto &col = doc.columns[vcol.docColIndex];
                    if (!col.sortable)
                        continue;

                    SortKey key;
                    key.column_id = col.id;
                    key.dir = (s.SortDirection == ImGuiSortDirection_Ascending) ? SortDir::Asc
                                                                                : SortDir::Desc;
                    vm.activeSortKeys.push_back(std::move(key));
                }

                vm.dirtyIndices = true;
                sort_specs->SpecsDirty = false;
            }
        }

        // ----- Apply sort + grouping pipeline -----
        if (vm.dirtyIndices)
        {
            ctl.RebuildIndices();
            vm.dirtyGroups = true;
            vm.dirtyRenderRows = true;
        }

        if (vm.dirtyGroups || vm.dirtyRenderRows)
        {
            ctl.RebuildGroups(); // must populate vm.render_rows using vm.indices
            // ideally rebuild_groups() clears vm.dirty_groups + vm.dirty_render_rows
        }

        // ----- Draw rows from vm.render_rows -----
        ImGuiListClipper clipper;
        clipper.Begin((int)vm.renderRows.size());

        while (clipper.Step())
        {
            for (int rr = clipper.DisplayStart; rr < clipper.DisplayEnd; ++rr)
            {
                const RenderRow &r = vm.renderRows[rr];
                ImGui::TableNextRow();

                if (r.kind == RenderRowKind::GroupHeader)
                {
                    if (r.groupNodeIndex < 0 || r.groupNodeIndex >= (int)vm.groupNodes.size())
                        continue;

                    const GroupNode &g = vm.groupNodes[r.groupNodeIndex];

                    ImGui::TableSetBgColor(ImGuiTableBgTarget_RowBg0, IM_COL32(45, 45, 70, 255));

                    for (int vc = 0; vc < col_count; ++vc)
                    {
                        ImGui::TableSetColumnIndex(vc);

                        if (vc == 0)
                        {
                            ImGui::Indent((float)g.indent * 16.0f);
                            ImGui::TextUnformatted(g.label.c_str());
                            ImGui::Unindent((float)g.indent * 16.0f);
                        }
                        else
                        {
                            if (vc < (int)g.summaryByCol.size() && !g.summaryByCol[vc].empty())
                                ImGui::TextUnformatted(g.summaryByCol[vc].c_str());
                        }
                    }
                }
                else // DataRow
                {
                    const int src_row_idx = r.srcRrowIndex;
                    const auto &row = doc.source->RowAt(src_row_idx);

                    for (int vc = 0; vc < col_count; ++vc)
                    {
                        ImGui::TableSetColumnIndex(vc);

                        if (vc == 0)
                            ImGui::Indent((float)r.indent * 16.0f);

                        const auto &vcol = vm.viewColumns[vc];

                        if (vcol.kind == ViewColumn::Kind::Doc)
                        {
                            const auto &col = doc.columns[vcol.docColIndex];
                            Value v = col.getValue ? col.getValue(row) : Value(std::string{});
                            const auto text = col.format ? col.format(v) : default_format(v);
                            ImGui::TextUnformatted(text.c_str());
                        }
                        else
                        {
                            // Aggregate columns are summary-only for now
                            ImGui::TextUnformatted("");
                        }

                        if (vc == 0)
                            ImGui::Unindent((float)r.indent * 16.0f);
                    }
                }
            }
        }

        ImGui::EndTable();
    }
}

} // namespace gird
