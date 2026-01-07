#pragma once

#include "GridFramework.h"
#include <random>
#include <ctime>
#include <iomanip>
#include <sstream>

namespace gird {

// Financial data generator with options, Greeks, and position book structure
class FinancialDataGenerator {
public:
    static constexpr int NUM_ROWS = 10000;
    static constexpr int NUM_COLUMNS = 200;

    static std::vector<SimpleRow> GenerateRows()
    {
        std::vector<SimpleRow> rows;
        rows.reserve(NUM_ROWS);  // Pre-allocate vector

        std::mt19937 gen(static_cast<unsigned>(std::time(nullptr)));

        // Distribution generators
        std::uniform_int_distribution<int> year_dist(2020, 2026);
        std::uniform_int_distribution<int> month_dist(1, 12);
        std::uniform_int_distribution<int> day_dist(1, 28);
        std::uniform_real_distribution<double> price_dist(10.0, 10000.0);
        std::uniform_real_distribution<double> spread_dist(0.01, 5.0);
        std::uniform_int_distribution<int> volume_dist(1000, 10000000);
        std::uniform_real_distribution<double> volatility_dist(0.05, 1.0);
        std::uniform_real_distribution<double> coupon_dist(0.5, 8.0);
        std::uniform_real_distribution<double> ytm_dist(1.0, 12.0);
        std::uniform_real_distribution<double> spread_basis_dist(10.0, 500.0);
        std::uniform_real_distribution<double> duration_dist(0.5, 30.0);
        std::uniform_real_distribution<double> marketcap_dist(1e6, 1e12);
        std::uniform_int_distribution<int> pe_dist(5, 50);
        std::uniform_real_distribution<double> bv_dist(1.0, 1000.0);
        std::uniform_int_distribution<int> dividend_dist(0, 12);
        std::uniform_real_distribution<double> notional_dist(1e5, 1e8);
        std::uniform_real_distribution<double> mtm_dist(-5e6, 5e6);
        std::uniform_real_distribution<double> margin_dist(0.0, 50.0);

        // Greeks distributions
        std::uniform_real_distribution<double> delta_dist(-1.0, 1.0);
        std::uniform_real_distribution<double> gamma_dist(0.0, 0.1);
        std::uniform_real_distribution<double> theta_dist(-1.0, 0.0);
        std::uniform_real_distribution<double> vega_dist(0.0, 50.0);
        std::uniform_real_distribution<double> rho_dist(-100.0, 100.0);
        std::uniform_real_distribution<double> lambda_dist(0.0, 10.0);

        std::vector<std::string> symbols = {
            "AAPL", "MSFT", "GOOGL", "AMZN", "NVDA", "TSLA", "META", "JPM", "BAC", "WFC"
        };

        std::vector<std::string> types = {
            "STOCK", "BOND", "OPTION", "FUTURE", "ETF"
        };

        std::vector<std::string> sectors = {
            "Technology", "Healthcare", "Financials", "Energy", "Consumer"
        };

        std::vector<std::string> exchanges = {
            "NYSE", "NASDAQ", "LSE", "EURONEXT"
        };

        std::vector<std::string> currencies = {
            "USD", "EUR", "GBP", "JPY"
        };

        std::vector<std::string> traders = {
            "John Smith", "Sarah Chen", "Michael Johnson", "Emma Williams", "David Brown"
        };

        std::vector<std::string> books = {
            "Cash_Equities", "Derivatives", "Fixed_Income", "FX_Spot"
        };

        std::vector<std::string> accounts = {
            "ACC001", "ACC002", "ACC003", "ACC004"
        };

        std::vector<std::string> regions = {
            "EMEA", "APAC", "AMERICAS"
        };

        std::vector<std::string> desks = {
            "Long_Equities", "Short_Equities", "Flow_Trading", "Algo_Trading"
        };

        for (int row = 0; row < NUM_ROWS; ++row) {
            SimpleRow r(NUM_COLUMNS);  // Pre-allocate exact size
            int col = 0;

            // Generate dates once per row
            int settle_year = year_dist(gen);
            int settle_month = month_dist(gen);
            int settle_day = day_dist(gen);

            std::ostringstream settle_oss;
            settle_oss << std::setfill('0') << std::setw(4) << settle_year << "-"
                      << std::setw(2) << settle_month << "-" << std::setw(2) << settle_day;
            std::string settle_date = settle_oss.str();

            int mature_year = settle_year + (row % 5);
            std::ostringstream mature_oss;
            mature_oss << std::setfill('0') << std::setw(4) << mature_year << "-"
                      << std::setw(2) << settle_month << "-" << std::setw(2) << settle_day;
            std::string maturity_date = mature_oss.str();

            // Generate instrument data
            std::string symbol = symbols[row % symbols.size()];
            std::string currency = currencies[row % currencies.size()];
            double base_price = price_dist(gen);
            double bid = base_price - spread_dist(gen);
            double ask = base_price + spread_dist(gen);
            double last = bid + (ask - bid) * 0.3;
            int64_t vol = volume_dist(gen);
            double volatility = volatility_dist(gen);
            double notional = notional_dist(gen);
            double mtm = mtm_dist(gen);

            // Position book organization (0-9)
            r[col++] = traders[row % traders.size()];
            r[col++] = books[row % books.size()];
            r[col++] = accounts[row % accounts.size()];
            r[col++] = regions[row % regions.size()];
            r[col++] = desks[row % desks.size()];
            r[col++] = (int64_t)(row % 500 + 1);
            r[col++] = (int64_t)(row / 100 + 1);
            r[col++] = (int64_t)(row % 50);
            r[col++] = (row % 2 == 0) ? "Long" : "Short";
            r[col++] = (row % 3 == 0 ? "Active" : (row % 3 == 1 ? "Monitoring" : "Closed"));

            // Trade and position fields (10-18)
            r[col++] = settle_date;
            r[col++] = (int64_t)(1000 + row);
            r[col++] = notional;
            r[col++] = mtm;
            r[col++] = (mtm / (notional + 0.001)) * 100;
            r[col++] = margin_dist(gen);
            r[col++] = last;
            r[col++] = base_price;
            r[col++] = ((base_price - last) / (last + 0.001)) * 100;

            // Instrument identification (19-26)
            std::string instr_type = types[row % types.size()];
            r[col++] = symbol;
            r[col++] = "US" + std::to_string(1000000 + (row % 1000000));
            r[col++] = currency;
            r[col++] = instr_type;
            r[col++] = (row % 2 == 0) ? "CALL" : "PUT";
            r[col++] = base_price * (0.8 + (row % 40) / 20.0);
            r[col++] = maturity_date;
            r[col++] = exchanges[row % exchanges.size()];

            // Core pricing (27-46)
            r[col++] = bid;
            r[col++] = ask;
            r[col++] = (int64_t)vol;
            r[col++] = (int64_t)volume_dist(gen);
            r[col++] = volatility * 100;
            r[col++] = maturity_date;
            r[col++] = coupon_dist(gen);
            r[col++] = ytm_dist(gen);
            r[col++] = spread_basis_dist(gen);
            r[col++] = duration_dist(gen);
            r[col++] = (int64_t)volume_dist(gen);
            r[col++] = (int64_t)volume_dist(gen);
            r[col++] = (int64_t)volume_dist(gen);
            r[col++] = base_price;
            r[col++] = marketcap_dist(gen);
            r[col++] = (int64_t)dividend_dist(gen);
            r[col++] = (double)pe_dist(gen);
            r[col++] = bv_dist(gen);
            r[col++] = sectors[row % sectors.size()];
            r[col++] = base_price * (0.95 + (0.1 * (row % 10)) / 10.0);

            // The Greeks - First generation (47-56)
            r[col++] = delta_dist(gen);
            r[col++] = gamma_dist(gen);
            r[col++] = theta_dist(gen);
            r[col++] = vega_dist(gen);
            r[col++] = rho_dist(gen);
            r[col++] = lambda_dist(gen);
            r[col++] = vega_dist(gen) * 0.01;
            r[col++] = theta_dist(gen) * 0.1;
            r[col++] = vega_dist(gen) * 0.05;
            r[col++] = vega_dist(gen) * gamma_dist(gen);

            // The Greeks - Second generation (57-66)
            r[col++] = rho_dist(gen) * 0.01;
            r[col++] = delta_dist(gen) * gamma_dist(gen);
            r[col++] = theta_dist(gen) * vega_dist(gen);
            r[col++] = gamma_dist(gen) * gamma_dist(gen);
            r[col++] = volatility * delta_dist(gen);
            r[col++] = vega_dist(gen) / (volatility + 0.01);
            r[col++] = theta_dist(gen) / (volatility + 0.01);
            r[col++] = delta_dist(gen) * price_dist(gen) / 100;
            r[col++] = vega_dist(gen) * volatility / 100;
            r[col++] = rho_dist(gen) / 10000;

            // Additional Greeks (67-76)
            r[col++] = gamma_dist(gen) * 100;
            r[col++] = delta_dist(gen) / (gamma_dist(gen) + 0.001);
            r[col++] = vega_dist(gen) * delta_dist(gen);
            r[col++] = theta_dist(gen) * gamma_dist(gen);
            r[col++] = lambda_dist(gen) * delta_dist(gen);
            r[col++] = std::abs(delta_dist(gen)) * vega_dist(gen);
            r[col++] = vega_dist(gen) / (vega_dist(gen) + 0.001);
            r[col++] = theta_dist(gen) + gamma_dist(gen);
            r[col++] = rho_dist(gen) * lambda_dist(gen);
            r[col++] = delta_dist(gen) + gamma_dist(gen) + theta_dist(gen);

            // Risk metrics (77-86)
            r[col++] = std::abs(mtm) * 1.96;
            r[col++] = std::abs(mtm) * 2.33;
            r[col++] = volatility * std::sqrt(static_cast<double>(row % 250 + 1));
            r[col++] = (row % 100) / 100.0;
            r[col++] = margin_dist(gen);
            r[col++] = margin_dist(gen) * 1.5;
            r[col++] = mtm / (notional + 0.001);
            r[col++] = std::abs(delta_dist(gen)) * notional;
            r[col++] = vega_dist(gen) * notional / 100;
            r[col++] = theta_dist(gen) * notional;

            // Fill remaining 114 columns (87-200)
            for (int i = 0; i < 113; ++i) {
                switch (i % 15) {
                    case 0:  r[col++] = price_dist(gen); break;
                    case 1:  r[col++] = (int64_t)volume_dist(gen); break;
                    case 2:  r[col++] = volatility_dist(gen); break;
                    case 3:  r[col++] = spread_dist(gen); break;
                    case 4:  r[col++] = (double)pe_dist(gen); break;
                    case 5:  r[col++] = ytm_dist(gen); break;
                    case 6:  r[col++] = duration_dist(gen); break;
                    case 7:  r[col++] = marketcap_dist(gen); break;
                    case 8:  r[col++] = bv_dist(gen); break;
                    case 9:  r[col++] = coupon_dist(gen); break;
                    case 10: r[col++] = delta_dist(gen); break;
                    case 11: r[col++] = vega_dist(gen); break;
                    case 12: r[col++] = theta_dist(gen); break;
                    case 13: r[col++] = rho_dist(gen); break;
                    case 14: r[col++] = gamma_dist(gen); break;
                }
            }

            // Only do this if col == NUM_COLUMNS (sanity check)
            assert(col == NUM_COLUMNS);
            rows.push_back(std::move(r));
        }
        
        return rows;
    }
};

} // namespace gird