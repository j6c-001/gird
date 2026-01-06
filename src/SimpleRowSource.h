#pragma once
#include "GridFramework.h"

namespace gird {

        struct SimpleRowSource final : public IRowSource
        {
            std::vector<SimpleRow> rows;

            int RowCount() const override { return (int)rows.size(); }
            const SimpleRow& RowAt(int row_index) const override { return rows[row_index]; }
        };

} // namespace gird
