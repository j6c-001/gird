#pragma once
#include <string>
#include <vector>
#include <functional>
#include <numeric>   // iota
#include <algorithm> // stable_sort

struct GridColumn
{
    std::string id;
    std::string label;
    bool visible = true;

    // Extract a display string from a row (row = vector<string> for now).
    // Later this becomes variant/typed + formatting.
    std::function<std::string(const std::vector<std::string>&)> get;
};

struct GridModel
{
    std::vector<GridColumn> cols;
    std::vector<std::vector<std::string>> rows;

    // View indirection (sorted / filtered later)
    std::vector<int> indices;

    // simple sort state (one column for now)
    int sortCol = -1;
    bool sortAsc = true;

    void rebuild_indices()
    {
        indices.resize(static_cast<int>(rows.size()));
        std::iota(indices.begin(), indices.end(), 0);
    }

    void sort_by(int col, bool asc)
    {
        sortCol = col;
        sortAsc = asc;

        if (sortCol < 0 || sortCol >= static_cast<int>(cols.size()))
            return;

        auto& c = cols[sortCol];
        std::stable_sort(indices.begin(), indices.end(),
            [&](int ia, int ib)
            {
                const auto sa = c.get(rows[ia]);
                const auto sb = c.get(rows[ib]);
                if (asc) return sa < sb;
                return sa > sb;
            });
    }
};
