#include <nlohmann/json.hpp>

#include "FileSaver.h"
#include "server_http.hpp"
#include <fstream>

using namespace std;
using HttpServer = SimpleWeb::Server<SimpleWeb::HTTP>;
using json = nlohmann::json;

bool createUploadsDirectory()
{
    int result;
#ifdef _WIN32
    result = system("mkdir uploads 2>nul");
#else
    result = system("mkdir -p uploads");
#endif

    if (result == 0)
    {
        std::cout << "Uploads directory created successfully" << std::endl;
        return true;
    }
    else
    {
        std::cout << "Uploads directory already exists or cannot be created" << std::endl;
        return false;
    }
}


int main()
{
    if(!createUploadsDirectory())
    {
        return 1;
    }

    HttpServer server;
    server.config.port = 1616;
    server.config.address = "0.0.0.0";



    // GET-example for the path /info
    // Responds with request-information
    server.resource["^/info$"]["GET"] = [](shared_ptr<HttpServer::Response> response, shared_ptr<HttpServer::Request> request)
                                        {
                                            json info =
                                            {
                                                {"state:", "ok"}
                                            };

                                            auto content = info.dump(2);
                                            *response << "HTTP/1.1 200 OK\r\n"
                                                      << "Content-Type: application/json\r\n"
                                                      << "Content-Length: " << content.length() << "\r\n"
                                                      << "\r\n"
                                                      << content;
                                        };


    server.resource["^/upload$"]["POST"] = [](auto response, auto request)
                                           {

                                                   FileSaver fileSaver;

                                                   fileSaver.setRequestHeader(request->header);
                                                   json result = fileSaver.processStream(request->content);

                                                   std::string response_content = result.dump();

                                                   *response << "HTTP/1.1 200 OK\r\n"
                                                           << "Content-Type: application/json\r\n"
                                                           << "Content-Length: " << response_content.length() << "\r\n"
                                                           << "\r\n" << response_content;
                                           };

    thread server_thread(
        [&server]()
        {
            // Start server
            server.start();
        });

    server_thread.join();
    return 0;
}
