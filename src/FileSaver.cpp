#include "FileSaver.h"

#include "FileSaver.h"
#include <ctime>
#include <stdexcept>

FileSaver::FileSaver() :
           m_state(WaitingRequestHeader),
           m_fileSize(0)
{

}

void FileSaver::setRequestHeader(const CaseInsensitiveMultimap& headers)
{
    m_requestHeadersMap = headers;
    m_boundary.clear();
    m_filename.clear();

    //Ищем заголовок Content-Type с boundary=
    auto it = headers.find("Content-Type");
    if(it != headers.end())
    {
        const std::string& contentType = it->second;

        //Проверяем, что это multipart/form-data
        if(contentType.find("multipart/form-data") != std::string::npos)
        {
            //Ищем подстроку boundary=
            std::string boundaryKey = "boundary=";
            size_t pos = contentType.find(boundaryKey);
            if(pos != std::string::npos)
            {
                //boundary начинается после "boundary="
                std::string boundary = contentType.substr(pos + boundaryKey.length());

                //Убираем возможные кавычки вокруг boundary
                if(!boundary.empty() && (boundary.front() == '"' || boundary.front() == '\''))
                {
                    boundary.erase(0,1);
                }
                if(!boundary.empty() && (boundary.back() == '"' || boundary.back() == '\''))
                {
                    boundary.pop_back();
                }

                m_boundary = boundary;
                m_state = WaitingBoundaryHeader;
                return;
            }
        }
    }

    m_state = ErrorState;
}

json FileSaver::processStream(std::istream& stream)
{
    std::string line;

    try
    {
        while(true)
        {
            if(m_state == FinishedRead || m_state == ErrorState)
            {
                break;
            }

            //Читаем строку из буфера
            if(!readLineFromBuffer(stream, line))
            {
                // Достигнут конец потока - обрабатываем в зависимости от текущего состояния
                if(m_state == ReadingData && m_file.is_open())
                {
                    // Корректно завершаем чтение файла
                    m_file.close();
                    json result = {
                        {"status", "success"},
                        {"filename", m_filename},
                        {"size", m_fileSize},
                        {"boundary", m_boundary}
                    };
                    m_filename.clear();
                    m_fileSize = 0;
                    m_state = FinishedRead;
                    return result;
                }
                else if(m_state == WaitingBoundaryHeader || m_state == ReadingBoundaryHeader)
                {
                    // Если ждем boundary, но поток закончился - это нормально
                    // (может быть пустой запрос или все файлы уже обработаны)
                    break;
                }
                else
                {
                    throw std::runtime_error("Unexpected end of stream in state: " + std::to_string(m_state));
                }
            }

            std::cout << "State: " << m_state << " Line: " << line << std::endl;

            //Определяем тип текущей строки
            TypeLine lineType = checkLineType(line);

            //Выполняем переход состояния
            m_state = m_transitionTable[m_state][lineType];

            //Выполняем действия в зависимости от состояния
            switch(m_state)
            {
                case ReadingBoundaryHeader:
                {
                    if(lineType == Boundary || lineType == BoundaryEnd)
                    {
                        //Нашли boundary, сбрасываем имя файла для новой части
                        m_filename.clear();
                    }
                    else if(lineType == Data)
                    {
                        //Анализируем дополнительные boundary заголовки
                        analyzeBoundaryHeader(line);
                    }
                    break;
                }
                case WaitingData:
                {
                    // Boundary header закончился, готовы к приему данных
                    break;
                }
                case ReadingData:
                {
                    if(lineType == Data)
                    {
                        //Начинаем чтение данных файла при первой Data строке
                        if(!m_file.is_open())
                        {
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

                        //Записываем данные файла
                        if(m_file.is_open())
                        {
                            m_file << line << "\n";
                            m_fileSize += line.size() + 1;
                        }
                    }
                    else if(lineType == Boundary || lineType == BoundaryEnd)
                    {
                        // Найден новый boundary - завершаем текущий файл
                        if(m_file.is_open())
                        {
                            m_file.close();
                        }

                        json result = {
                            {"status", "success"},
                            {"filename", m_filename},
                            {"size", m_fileSize},
                            {"boundary", m_boundary}
                        };

                        // Сбрасываем для следующего файла
                        m_filename.clear();
                        m_fileSize = 0;

                        // Если это конечный boundary, завершаем обработку
                        if(lineType == BoundaryEnd)
                        {
                            m_state = FinishedRead;
                            return result;
                        }
                        else
                        {
                            // Переходим к обработке следующего boundary
                            m_state = ReadingBoundaryHeader;
                            return result;
                        }
                    }
                    break;
                }
                case FinishedRead:
                {
                    //Найден конец boundary - завершаем файл
                    if(m_file.is_open())
                    {
                        m_file.close();
                    }

                    json result = {
                        {"status", "success"},
                        {"filename", m_filename},
                        {"size", m_fileSize},
                        {"boundary", m_boundary}
                    };

                    // Сбрасываем для следующего файла
                    m_filename.clear();
                    m_fileSize = 0;
                    return result;
                }
                case ErrorState:
                {
                    if(m_file.is_open())
                    {
                        m_file.close();
                        std::remove(("uploads/" + m_filename).c_str());
                    }
                    throw std::runtime_error("Parser error in state: " + std::to_string(m_state));
                }
                default:
                    break;
            }
        }

        // Если дошли сюда без ошибок, но не нашли файлов - возвращаем пустой результат
        if(m_state == WaitingBoundaryHeader || m_state == FinishedRead)
        {
            return {
                {"status", "success"},
                {"filename", ""},
                {"size", 0},
                {"boundary", m_boundary},
                {"message", "No files found in stream"}
            };
        }

        throw std::runtime_error("Unexpected end of processing in state: " + std::to_string(m_state));
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

bool FileSaver::readLineFromBuffer(std::istream& stream, std::string& line)
{
    line.clear();

    if(std::getline(stream, line))
    {
        return true;
    }

    return false;
}

FileSaver::TypeLine FileSaver::checkLineType(const std::string& line)
{
    // Быстрая проверка на NewLine (пустая строка или только \r\n)
    if(line.empty() || line == "\r")
    {
        return NewLine;
    }

    // Быстрая проверка на Boundary (начинается с "--")
    if(line.size() >= 2 && line[0] == '-' && line[1] == '-')
    {
        if(!m_boundary.empty())
        {
            // Проверяем полное соответствие boundary
            std::string expectedBoundary = "--" + m_boundary;
            std::string expectedEndBoundary = expectedBoundary + "--";

            if(line == expectedEndBoundary)
            {
                return BoundaryEnd;
            }
            else if(line == expectedBoundary)
            {
                return Boundary;
            }
        }

        // Если boundary не установлен или не совпадает, проверяем по формату
        if(line.size() > 4 && line.substr(line.size() - 2) == "--")
        {
            return BoundaryEnd;
        }
        return Boundary;
    }

    // Все остальное - данные
    return Data;
}

bool FileSaver::isRequestHeaderStart(const std::string& line)
{
    return (line.find("POST") == 0 || line.find("GET") == 0 ||
            line.find("PUT") == 0 || line.find("DELETE") == 0);
}

void FileSaver::extractFilename(const std::string& line)
{
    if(line.find("filename=\"") != std::string::npos)
    {
        size_t start = line.find("filename=\"") + 10;
        size_t end = line.find("\"", start);

        if(start != std::string::npos && end != std::string::npos)
        {
            m_filename = line.substr(start, end - start);

            // Убираем путь из имени файла
            size_t last_slash = m_filename.find_last_of("/\\");
            if(last_slash != std::string::npos)
            {
                m_filename = m_filename.substr(last_slash + 1);
            }
        }
    }
}

void FileSaver::analyzeBoundaryHeader(const std::string& line)
{
    if(line.find("Content-Disposition") != std::string::npos)
    {
        extractFilename(line);
    }
    else if(line.find("Content-Type:") != std::string::npos)
    {
        // Можно сохранить тип контента если нужно
    }
}