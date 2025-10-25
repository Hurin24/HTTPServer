#include "FileSaver.h"

#include "server_http.hpp"
#include <nlohmann/json.hpp>
#include "spdlog/spdlog.h"
#include "spdlog/sinks/rotating_file_sink.h"
#include "spdlog/logger.h"

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


    //Создаём логгер
    auto max_size = 1024 * 1024 * 1024 * 2.5; //2.5 Мб, общий размер двух файлов лога 5 МБ
    auto max_files = 1;

    auto file_sink = std::make_shared<spdlog::sinks::rotating_file_sink_st>("log.txt", max_size, max_files);
    file_sink->set_level(spdlog::level::trace);

    auto logger = std::make_shared<spdlog::logger>("HTTP server logger", file_sink);
    logger->set_level(spdlog::level::trace);
    logger->flush_on(spdlog::level::trace);


    //Создаём сервер
    HttpServer server;
    server.config.address = ip;
    server.config.port = std::stoi(port);


    //Создаём сохраняльщик файлов
    FileSaver fileSaver;
    fileSaver.setLogger(logger);


    //GET запрос по пути /info
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


    //POST запрос по пути /upload
    server.resource["^/upload$"]["POST"] = [&fileSaver](auto response, auto request)
                                           {
                                                   fileSaver.setRequestHeader(request->header);
                                                   json result = fileSaver.processStream(request->content);

                                                   std::string response_content = result.dump();

                                                   *response << "HTTP/1.1 200 OK\r\n"
                                                             << "Content-Type: application/json\r\n"
                                                             << "Content-Length: " << response_content.length() << "\r\n"
                                                             << "\r\n" << response_content;
                                           };


    //GET запрос по пути /log
    server.resource["^/log$"]["GET"] = [](shared_ptr<HttpServer::Response> response, shared_ptr<HttpServer::Request> request)
                                       {
                                           try
                                           {
                                               const size_t MAX_TOTAL_SIZE = 1024 * 1024 * 1024 * 5; //5 Мб
                                               size_t total_size = 0;
                                               std::string content;
                                               bool log_exists = false;
                                               bool log1_exists = false;

                                               //Проверяем существование и размер log.1.txt
                                               std::ifstream file1("log.1.txt");
                                               if(file1)
                                               {
                                                   log1_exists = true;
                                                   file1.seekg(0, std::ios::end);
                                                   std::streampos size1 = file1.tellg();
                                                   file1.seekg(0, std::ios::beg);
                                                   total_size += size1;

                                                   if(size1 > 0)
                                                   {
                                                       content += std::string((std::istreambuf_iterator<char>(file1)), std::istreambuf_iterator<char>());
                                                   }
                                               }


                                               //Проверяем существование и размер log.txt
                                               std::ifstream file("log.txt");
                                               if(file)
                                               {
                                                   log_exists = true;
                                                   file.seekg(0, std::ios::end);
                                                   std::streampos size = file.tellg();
                                                   file.seekg(0, std::ios::beg);
                                                   total_size += size;

                                                   if(size > 0)
                                                   {
                                                       content += std::string((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
                                                   }
                                               }


                                               //Проверяем общий размер файлов
                                               if(total_size > MAX_TOTAL_SIZE)
                                               {
                                                   std::string size_warning = "Общий размер файла логов превышает 5 МБ: " + std::to_string(total_size) + " байт";
                                                   *response << "HTTP/1.1 200 OK\r\n"
                                                               << "Content-Type: text/plain; charset=utf-8\r\n"
                                                               << "Content-Length: " << size_warning.length() << "\r\n"
                                                               << "\r\n"
                                                               << size_warning;
                                                   return;
                                               }

                                               //Формируем ответ в зависимости от наличия файлов и их содержимого
                                               if(!content.empty())
                                               {
                                                   *response << "HTTP/1.1 200 OK\r\n"
                                                               << "Content-Type: text/plain; charset=utf-8\r\n"
                                                               << "Content-Length: " << content.length() << "\r\n"
                                                               << "\r\n"
                                                               << content;
                                               }
                                               else if(log_exists)
                                               {
                                                   std::string empty_content = "Файл логов существует, но пуст";
                                                   *response << "HTTP/1.1 200 OK\r\n"
                                                               << "Content-Type: text/plain; charset=utf-8\r\n"
                                                               << "Content-Length: " << empty_content.length() << "\r\n"
                                                               << "\r\n"
                                                               << empty_content;
                                               }
                                               else
                                               {
                                                   std::string no_files_content = "Файл логов отсутствует";
                                                   *response << "HTTP/1.1 200 OK\r\n"
                                                               << "Content-Type: text/plain; charset=utf-8\r\n"
                                                               << "Content-Length: " << no_files_content.length() << "\r\n"
                                                               << "\r\n"
                                                               << no_files_content;
                                               }
                                           }
                                           catch (const std::exception& e)
                                           {
                                               std::string error_content = "Ошибка: " + std::string(e.what());
                                               *response << "HTTP/1.1 500 Internal Server Error\r\n"
                                                           << "Content-Type: text/plain; charset=utf-8\r\n"
                                                           << "Content-Length: " << error_content.length() << "\r\n"
                                                           << "\r\n"
                                                           << error_content;
                                           }
                                       };


    std::string info = "Запущен сервер с IP = " + server.config.address + " и портом = " + std::to_string(server.config.port);
    logger->info(info);
    std::cout << info << std::endl;

    //Запуск сервера
    server.start();

    return 0;
}
