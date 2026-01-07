// BuildFinancialColumns.h - Set up 200 financial columns with proper types

void BuildFinancialColumns(gird::GridDocument &doc)
{
    std::vector<std::string> col_names = {
        // Position book organization (0-9) - GROUPABLE
        "Trader Name", "Book Name", "Account ID", "Region", "Trading Desk",
        "Position ID", "Book Risk ID", "Risk Level", "Position Direction", "Position Status",

        // Trade and position fields (10-18)
        "Trade Date", "Quantity", "Notional Value", "MTM P&L", "MTM Return %",
        "Required Margin %", "Entry Price", "Current Price", "Unrealized P&L %",

        // Instrument identification (19-26)
        "Symbol", "ISIN", "Currency", "Instrument Type", "Option Type",
        "Strike Price", "Expiry Date", "Exchange",

        // Core pricing columns (27-46)
        "Bid Price", "Ask Price", "Volume", "Open Interest", "Volatility %",
        "Maturity Date", "Coupon Rate %", "YTM %", "Credit Spread bps", "Duration Years",
        "Open Volume", "High Volume", "Low Volume", "VWAP", "Market Cap",
        "Dividend Yield %", "P/E Ratio", "Book Value", "Sector", "Opening Price",

        // The Greeks - First generation (47-56)
        "Delta", "Gamma", "Theta", "Vega", "Rho",
        "Lambda", "Vanna", "Charm", "Volga", "Vomma",

        // The Greeks - Second generation (57-66)
        "Vera", "Gamma-weighted Delta", "Theta-Vega Correlation", "Gamma Squared", "Vol-adjusted Delta",
        "Vega Ratio", "Theta Ratio", "Delta Nominal Exposure", "Vega Nominal", "Rho Normalized",

        // Additional Greeks-derived metrics (67-76)
        "Gamma Scaled", "Gamma-adjusted Delta", "Vega-Delta Product", "Theta-Gamma Product", "Leverage-adjusted Delta",
        "Directional Vega Exposure", "Vega Purity", "Theta-Gamma Tradeoff", "Rate-Leverage Interaction", "Greek Sum",

        // Risk metrics (77-86)
        "Value at Risk 95%", "Conditional VaR", "Realized Volatility", "Portfolio Correlation", "Maintenance Margin",
        "Initial Margin", "Return on Notional", "Dollar Delta Exposure", "Vega Dollar Exposure", "Theta Dollar Exposure",
    };

    // Add 113 more generic column names (87-199)
    for (int i = col_names.size(); i < 200; ++i) {
        col_names.push_back("Data_Col_" + std::to_string(i));
    }

    // Clear existing columns
    doc.columns.clear();

    // Add columns with proper types and attributes
    for (int i = 0; i < 200; ++i) {
        gird::ColumnDef col;
        col.id = "col_" + std::to_string(i);
        col.label = col_names[i];
        col.visible = true;
        col.sortable = true;
        col.groupable = (i < 10);  // First 10 columns are groupable


        // Determine type based on column
        if (i == 0 || i == 1 || i == 2 || i == 3 || i == 4) {
            col.type = gird::ValueType::String;  // Trader, Book, Account, Region, Desk
        } else if (i == 8 || i == 9) {
            col.type = gird::ValueType::String;  // Direction, Status
        } else if (i == 10 || i == 25 || i == 35 || i == 45) {
            col.type = gird::ValueType::String;  // Trade Date, Expiry Date, Maturity Date, Sector
        } else if (i == 20 || i == 22 || i == 23) {
            col.type = gird::ValueType::String;  // ISIN, Currency, Instrument Type
        } else if (i == 24) {
            col.type = gird::ValueType::String;  // Option Type
        } else if (i == 5 || i == 6 || i == 7 || i == 11 || i == 12 || i == 29 || i == 30 || i == 31 || i == 32 || i == 45) {
            col.type = gird::ValueType::Int64;  // Integer columns (IDs, quantities, volumes)
        } else {
            col.type = gird::ValueType::Double;  // Default to double for all numerical data
        }

        col.getGroupKey = [i](const gird::SimpleRow &row) -> std::string {
            if (i < 0 || i >= (int)row.size()) {
                return "";
            }
            const auto& cell = row[i];
            if (std::holds_alternative<std::string>(cell)) {
                return std::get<std::string>(cell);
            } else if (std::holds_alternative<int64_t>(cell)) {
                return std::to_string(std::get<int64_t>(cell));
            } else if (std::holds_alternative<double>(cell)) {
                char buf[64];
                snprintf(buf, sizeof(buf), "%.2f", std::get<double>(cell));
                return std::string(buf);
            }
            return "";
        };

        col.getValue = [i](const gird::SimpleRow &row) -> gird::Value {
            if (i < 0 || i >= (int)row.size()) {
                return gird::Value(std::string(""));
            }
            const auto& cell = row[i];

            // Convert variant to string for display
            if (std::holds_alternative<std::string>(cell)) {
                return gird::Value(std::get<std::string>(cell));
            } else if (std::holds_alternative<int64_t>(cell)) {
                return gird::Value(std::to_string(std::get<int64_t>(cell)));
            } else if (std::holds_alternative<double>(cell)) {
                char buf[64];
                snprintf(buf, sizeof(buf), "%.2f", std::get<double>(cell));
                return gird::Value(std::string(buf));
            }  // Return the cell directly
        };

        doc.columns.push_back(col);
    }
}

