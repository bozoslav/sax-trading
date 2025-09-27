#include "http_server.hpp"
#include <cstdlib>
#include <iostream>

int main()
{
    unsigned short port = 8080;
    if (const char *p = std::getenv("PORT"))
    {
        try
        {
            port = static_cast<unsigned short>(std::stoi(p));
        }
        catch (...)
        {
            std::cerr << "Invalid PORT env, using default 8080\n";
        }
    }
    std::cout << "Starting server on port " << port << std::endl;
    run_http_server(port);
    return 0;
}