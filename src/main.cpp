#include <nlohmann/json.hpp>

#include "FileSaver.h"
#include "server_http.hpp"
#include <fstream>

using namespace std;
using HttpServer = SimpleWeb::Server<SimpleWeb::HTTP>;
using json = nlohmann::json;


bool isValidIP(const std::string& ip)
{
    //Регулярное выражение для IPv4
    std::regex ipRegex("^((25[0-5]|2[0-4][0-9]|[01]?[0-9][0-9]?)\\.){3}"
                        "(25[0-5]|2[0-4][0-9]|[01]?[0-9][0-9]?)$");

    return std::regex_match(ip, ipRegex);
}

bool isValidPort(const std::string& portStr)
{
    //Проверка что строка состоит только из цифр
    if(portStr.empty() || !std::all_of(portStr.begin(), portStr.end(), ::isdigit))
    {
        return false;
    }

    try
    {
        int port = std::stoi(portStr);
        return port > 0 && port <= 65535;
    }
    catch(const std::exception&)
    {
        return false;
    }
}

bool isUserPort(const std::string& portStr)
{
    if(!isValidPort(portStr))
    {
        return false;
    }

    int port = std::stoi(portStr);
    return port >= 1024 && port <= 65535;
}

void printUsage(const char* programName)
{
    std::cout << "Использование: " << programName << " <IP-адрес> <порт>" << std::endl;
    std::cout << "Пример: " << programName << " 192.168.1.1 8080" << std::endl;
    std::cout << "IP-адрес должен быть валидным IPv4 адресом" << std::endl;
    std::cout << "Порт должен быть в диапазоне 1-65535" << std::endl;
    std::cout << "Порт должен быть ≥ 1024" << std::endl;
}

bool checkRootPrivileges()
{
    uid_t uid = getuid();
    uid_t euid = geteuid();

    if(uid == 0 || euid == 0)
    {
        return true;
    }
    else
    {
        return false;
    }
}

bool createUploadsDirectory()
{
    int result;
#ifdef _WIN32
    result = system("mkdir uploads 2>nul");
#else
    result = system("mkdir -p uploads");
#endif

    if(result == 0)
    {
        return true;
    }
    else
    {
        return false;
    }
}

int main(int argc, char* argv[])
{
    //Проверка количества аргументов
    if(argc != 3)
    {
        std::cerr << "Ошибка: неверное количество аргументов!" << std::endl;
        printUsage(argv[0]);
        return 1;
    }

    std::string ip = argv[1];
    std::string port = argv[2];

    //Проверка IP-адреса
    if(!isValidIP(ip))
    {
        std::cerr << "Ошибка: невалидный IP-адрес: " << ip << std::endl;
        std::cerr << "IP-адрес должен быть в формате XXX.XXX.XXX.XXX" << std::endl;
        return 1;
    }

    //Проверка порта
    if(!isValidPort(port))
    {
        std::cerr << "Ошибка: невалидный порт: " << port << std::endl;
        std::cerr << "Порт должен быть числом в диапазоне 1024 - 65535" << std::endl;
        return 1;
    }

    //Проверка диапазона порта
    if(!isUserPort(port))
    {
        std::cerr << "Ошибка: невалидный порт: " << port << std::endl;
        std::cerr << "Порт должен быть числом в диапазоне 1024 - 65535" << std::endl;
        return 1;
    }

    if(!checkRootPrivileges())
    {
        std::cerr << "Ошибка: программа должна быть запущена с правами root!" << std::endl;
        return 1;
    }

    if(!createUploadsDirectory())
    {
        std::cerr << "Не удалось создать директорию uploads" << std::endl;
        return 1;
    }

    HttpServer server;
    server.config.address = ip;
    server.config.port = std::stoi(port);


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
        std::cout << "Запущен сервер с IP = " << server.config.address << " и портом = " << server.config.port << std::endl;

        //Запуск сервера
        server.start();
    });

    server_thread.join();
    return 0;
}
