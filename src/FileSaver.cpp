#include "FileSaver.h"

#include "FileSaver.h"
#include <ctime>
#include <stdexcept>

FileSaver::FileSaver() :
           m_state(WaitingRequestHeader),
           m_fileSize(0)
{
    descriptionUploadedFiles = json::array();
}

void FileSaver::setRequestHeader(const CaseInsensitiveMultimap& headers)
{
    m_requestHeadersMap = headers;
    m_boundary.clear();
    m_boundaryExtended.clear();
    m_boundaryEnd.clear();
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
                m_boundaryExtended = "--" + m_boundary;
                m_boundaryEnd = m_boundaryExtended + "--";

                setState(WaitingBoundary);
                return;
            }
        }
    }

    //Если не нашли boundary, переходим в состояние ошибки
    setState(ErrorState);
    lastError = "Content-Type with boundary not found in request headers";
}

json FileSaver::processStream(std::istream& stream)
{
    std::string line;

    while(true)
    {
        //Читаем строку из буфера
        if(!readLineFromBuffer(stream, line))
        {
            //Не удалось считать

            //Если файл открыт
            if(m_file.is_open())
            {
                //Файл открыт - корректно завершаем запись файла
                m_file.close();

                json descriptionFile = {
                                           {"filename", m_filename},
                                           {"size", m_fileSize}
                                       };

                addFileToDescriptionUploadedFiles(descriptionFile);
            }

            if(m_state != FinishedRead)
            {
                json result = {
                                  {"status", "error"},
                                  {"description", "The file was finished read in unexpected state: " + std::to_string(m_state)}
                              };

                return result;
            }

            json result = {
                              {"status", "success"},
                              {"uploadedFiles", descriptionUploadedFiles}
                          };

            return result;
        }

        //Определяем тип текущей строки
        TypeLine lineType = getLineType(line);

        //Выполняем переход состояния
        FileSaverState newState = m_transitionTable[m_state][lineType];
        setState(newState);

        //Анализируем содержимое линии исходя из состояния
        if(!analyzeLine(line))
        {
            //Если одно из состояний вернуло false, значит произошла ошибка
            json result = {
                              {"status", "error"},
                              {"description", lastError}
                          };

            return result;
        }

        // Если достигли конечного состояния, завершаем обработку
        if(m_state == FinishedRead || m_state == ErrorState)
        {
            break;
        }
    }

    if(m_state == FinishedRead)
    {
        json result = {
                          {"status", "success"},
                          {"uploadedFiles", descriptionUploadedFiles}
                      };

        return result;
    }
    else
    {
        json result = {
                          {"status", "error"},
                          {"description", lastError}
                      };

        return result;
    }
}

void FileSaver::addFileToDescriptionUploadedFiles(json& newDescriptionFile)
{
    descriptionUploadedFiles.push_back(newDescriptionFile);
}

void FileSaver::setState(FileSaverState newState)
{
    m_state = newState;
}

bool FileSaver::waitingRequestHeader(const std::string& line)
{
    lastError = "The request header was not read properly";
    return false;
}

bool FileSaver::waitingBoundary(const std::string& line)
{
    //В этом состоянии мы ожидаем boundary, но сама обработка будет в wasReadBoundary
    return true;
}

bool FileSaver::wasReadBoundary(const std::string& line)
{
    //Завершаем текущий файл
    if(m_file.is_open())
    {
        m_file.close();

        json descriptionFile = {
                                   {"filename", m_filename},
                                   {"size", m_fileSize}
                               };

        addFileToDescriptionUploadedFiles(descriptionFile);

        //Сбрасываем для следующего файла
        m_filename.clear();
        m_fileSize = 0;
    }

    //После чтения boundary переходим к ожиданию Content-Disposition
    setState(WaitingContentDisposition);
    return true;
}

bool FileSaver::waitingContentDisposition(const std::string& line)
{
    //В этом состоянии мы ожидаем Content-Disposition, но обработка будет в wasReadContentDisposition
    return true;
}

bool FileSaver::wasReadContentDisposition(const std::string& line)
{
    //Извлекаем имя файла из Content-Disposition
    m_filename = extractFilenameFromContentDisposition(line);

    //После чтения Content-Disposition переходим к ожиданию новой строки
    setState(WaitingNewLine);
    return true;
}

bool FileSaver::waitingNewLine(const std::string& line)
{
    //В этом состоянии мы ожидаем новую строку, но обработка будет в wasReadNewLine
    return true;
}

bool FileSaver::wasReadNewLine(const std::string& line)
{
    //После чтения новой строки переходим к чтению данных
    setState(ReadingData);
    return true;
}

bool FileSaver::readingData(const std::string& line)
{
    TypeLine lineType = getLineType(line);

    //Если это данные, записываем их в файл

    //Открываем файл при первой записи данных
    if(!m_file.is_open())
    {
        if(m_filename.empty())
        {
            m_filename = "upload_" + std::to_string(std::time(nullptr)) + ".dat";
        }

        m_file.open("uploads/" + m_filename, std::ios::binary);
        if(!m_file.is_open())
        {
            lastError = "Cannot open file: uploads/" + m_filename;
            return false;
        }

        m_fileSize = 0;
    }

    //Записываем данные с переводом строки
    m_file << line;
    m_fileSize += line.size();

    return true;
}

bool FileSaver::wasReadBoundaryEnd(const std::string& line)
{
    //Завершаем текущий файл
    if(m_file.is_open())
    {
        m_file.close();

        json descriptionFile = {
                                   {"filename", m_filename},
                                   {"size", m_fileSize}
                               };

        addFileToDescriptionUploadedFiles(descriptionFile);

        //Сбрасываем для следующего файла
        m_filename.clear();
        m_fileSize = 0;
    }

    //Переходим в конечное состояние
    setState(FinishedRead);
    return true;
}

bool FileSaver::finishedRead(const std::string& line)
{
    //В конечном состоянии не ожидаем больше данных
    lastError = "Extra data after finishing reading";
    return false;
}

bool FileSaver::errorState(const std::string& line)
{
    lastError = "Impossible state";
    return false;
}

bool FileSaver::analyzeLine(const std::string& line)
{
    switch(m_state)
    {
        case WaitingRequestHeader:
        {
            return waitingRequestHeader(line);
        }
        case WaitingBoundary:
        {
            return waitingBoundary(line);
        }
        case WasReadBoundary:
        {
            return wasReadBoundary(line);
        }
        case WaitingContentDisposition:
        {
            return waitingContentDisposition(line);
        }
        case WasReadContentDisposition:
        {
            return wasReadContentDisposition(line);
        }
        case WaitingNewLine:
        {
            return waitingNewLine(line);
        }
        case WasReadNewLine:
        {
            return wasReadNewLine(line);
        }
        case ReadingData:
        {
            return readingData(line);
        }
        case WasReadBoundaryEnd:
        {
            return wasReadBoundaryEnd(line);
        }
        case FinishedRead:
        {
            return finishedRead(line);
        }
        case ErrorState:
        {
            return errorState(line);
        }
        default:
            lastError = "Unknown state: " + std::to_string(m_state);
            return false;
    }
}

bool FileSaver::readLineFromBuffer(std::istream& stream, std::string& line)
{
    line.clear();

    if(std::getline(stream, line))
    {
        line.push_back('\n');
        return true;
    }

    return false;
}

FileSaver::TypeLine FileSaver::getLineType(const std::string& line)
{
    //Быстрая проверка на NewLine (пустая строка)
    if(line.empty() || line == "\r\n" || line == "\n")
    {
        return NewLine;
    }


    //Быстрая проверка на Boundary (начинается с "-")
    if(line[0] == '-')
    {
        if(line.size() >= m_boundaryExtended.size())
        {
            if(line.find(m_boundaryEnd) != std::string::npos)
            {
                return BoundaryEnd;
            }

            if(line.find(m_boundaryExtended) != std::string::npos)
            {
                return Boundary;
            }
        }
    }

    if(line[0] == 'C' || line[0] == 'c')
    {
        //Проверка на Content-Disposition
        if(line.find("Content-Disposition:") != std::string::npos ||
           line.find("content-disposition:") != std::string::npos)
        {
            return ContentDisposition;
        }
    }

    //Все остальное - данные
    return Data;
}

std::string FileSaver::extractFilenameFromContentDisposition(const std::string& line)
{
    std::string tempFilename;

    if(line.find("filename=\"") != std::string::npos)
    {
        size_t start = line.find("filename=\"") + 10;
        size_t end = line.find("\"", start);

        if(start != std::string::npos && end != std::string::npos && end > start)
        {
            tempFilename = line.substr(start, end - start);

            // Убираем путь из имени файла
            size_t last_slash = tempFilename.find_last_of("/\\");
            if(last_slash != std::string::npos)
            {
                tempFilename = tempFilename.substr(last_slash + 1);
            }
        }
    }

    return tempFilename;
}
