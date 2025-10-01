#include "json.hpp"
#include "http_server.hpp"
#include <boost/beast/core.hpp>
#include <boost/beast/http.hpp>
#include <boost/beast/version.hpp>
#include <boost/asio/ip/tcp.hpp>
#include <boost/asio/ssl.hpp>
#include <boost/beast/ssl.hpp>
#include <pqxx/pqxx>
#include <iostream>
#include <chrono>
#include <thread>
#include <mutex>
#include <atomic>
#include <cstdlib>
#include <algorithm>
#include <unordered_map>

using tcp = boost::asio::ip::tcp;
namespace http = boost::beast::http;

namespace
{
    std::mutex stocks_mtx;
    nlohmann::json stocks_cache = nlohmann::json::array();
    std::chrono::steady_clock::time_point stocks_last;
    std::atomic<bool> stocks_ready{false};
    std::atomic<bool> stocks_stop{false};
    int refresh_seconds = 60; // default
    std::string symbols_cfg = "AAPL,MSFT,TSLA,AMZN,GOOG";
    std::string stocks_provider = "STOOQ";             // options: STOOQ, YAHOO, TRADING212
    std::string scraper_url = "http://localhost:9000"; // for TRADING212 provider

    std::string to_upper(std::string s)
    {
        for (char &c : s)
            c = static_cast<char>(::toupper(static_cast<unsigned char>(c)));
        return s;
    }
    std::string to_lower(std::string s)
    {
        for (char &c : s)
            c = static_cast<char>(::tolower(static_cast<unsigned char>(c)));
        return s;
    }

    nlohmann::json fetch_once(bool allow_insecure_retry = true)
    {
        nlohmann::json result = nlohmann::json::array();
        std::string provider = to_upper(stocks_provider);
        if (provider == "STOOQ")
        {
            // Build CSV request to stooq.com for symbols (append .US for US stocks)
            std::string symbols_query;
            {
                std::stringstream ss(symbols_cfg);
                std::string item;
                bool first = true;
                while (std::getline(ss, item, ','))
                {
                    std::string trimmed;
                    for (char c : item)
                    {
                        if (!isspace(static_cast<unsigned char>(c)))
                            trimmed.push_back(c);
                    }
                    if (trimmed.empty())
                        continue;
                    if (!first)
                        symbols_query += ",";
                    first = false;
                    std::string upper = to_upper(trimmed);
                    symbols_query += upper + ".US"; // stooq symbol format requires uppercase
                }
            }
            std::string host = "stooq.com";
            std::string target = "/q/l/?s=" + symbols_query + "&f=sd2t2ohlcv&h&e=csv"; // include open/high/low/close
            auto fetch_stooq_http = [&](std::string scheme, std::string host_in, std::string tgt)
            {
                boost::asio::io_context ioc_api;
                if (scheme == "http")
                {
                    tcp::resolver resolver{ioc_api};
                    auto endpoints = resolver.resolve(host_in, "80");
                    tcp::socket sock{ioc_api};
                    boost::asio::connect(sock, endpoints.begin(), endpoints.end());
                    http::request<http::string_body> req_api{http::verb::get, tgt, 11};
                    req_api.set(http::field::host, host_in);
                    req_api.set(http::field::user_agent, "Mozilla/5.0");
                    req_api.set(http::field::accept, "text/csv");
                    http::write(sock, req_api);
                    boost::beast::flat_buffer buffer_api;
                    http::response<http::string_body> res_api;
                    http::read(sock, buffer_api, res_api);
                    return res_api;
                }
                else
                { // https
                    boost::asio::ssl::context tls_ctx{boost::asio::ssl::context::tls_client};
                    tls_ctx.set_default_verify_paths();
                    tls_ctx.set_verify_mode(boost::asio::ssl::verify_peer);
                    tcp::resolver resolver{ioc_api};
                    auto endpoints = resolver.resolve(host_in, "443");
                    boost::beast::ssl_stream<tcp::socket> stream{ioc_api, tls_ctx};
                    if (!SSL_set_tlsext_host_name(stream.native_handle(), host_in.c_str()))
                        throw std::runtime_error("SNI failure");
                    boost::asio::connect(stream.next_layer(), endpoints.begin(), endpoints.end());
                    stream.handshake(boost::asio::ssl::stream_base::client);
                    http::request<http::string_body> req_api{http::verb::get, tgt, 11};
                    req_api.set(http::field::host, host_in);
                    req_api.set(http::field::user_agent, "Mozilla/5.0");
                    req_api.set(http::field::accept, "text/csv");
                    http::write(stream, req_api);
                    boost::beast::flat_buffer buffer_api;
                    http::response<http::string_body> res_api;
                    http::read(stream, buffer_api, res_api);
                    return res_api;
                }
            };
            std::cerr << "[stooq] fetching symbols=" << symbols_query << std::endl;
            http::response<http::string_body> res_api = fetch_stooq_http("http", host, target);
            std::cerr << "[stooq] initial status=" << static_cast<int>(res_api.result()) << std::endl;
            if (res_api.result() == http::status::moved_permanently || res_api.result() == http::status::found)
            {
                auto loc_it = res_api.find(http::field::location);
                if (loc_it != res_api.end())
                {
                    std::string loc = std::string(loc_it->value());
                    // location could be full URL
                    if (loc.rfind("https://", 0) == 0)
                    {
                        std::string without = loc.substr(8); // after https://
                        auto slash = without.find('/');
                        std::string new_host = without.substr(0, slash);
                        std::string new_target = slash == std::string::npos ? "/" : without.substr(slash);
                        std::cerr << "[stooq] following redirect to host=" << new_host << " target=" << new_target << std::endl;
                        res_api = fetch_stooq_http("https", new_host, new_target);
                    }
                }
                else
                {
                    // fallback attempt https same target
                    std::cerr << "[stooq] no location header, trying https same target" << std::endl;
                    res_api = fetch_stooq_http("https", host, target);
                }
            }
            std::cerr << "[stooq] final status=" << static_cast<int>(res_api.result()) << " length=" << res_api.body().size() << std::endl;
            if (res_api.result() != http::status::ok)
            {
                throw std::runtime_error("stooq_upstream=" + std::to_string(static_cast<int>(res_api.result())));
            }
            // Parse CSV
            std::istringstream csv(res_api.body());
            std::string line;
            bool header = true;
            int parsed_rows = 0;
            while (std::getline(csv, line))
            {
                if (line.empty())
                    continue;
                if (line.size() > 0 && (line.back() == '\r' || line.back() == '\n'))
                    line.erase(std::remove_if(line.begin(), line.end(), [](char c)
                                              { return c == '\r' || c == '\n'; }),
                               line.end());
                if (header)
                {
                    header = false;
                    continue;
                }
                std::vector<std::string> cols;
                std::string col;
                std::stringstream ls(line);
                while (std::getline(ls, col, ','))
                    cols.push_back(col);
                if (cols.size() < 8)
                    continue; // Symbol,Date,Time,Open,High,Low,Close,Volume
                std::string symbol = cols[0];
                double open = std::atof(cols[3].c_str());
                double high = std::atof(cols[4].c_str());
                double low = std::atof(cols[5].c_str());
                double close = std::atof(cols[6].c_str());
                double change = close - open;
                double percent = open != 0.0 ? (change / open) * 100.0 : 0.0;
                result.push_back({{"symbol", symbol.substr(0, symbol.find('.'))},
                                  {"name", symbol},
                                  {"price", close},
                                  {"change", change},
                                  {"percent", percent}});
                parsed_rows++;
            }
            std::cerr << "[stooq] parsed_rows=" << parsed_rows << std::endl;
            if (parsed_rows == 0)
            {
                std::cerr << "[stooq] CSV body was: " << res_api.body() << std::endl;
            }
            // If not all requested symbols returned, perform per-symbol fallback
            std::vector<std::string> requested_symbols;
            {
                std::stringstream ss(symbols_cfg);
                std::string item;
                while (std::getline(ss, item, ','))
                {
                    std::string trimmed;
                    for (char c : item)
                    {
                        if (!isspace(static_cast<unsigned char>(c)))
                            trimmed.push_back(c);
                    }
                    if (!trimmed.empty())
                        requested_symbols.push_back(to_upper(trimmed));
                }
            }
            std::unordered_map<std::string, nlohmann::json> map_current;
            for (auto &o : result)
            {
                map_current[o["symbol"].get<std::string>()] = o;
            }
            if (map_current.size() < requested_symbols.size())
            {
                for (const auto &sym : requested_symbols)
                {
                    if (map_current.find(sym) != map_current.end())
                        continue;
                    try
                    {
                        std::string single = to_upper(sym) + ".US";
                        std::string single_target = "/q/l/?s=" + single + "&f=sd2t2ohlcv&h&e=csv";
                        auto single_res = fetch_stooq_http("https", host, single_target);
                        if (single_res.result() == http::status::ok)
                        {
                            std::istringstream scsv(single_res.body());
                            std::string l;
                            bool hdr = true;
                            while (std::getline(scsv, l))
                            {
                                if (hdr)
                                {
                                    hdr = false;
                                    continue;
                                }
                                if (l.empty())
                                    continue;
                                std::vector<std::string> cols;
                                std::stringstream ls(l);
                                std::string col;
                                while (std::getline(ls, col, ','))
                                    cols.push_back(col);
                                if (cols.size() < 8)
                                    continue;
                                double open = std::atof(cols[3].c_str());
                                double close = std::atof(cols[6].c_str());
                                double change = close - open;
                                double percent = open != 0 ? (change / open) * 100.0 : 0.0;
                                map_current[sym] = nlohmann::json{{"symbol", sym}, {"name", cols[0]}, {"price", close}, {"change", change}, {"percent", percent}};
                            }
                        }
                    }
                    catch (const std::exception &se)
                    {
                        std::cerr << "[stooq] per-symbol fetch failed sym=" << sym << " err=" << se.what() << std::endl;
                    }
                }
            }
            // Reconstruct ordered result, filtering symbols with price==0 if we have at least one non-zero price overall
            bool any_non_zero = false;
            for (auto &kv : map_current)
            {
                if (kv.second["price"].get<double>() != 0.0)
                {
                    any_non_zero = true;
                    break;
                }
            }
            nlohmann::json ordered = nlohmann::json::array();
            for (const auto &sym : requested_symbols)
            {
                auto it = map_current.find(sym);
                if (it == map_current.end())
                    continue;
                if (any_non_zero && it->second["price"].get<double>() == 0.0)
                    continue; // drop zero-only if we have real data
                ordered.push_back(it->second);
            }
            if (ordered.empty())
            {
                throw std::runtime_error("stooq_no_rows_after_fallback");
            }
            return ordered;
        }
        // TRADING212 provider (calls local scraper service)
        if (provider == "TRADING212")
        {
            std::cerr << "[trading212] calling scraper at " << scraper_url << std::endl;
            // Parse scraper_url to get host and port
            std::string scraper_host = "localhost";
            std::string scraper_port = "9000";
            std::string scraper_path = "/quotes?symbols=" + symbols_cfg;

            // Simple URL parsing (assumes http://host:port format)
            if (scraper_url.rfind("http://", 0) == 0)
            {
                std::string without_proto = scraper_url.substr(7);
                auto colon_pos = without_proto.find(':');
                if (colon_pos != std::string::npos)
                {
                    scraper_host = without_proto.substr(0, colon_pos);
                    scraper_port = without_proto.substr(colon_pos + 1);
                }
                else
                {
                    scraper_host = without_proto;
                }
            }

            boost::asio::io_context ioc_api;
            tcp::resolver resolver{ioc_api};
            auto endpoints = resolver.resolve(scraper_host, scraper_port);
            tcp::socket sock{ioc_api};
            boost::asio::connect(sock, endpoints.begin(), endpoints.end());
            http::request<http::string_body> req_api{http::verb::get, scraper_path, 11};
            req_api.set(http::field::host, scraper_host + ":" + scraper_port);
            req_api.set(http::field::user_agent, "exchange-backend/1.0");
            http::write(sock, req_api);
            boost::beast::flat_buffer buffer_api;
            http::response<http::string_body> res_api;
            http::read(sock, buffer_api, res_api);

            if (res_api.result() != http::status::ok)
            {
                throw std::runtime_error("scraper_upstream=" + std::to_string(static_cast<int>(res_api.result())));
            }

            auto scraper_json = nlohmann::json::parse(res_api.body());
            std::cerr << "[trading212] received " << scraper_json.size() << " symbols from scraper" << std::endl;
            return scraper_json;
        }
        // Default / YAHOO provider (original logic)
        std::string host = "query1.finance.yahoo.com";
        const std::string alt_host = "query2.finance.yahoo.com";
        std::string target = "/v7/finance/quote?symbols=" + symbols_cfg;
        auto perform = [&](bool insecure)
        {
            boost::asio::io_context ioc_api;
            boost::asio::ssl::context tls_ctx{boost::asio::ssl::context::tls_client};
            if (!insecure)
            {
                tls_ctx.set_default_verify_paths();
                tls_ctx.set_verify_mode(boost::asio::ssl::verify_peer);
            }
            else
            {
                tls_ctx.set_verify_mode(boost::asio::ssl::verify_none);
            }
            tcp::resolver resolver{ioc_api};
            auto endpoints = resolver.resolve(host, "443");
            boost::beast::ssl_stream<tcp::socket> stream{ioc_api, tls_ctx};
            if (!SSL_set_tlsext_host_name(stream.native_handle(), host.c_str()))
                throw std::runtime_error("SNI failure");
            boost::asio::connect(stream.next_layer(), endpoints.begin(), endpoints.end());
            stream.handshake(boost::asio::ssl::stream_base::client);
            http::request<http::string_body> req_api{http::verb::get, target, 11};
            req_api.set(http::field::host, host);
            req_api.set(http::field::user_agent, "Mozilla/5.0 (Macintosh) AppleWebKit/537.36 Chrome Safari");
            req_api.set(http::field::accept, "application/json,text/plain,*/*");
            req_api.set(http::field::accept_language, "en-US,en;q=0.9");
            req_api.set(http::field::accept_encoding, "identity");
            req_api.set(http::field::connection, "close");
            http::write(stream, req_api);
            boost::beast::flat_buffer buffer_api;
            http::response<http::string_body> res_api;
            http::read(stream, buffer_api, res_api);
            if (res_api.result() != http::status::ok)
            {
                std::string body_snip = res_api.body().substr(0, 200);
                throw std::runtime_error("upstream=" + std::to_string(static_cast<int>(res_api.result())) + " host=" + host + " body_snip=" + body_snip);
            }
            auto api_json = nlohmann::json::parse(res_api.body());
            auto quotes = api_json["quoteResponse"]["result"];
            for (const auto &stock : quotes)
            {
                result.push_back({{"symbol", stock.value("symbol", "")},
                                  {"name", stock.value("shortName", "")},
                                  {"price", stock.value("regularMarketPrice", 0.0)},
                                  {"change", stock.value("regularMarketChange", 0.0)},
                                  {"percent", stock.value("regularMarketChangePercent", 0.0)}});
            }
        };
        try
        {
            perform(false);
        }
        catch (const std::exception &e)
        {
            std::cerr << "[stocks-fetch] primary verified failed: " << e.what() << std::endl;
            if (!allow_insecure_retry)
                throw;
            try
            {
                perform(true);
            }
            catch (const std::exception &e2)
            {
                std::cerr << "[stocks-fetch] primary insecure failed: " << e2.what() << " switching host" << std::endl;
                host = alt_host;
                try
                {
                    perform(false);
                }
                catch (const std::exception &e3)
                {
                    std::cerr << "[stocks-fetch] alt host verified failed: " << e3.what() << std::endl;
                    try
                    {
                        perform(true);
                    }
                    catch (const std::exception &e4)
                    {
                        std::cerr << "[stocks-fetch] alt host insecure failed: " << e4.what() << std::endl;
                        throw;
                    }
                }
            }
        }
        return result;
    }

    void stocks_background_loop()
    {
        int attempts = 0;
        int consecutive_failures = 0;
        while (!stocks_stop.load())
        {
            try
            {
                auto data = fetch_once();
                if (!data.empty())
                {
                    {
                        std::lock_guard<std::mutex> lk(stocks_mtx);
                        stocks_cache = std::move(data);
                        stocks_last = std::chrono::steady_clock::now();
                        stocks_ready.store(true);
                    }
                    attempts = 0;
                    consecutive_failures = 0;
                }
            }
            catch (const std::exception &e)
            {
                std::cerr << "[stocks-bg] fetch error: " << e.what() << std::endl;
                consecutive_failures++;
                // After several failures, if we have never succeeded, expose empty list so UI stops showing 503
                if (!stocks_ready.load() && consecutive_failures >= 5)
                {
                    std::lock_guard<std::mutex> lk(stocks_mtx);
                    stocks_cache = nlohmann::json::array();
                    stocks_last = std::chrono::steady_clock::now();
                    stocks_ready.store(true);
                    std::cerr << "[stocks-bg] elevating empty cache after repeated failures" << std::endl;
                }
            }
            // if not ready yet use shorter retry interval up to 15s, else normal refresh
            int sleep_sec = stocks_ready.load() ? refresh_seconds : std::min(15, 2 + attempts * 2);
            for (int i = 0; i < sleep_sec && !stocks_stop.load(); ++i)
                std::this_thread::sleep_for(std::chrono::seconds(1));
            attempts++;
        }
    }
}

void run_http_server(unsigned short port)
{
    if (const char *envR = std::getenv("STOCKS_REFRESH_SECONDS"))
    {
        try
        {
            refresh_seconds = std::max(5, std::stoi(envR));
        }
        catch (...)
        {
        }
    }
    if (const char *envS = std::getenv("STOCKS_SYMBOLS"))
    {
        if (std::string(envS).size() > 0)
            symbols_cfg = envS;
    }
    if (const char *envP = std::getenv("STOCKS_PROVIDER"))
    {
        std::string val = to_upper(envP);
        if (val == "STOOQ" || val == "YAHOO" || val == "TRADING212")
            stocks_provider = val;
        else
            std::cerr << "[stocks] unknown provider '" << val << "' defaulting to " << stocks_provider << "\n";
    }
    if (const char *envU = std::getenv("SCRAPER_URL"))
    {
        scraper_url = envU;
    }
    std::thread bg(stocks_background_loop);
    bg.detach();
    try
    {
        boost::asio::io_context ioc{1};
        tcp::acceptor acceptor{ioc, {tcp::v4(), port}};
        std::cout << "HTTP server listening on port " << port << " (stocks refresh=" << refresh_seconds << "s)" << std::endl;
        for (;;)
        {
            tcp::socket socket{ioc};
            acceptor.accept(socket);

            boost::beast::flat_buffer buffer;
            http::request<http::string_body> req;
            http::read(socket, buffer, req);

            http::response<http::string_body> res{http::status::ok, req.version()};
            res.set(http::field::server, "Beast");
            res.set("Access-Control-Allow-Origin", "*");

            // /stocks endpoint (serve cached data)
            if (req.method() == http::verb::get && req.target() == "/stocks")
            {
                bool ready = stocks_ready.load();
                nlohmann::json snapshot;
                std::chrono::steady_clock::time_point ts;
                if (ready)
                {
                    std::lock_guard<std::mutex> lk(stocks_mtx);
                    snapshot = stocks_cache;
                    ts = stocks_last;
                }
                if (!ready)
                {
                    nlohmann::json err{{"error", "initializing"}, {"message", "Stock data not yet available"}};
                    res.result(http::status::service_unavailable);
                    res.set(http::field::content_type, "application/json");
                    res.body() = err.dump();
                    res.prepare_payload();
                    http::write(socket, res);
                    continue;
                }
                auto age = std::chrono::duration_cast<std::chrono::seconds>(std::chrono::steady_clock::now() - ts).count();
                bool stale = age > refresh_seconds * 2;
                res.set(http::field::content_type, "application/json");
                res.set("X-Data-Age-Seconds", std::to_string(age));
                res.set("X-Data-Refresh-Seconds", std::to_string(refresh_seconds));
                res.set("X-Data-Symbols", symbols_cfg);
                res.set("X-Data-Provider", stocks_provider);
                if (stale)
                    res.set("X-Data-Stale", "true");
                res.body() = snapshot.dump();
                res.prepare_payload();
                http::write(socket, res);
                continue;
            }

            // /orderbook endpoint
            if (req.method() == http::verb::get && req.target() == "/orderbook")
            {
                nlohmann::json orderbook = nlohmann::json::array();
                try
                {
                    pqxx::connection c{"dbname=exchange user=leonmamic"};
                    pqxx::work txn{c};
                    pqxx::result r = txn.exec("SELECT id, user_id, side, price, amount, status, created_at FROM orders");
                    for (const auto &row : r)
                    {
                        orderbook.push_back({{"id", row[0].as<int>()},
                                             {"user_id", row[1].as<int>()},
                                             {"side", row[2].as<std::string>()},
                                             {"price", row[3].as<double>()},
                                             {"amount", row[4].as<double>()},
                                             {"status", row[5].as<std::string>()},
                                             {"created_at", row[6].as<std::string>()}});
                    }
                }
                catch (const std::exception &e)
                {
                    res.result(http::status::internal_server_error);
                    res.set(http::field::content_type, "text/plain");
                    res.body() = std::string("Database error: ") + e.what();
                    res.prepare_payload();
                    http::write(socket, res);
                    continue;
                }
                res.set(http::field::content_type, "application/json");
                res.body() = orderbook.dump();
                res.prepare_payload();
                http::write(socket, res);
                continue;
            }

            // Default response
            res.set(http::field::content_type, "text/plain");
            res.body() = "Hello, world!";
            res.prepare_payload();
            http::write(socket, res);
        }
    }
    catch (const std::exception &e)
    {
        std::cerr << "HTTP server error: " << e.what() << std::endl;
    }
}
