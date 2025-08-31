#include "json.hpp"
#include "http_server.hpp"
#include <boost/beast/core.hpp>
#include <boost/beast/http.hpp>
#include <boost/beast/version.hpp>
#include <boost/asio/ip/tcp.hpp>
#include <pqxx/pqxx>
#include <iostream>

using tcp = boost::asio::ip::tcp;
namespace http = boost::beast::http;

void run_http_server(unsigned short port)
{
    try
    {
        boost::asio::io_context ioc{1};
        tcp::acceptor acceptor{ioc, {tcp::v4(), port}};
        std::cout << "HTTP server listening on port " << port << std::endl;
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
            }
            else
            {
                res.set(http::field::content_type, "text/plain");
                res.body() = "Hello, world!";
            }
            res.prepare_payload();
            http::write(socket, res);
        }
    }
    catch (const std::exception &e)
    {
        std::cerr << "HTTP server error: " << e.what() << std::endl;
    }
}
