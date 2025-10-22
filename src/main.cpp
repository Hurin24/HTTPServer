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

class FileSaver
{
public:
    // Состояния парсера
    enum ParserState : uint8_t
    {
        Waiting23Dashes,      //Ожидаем 23 тире (начало boundary)
        WaitingFileName,      //Ожидаем строку с именем файла
        WaitingNewLines,      //Ожидаем \r\n\r\n
        ReadingFile,          //Читаем данные файла
        FinishedRead,         //Завершено успешно
        ErrorState,           //Ошибка
        QuantityParserState   //Количество состояний
    };

    // События парсера
    enum ParserEvent : uint8_t
    {
        Found23Dashes,        //Найдены 23 тире
        FoundFileName,        //Найдено имя файла
        FoundNewLines,        //Найдены \r\n\r\n
        FoundEndBoundary,     //Найден конец файла (boundary)
        AnotherLine,          //Обычная строка
        QuantityParserEvent   //Количество событий
    };

    FileSaver() : m_state(Waiting23Dashes), m_fileSize(0)
    {

    }

    json processStream(std::istream& stream)
    {
        std::string line;

        try
        {
            while(std::getline(stream, line))
            {
                if(m_state == FinishedRead || m_state == ErrorState)
                {
                    break;
                }

                //Определяем тип текущей строки
                ParserEvent event = checkLineType(line);

                //Выполняем переход состояния
                m_state = m_transitionTable[m_state][event];

                //Выполняем действия в зависимости от состояния
                switch (m_state)
                {
                    case WaitingNewLines:
                    {
                        //Имя файла уже извлечено, готовимся к чтению данных
                        break;
                    }
                    case ReadingFile:
                    {
                        if(event == FoundNewLines)
                        {
                            //Первый переход в ReadingFile - открываем файл
                            if(m_filename.empty())
                            {
                                m_filename = "upload_" + std::to_string(std::time(nullptr)) + ".dat";
                            }

                            m_file.open("uploads/" + m_filename, std::ios::binary);

                            if(!m_file.is_open())
                            {
                                throw std::runtime_error("Cannot open file: uploads/" + m_filename);
                            }
                        }
                        else if(event == AnotherLine)
                        {
                            m_file << line << "\n";
                            m_fileSize += line.size() + 1;
                        }
                        else if(event == Found23Dashes)
                        {
                            //Закрываем текущий файл и возвращаем результат
                            if(m_file.is_open())
                            {
                                m_file.close();
                            }

                            json result = {
                                {"status", "success"},
                                {"filename", m_filename},
                                {"size", m_fileSize}
                            };

                            //Сбрасываем для следующего файла
                            m_filename.clear();
                            m_fileSize = 0;
                            m_state = WaitingFileName; // Переходим в ожидание имени следующего файла

                            return result; // ВОЗВРАЩАЕМ РЕЗУЛЬТАТ СРАЗУ
                        }
                        break;
                    }
                    case FinishedRead:
                    {
                        if(m_file.is_open())
                        {
                            m_file.close();
                        }
                        return {
                                   {"status", "success"},
                                   {"filename", m_filename},
                                   {"size", m_fileSize}
                               };
                    }
                    case ErrorState:
                    {
                        if(m_file.is_open())
                        {
                            m_file.close();
                            //Удаляем частично записанный файл
                            std::remove(("uploads/" + m_filename).c_str());
                        }
                        throw std::runtime_error("Parser error in state: " + std::to_string(m_state));
                    }
                    default:
                        break;
                }
            }

            //Если вышли из цикла, но не завершили чтение
            if(m_state == ReadingFile && m_file.is_open())
            {
                m_file.close();
                return {
                           {"status", "success"},
                           {"filename", m_filename},
                           {"size", m_fileSize}
                       };
            }

            throw std::runtime_error("Unexpected end of stream");
        }
        catch(const std::exception& e)
        {
            if(m_file.is_open())
            {
                m_file.close();
                std::remove(("uploads/" + m_filename).c_str());
            }
            throw;
        }
    }

private:
    ParserState m_state;
    std::string m_filename;
    std::ofstream m_file;
    std::string m_boundary;
    size_t m_fileSize;

    //Таблица переходов состояний
    ParserState m_transitionTable[QuantityParserState][QuantityParserEvent] = {
        //Found23Dashes     FoundFileName    FoundNewLines    FoundEndBoundary    AnotherLine
        { WaitingFileName,  ErrorState,      ErrorState,      ErrorState,         Waiting23Dashes }, //Waiting23Dashes
        { ErrorState,       WaitingNewLines, ErrorState,      ErrorState,         WaitingFileName }, //WaitingFileName
        { ErrorState,       ErrorState,      ReadingFile,     ErrorState,         WaitingNewLines }, //WaitingNewLines
        { Waiting23Dashes,  ReadingFile,     ReadingFile,     FinishedRead,       ReadingFile },     //ReadingFile
        { FinishedRead,     FinishedRead,    FinishedRead,    FinishedRead,       FinishedRead },    //FinishedRead
        { ErrorState,       ErrorState,      ErrorState,      ErrorState,         ErrorState }       //ErrorState
    };

    // Проверяем тип строки
    ParserEvent checkLineType(const std::string& line)
    {
        switch(m_state)
        {
            case Waiting23Dashes:
            {
                if(checkDashesLine(line))  //Ищем тире
                {
                    m_boundary = line;
                    return Found23Dashes;
                }
                break;
            }
            case WaitingFileName:
            {
                if(line.find("Content-Disposition: form-data; name=\"file\"; filename=\"") != std::string::npos)
                {
                    //Извлекаем имя файла
                    size_t start = line.find("filename=\"") + 10;
                    size_t end = line.find("\"", start);

                    if(start != std::string::npos && end != std::string::npos)
                    {
                        m_filename = line.substr(start, end - start);

                        //Убираем путь из имени файла
                        size_t last_slash = m_filename.find_last_of("/\\");
                        if (last_slash != std::string::npos)
                        {
                            m_filename = m_filename.substr(last_slash + 1);
                        }
                        return FoundFileName;
                    }
                }
                break;
            }
            case WaitingNewLines:
            {
                if(line == "\r" || line.empty())
                {
                    return FoundNewLines;
                }
                break;
            }
            case ReadingFile:
            {
                if(!m_boundary.empty() && line.find(m_boundary) == 0)
                {
                    return FoundEndBoundary;
                }

                if(checkDashesLine(line))
                {
                    return Found23Dashes;
                }
                break;
            }
            default:
                break;
        }

        return AnotherLine;
    }

    bool checkDashesLine(const std::string& line)
    {
        if(line.empty() || line[0] != '-')
        {
            return false;
        }

        //Считаем количество подряд идущих тире
        size_t dashCount = 0;
        for(char c : line)
        {
            if(c == '-')
            {
                dashCount++;
            }
            else
            {
                break; //Прерываем при первом не-тире
            }
        }

        //Проверяем что есть 23 тире и затем есть ещё данные
        return (dashCount >= 23) && (dashCount < line.length());
    }
};

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
                                               try
                                               {
                                                   FileSaver fileSaver;
                                                   json result = fileSaver.processStream(request->content);

                                                   std::string response_content = result.dump();
                                                   *response << "HTTP/1.1 200 OK\r\n"
                                                           << "Content-Type: application/json\r\n"
                                                           << "Content-Length: " << response_content.length() << "\r\n"
                                                           << "\r\n" << response_content;

                                               }
                                               catch(const std::exception& e)
                                               {
                                                   std::cerr << "Upload error: " << e.what() << std::endl;
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
