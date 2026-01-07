#include "GridViewImGui.h"
#include "GridFramework.h"
#include "GridPersistence.h"

#include <imgui.h>

namespace gird
{


struct PresetUIState
{

    static const int PRESET_NAME_BUFFER_SIZE = 256;

    char savePresetNameBuffer[PRESET_NAME_BUFFER_SIZE] = "";
    std::string currentLoadPresetName = "";  // For combo display on

    std::string loadPresetName = "";
    std::vector<std::string> availablePresets;
    bool presetsNeedRefresh = true;
    bool showSaveSuccess = false;
    bool showLoadSuccess = false;
    int saveSuccessFrames = 0;
    int loadSuccessFrames = 0;
};

static PresetUIState g_presetUI;

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


bool DrawPresetsUI(const GridDocument &doc, GridViewModel &vm, IPersistence *persistence)
{
    if (!persistence || vm.persistenceKey.empty())
    {
        ImGui::TextUnformatted("(Persistence not configured)");
        return false;
    }

    bool changed = false;

    // Refresh preset list periodically
    if (g_presetUI.presetsNeedRefresh)
    {
        g_presetUI.availablePresets = persistence->ListPresets(vm.persistenceKey);
        g_presetUI.presetsNeedRefresh = false;
    }

    // ---- SAVE SECTION ----
    ImGui::Spacing();
    ImGui::TextUnformatted("Save Current State As:");

    ImGui::InputText(
        "##presetSaveName",
        g_presetUI.savePresetNameBuffer,
        256
    );

    ImGui::SameLine();

    if (ImGui::Button("Save Preset##btn") && g_presetUI.savePresetNameBuffer[0] != '\0')
    {
        // Validate preset name (no special chars)
        std::string cleanName = g_presetUI.savePresetNameBuffer;
        cleanName.erase(std::remove_if(cleanName.begin(), cleanName.end(),
            [](char c) { return c == '/' || c == '\\' || c == '"' || c == ':'; }),
            cleanName.end());

        if (!cleanName.empty())
        {
            auto state = ExtractGridState(doc, vm);
            std::string presetKey = vm.persistenceKey + "_" + cleanName;

            if (persistence->Save(presetKey, state))
            {
                g_presetUI.showSaveSuccess = true;
                g_presetUI.saveSuccessFrames = 120;  // Show for 2 seconds at 60 FPS
                g_presetUI.presetsNeedRefresh = true;
                g_presetUI.savePresetNameBuffer[0] = '\0';
                changed = true;
            }
        }
    }

    // Show save success message
    if (g_presetUI.showSaveSuccess)
    {
        ImGui::SameLine();
        ImGui::TextColored(ImVec4(0.2f, 0.8f, 0.2f, 1.0f), "✓ Saved!");

        g_presetUI.saveSuccessFrames--;
        if (g_presetUI.saveSuccessFrames <= 0)
            g_presetUI.showSaveSuccess = false;
    }

    // ---- LOAD SECTION ----
    ImGui::Spacing();
    ImGui::Separator();
    ImGui::Spacing();
    ImGui::TextUnformatted("Load Preset:");

    const char* loadPreview = g_presetUI.loadPresetName.empty()
        ? "Select a preset..."
        : g_presetUI.loadPresetName.c_str();

    if (ImGui::BeginCombo("##presetSelect", loadPreview, ImGuiComboFlags_HeightLarge))
    {
        if (g_presetUI.availablePresets.empty())
        {
            ImGui::TextDisabled("(No saved presets)");
        }
        else
        {
            for (const auto& presetName : g_presetUI.availablePresets)
            {
                bool isSelected = (g_presetUI.loadPresetName == presetName);

                if (ImGui::Selectable(presetName.c_str(), isSelected))
                {
                    g_presetUI.loadPresetName = presetName;
                }

                if (isSelected)
                    ImGui::SetItemDefaultFocus();
            }
        }
        ImGui::EndCombo();
    }

    // Load button
    ImGui::SameLine();
    if (ImGui::Button("Load##btn") && !g_presetUI.loadPresetName.empty())
    {
        std::string presetKey = vm.persistenceKey + "_" + g_presetUI.loadPresetName;
        GridState state;

        if (persistence->Load(presetKey, state))
        {
            ApplyGridState(state, const_cast<GridDocument&>(doc), vm);
            g_presetUI.showLoadSuccess = true;
            g_presetUI.loadSuccessFrames = 120;
            changed = true;
        }
    }

    // Show load success message
    if (g_presetUI.showLoadSuccess)
    {
        ImGui::SameLine();
        ImGui::TextColored(ImVec4(0.2f, 0.8f, 0.2f, 1.0f), "✓ Loaded!");

        g_presetUI.loadSuccessFrames--;
        if (g_presetUI.loadSuccessFrames <= 0)
            g_presetUI.showLoadSuccess = false;
    }

    // Delete button
    ImGui::SameLine();
    if (ImGui::Button("Delete##btn") && !g_presetUI.loadPresetName.empty())
    {
        std::string presetKey = vm.persistenceKey + "_" + g_presetUI.loadPresetName;
        persistence->Clear(presetKey);
        g_presetUI.presetsNeedRefresh = true;
        g_presetUI.loadPresetName = "";
        changed = true;
    }

    // Show saved presets list
    if (!g_presetUI.availablePresets.empty())
    {
        ImGui::Spacing();
        ImGui::Separator();
        ImGui::TextUnformatted("Available Presets:");

        for (const auto& presetName : g_presetUI.availablePresets)
        {
            ImGui::BulletText("%s", presetName.c_str());
        }
    }

    return changed;
}

static bool DrawGroupingConfig(const gird::GridDocument &doc, gird::GridViewModel &vm, gird::IPersistence *persistence)
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
    for (int c = 0; c < static_cast<int>(doc.columns.size()); ++c)
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
        for (int i = 0; i < static_cast<int>(vm.groupByColumnIds.size());)
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
        for (int i = 0; i < static_cast<int>(vm.groupByColumnIds.size()); ++i)
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
                if (i + 1 < static_cast<int>(vm.groupByColumnIds.size()))
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
                for (int gi = 0; gi < static_cast<int>(groupable.size()); ++gi)
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
            for (int gi = 0; gi < static_cast<int>(groupable.size()); ++gi)
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

    for (int i = 0; i < static_cast<int>(vm.active_aggs.size()); ++i)
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
            if (i + 1 < static_cast<int>(vm.active_aggs.size()))
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
            for (int c = 0; c < static_cast<int>(doc.columns.size()); ++c)
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
            for (int t = static_cast<int>(gird::AggType::Count); t <= static_cast<int>(gird::AggType::Custom); ++t)
            {
                auto at = static_cast<gird::AggType>(t);
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

    for (int i = 0; i < static_cast<int>(vm.viewColumns.size()); ++i)
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
            for (int k = 0; k < static_cast<int>(vm.active_aggs.size()); ++k)
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
                    if (agg_idx + 1 < static_cast<int>(vm.active_aggs.size()))
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

    if (ImGui::CollapsingHeader("Presets"))
    {
        if (DrawPresetsUI(doc, vm, persistence))
        {
            vm.dirtyViewColumns = true;
            vm.dirtyIndices = true;
            vm.dirtyGroups = true;
            vm.dirtyRenderRows = true;
        }
    }

    return changed;
}

void DrawGridImGui(GridDocument &doc, GridViewModel &vm, GridController &ctl, ImVec2 size)
{
    ctl.doc = &doc;
    ctl.vm = &vm;

    if (!doc.source)
    {
        return;
    }


    ImGui::SetNextItemOpen(false, ImGuiCond_Once); // default collapsed [web:545]
    if (ImGui::CollapsingHeader("Grid config", ImGuiTreeNodeFlags_DefaultOpen == 0))
    {
        if (DrawGroupingConfig(doc, vm, ctl.persistence))
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
    const int colCount = static_cast<int>(vm.viewColumns.size());
    if (colCount <= 0)
    {
        return;
    }


    // Count visible columns first
    int visibleCount = std::count_if(vm.viewColumns.begin(), vm.viewColumns.end(),
        [](const ViewColumn &vc) { return vc.visible; });

    if (visibleCount == 0)
    {
        ImGui::Text("(No columns visible - toggle column visibility to display data)");
        return;
    }
    // ----- Sorting / indices -----
    if (vm.dirtyIndices)
    {
        ctl.RebuildIndices();
    }

    ImGuiTableFlags flags = ImGuiTableFlags_Resizable | ImGuiTableFlags_Reorderable |
                            ImGuiTableFlags_Hideable | ImGuiTableFlags_Sortable |
                            ImGuiTableFlags_RowBg | ImGuiTableFlags_BordersOuter |
                            ImGuiTableFlags_BordersV | ImGuiTableFlags_ScrollX |
                            ImGuiTableFlags_ScrollY | ImGuiTableFlags_SortMulti | ImGuiTableFlags_SizingFixedFit;;


    if (ImGui::BeginTable("##gird_table", colCount, flags, size))
    {
        // ----- Table columns from vm.view_columns -----
        for (int vc = 0; vc < colCount; ++vc)
        {
            const auto &vcol = vm.viewColumns[vc];

            const char *label = "";
            ImGuiTableColumnFlags cflags = ImGuiTableColumnFlags_WidthFixed;;
            if (vcol.kind == ViewColumn::Kind::Doc)
            {
                const auto &col = doc.columns[vcol.docColIndex];
                label = col.label.c_str();

                if (!col.sortable)
                    cflags |= ImGuiTableColumnFlags_NoSort;
            }
            else
            {
                // virtual aggregate column
                label = vcol.label.c_str();

                cflags |= ImGuiTableColumnFlags_NoSort; // start simple
            }

            ImGui::TableSetColumnEnabled(vc, vcol.visible);
            ImGui::TableSetupColumn(label, cflags, 100.0f, static_cast<ImGuiID>(vc));
        }

        ImGui::TableHeadersRow();

        // ----- Read ImGui sort specs -> vm.active_sort_keys -----
        if (ImGuiTableSortSpecs *sortSpecs = ImGui::TableGetSortSpecs())
        {
            if (sortSpecs->SpecsDirty)
            {
                vm.activeSortKeys.clear();

                for (int i = 0; i < sortSpecs->SpecsCount; ++i)
                {
                    const ImGuiTableColumnSortSpecs &s = sortSpecs->Specs[i];
                    const int viewColIndex = static_cast<int>(s.ColumnUserID);
                    if (viewColIndex < 0 || viewColIndex >= colCount)
                        continue;

                    const auto &vcol = vm.viewColumns[viewColIndex];

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
                sortSpecs->SpecsDirty = false;
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
        clipper.Begin(static_cast<int>(vm.renderRows.size()));

        while (clipper.Step())
        {
            for (int rr = clipper.DisplayStart; rr < clipper.DisplayEnd; ++rr)
            {
                const RenderRow &r = vm.renderRows[rr];
                ImGui::TableNextRow();

                if (r.kind == RenderRowKind::GroupHeader)
                {
                    if (r.groupNodeIndex < 0 || r.groupNodeIndex >= static_cast<int>(vm.groupNodes.size()))
                        continue;

                    const GroupNode &g = vm.groupNodes[r.groupNodeIndex];

                    ImGui::TableSetBgColor(ImGuiTableBgTarget_RowBg0, IM_COL32(45, 45, 70, 255));

                    for (int vc = 0; vc < colCount; ++vc)
                    {
                        ImGui::TableSetColumnIndex(vc);

                        if (vc == 0)
                        {
                            ImGui::Indent(static_cast<float>(g.indent) * 16.0f);
                            ImGui::TextUnformatted(g.label.c_str());
                            ImGui::Unindent(static_cast<float>(g.indent) * 16.0f);
                        }
                        else
                        {
                            if (vc < static_cast<int>(g.summaryByCol.size()) && !g.summaryByCol[vc].empty())
                                ImGui::TextUnformatted(g.summaryByCol[vc].c_str());
                        }
                    }
                }
                else // DataRow
                {
                    const int src_row_idx = r.srcRrowIndex;
                    const auto &row = doc.source->RowAt(src_row_idx);

                    for (int vc = 0; vc < colCount; ++vc)
                    {
                        ImGui::TableSetColumnIndex(vc);

                        if (vc == 0)
                            ImGui::Indent(static_cast<float>(r.indent) * 16.0f);

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
                            ImGui::Unindent(static_cast<float>(r.indent) * 16.0f);
                    }
                }
            }
        }

        ImGui::EndTable();
    }
}



} // namespace gird
