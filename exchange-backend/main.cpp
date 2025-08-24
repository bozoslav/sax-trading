// Boost.Beast i libpqxx
#include <boost/beast/core.hpp>
#include <boost/beast/http.hpp>
#include <boost/beast/version.hpp>
#include <boost/asio/ip/tcp.hpp>
#include <boost/asio/signal_set.hpp>
#include <pqxx/pqxx>
#include <iostream>

int main()
{
    try
    {
        pqxx::connection c{"dbname=exchange user=leonmamic"};
        pqxx::work txn{c};
        pqxx::result r = txn.exec("SELECT * FROM orders");
        std::cout << "Orders in DB:" << std::endl;
        for (auto row : r)
        {
            std::cout << row[0].c_str() << std::endl;
        }
    }
    catch (const std::exception &e)
    {
        std::cerr << "Error: " << e.what() << std::endl;
    }
    return 0;
}
