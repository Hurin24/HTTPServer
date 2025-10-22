#include <nlohmann/json.hpp>

#include "client_http.hpp"
#include "server_http.hpp"
#include <fstream>

using namespace std;
using HttpServer = SimpleWeb::Server<SimpleWeb::HTTP>;
using HttpClient = SimpleWeb::Client<SimpleWeb::HTTP>;
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


    //POST /upload - загрузка файла
    server.resource["^/upload$"]["POST"] = [](shared_ptr<HttpServer::Response> response, shared_ptr<HttpServer::Request> request)
{
    try
    {
        std::string filename = "upload_" + std::to_string(std::time(nullptr)) + ".dat";
        std::string filepath = "uploads/" + filename;

        // Сохраняем файл
        std::string content_str = request->content.string();
        std::ofstream file(filepath, std::ios::binary);
        file << content_str;
        file.close();

        // Простой ответ
        json result = {
            {"status", "success"},
            {"filename", filename},
            {"size", content_str.size()}
        };

        std::string content = result.dump();
        *response << "HTTP/1.1 200 OK\r\n"
                  << "Content-Type: application/json\r\n"
                  << "Content-Length: " << content.length() << "\r\n"
                  << "\r\n" << content;

    } catch (const std::exception& e) {
        json error = {{"error", e.what()}};
        std::string content = error.dump();
        *response << "HTTP/1.1 500 Internal Server Error\r\n"
                  << "Content-Type: application/json\r\n"
                  << "Content-Length: " << content.length() << "\r\n"
                  << "\r\n" << content;
    }
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
