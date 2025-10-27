#include "FileSaver.h"

#include "FileSaver.h"
#include <ctime>
#include <stdexcept>

#define WRITE_TO_LOGGER(a) \
if(m_logger) \
{ \
    m_logger->trace(a); \
}

FileSaver::FileSaver() :
           m_state(WaitingRequestHeader),
           m_fileSize(0)
{
    descriptionUploadedFiles = json::array();
}

void FileSaver::setRequestHeader(const CaseInsensitiveMultimap& headers)
{
    //Завершаем текущий файл если он открыт
    closeFileAndResetValues();

    //Сбрасываем
    m_boundary.clear();
    m_boundaryExtended.clear();
    m_boundaryEnd.clear();
    descriptionUploadedFiles.clear();


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
                    boundary.erase(0, 1);
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
    setLastError("Content-Type with boundary not found in request headers");
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

            //Завершаем текущий файл если он открыт
            closeFileAndResetValues();

            if(m_state != FinishedRead)
            {
                setLastError("The file was finished read in unexpected state: " + std::to_string(m_state));

                json result = {
                                  {"status", "error"},
                                  {"description", m_lastError}
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
                              {"description", m_lastError}
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
                          {"description", m_lastError}
                      };

        return result;
    }
}

void FileSaver::setLogger(std::shared_ptr<spdlog::logger> newLogger)
{
    m_logger = newLogger;
}

void FileSaver::setDir(std::string newDir)
{
    m_dir = newDir;
}

void FileSaver::addFileToDescriptionUploadedFiles(json& newDescriptionFile)
{
    descriptionUploadedFiles.push_back(newDescriptionFile);
}

void FileSaver::setState(FileSaverState newState)
{
    m_state = newState;
}

void FileSaver::setLastError(std::string newLastError)
{
    m_lastError = newLastError;

    WRITE_TO_LOGGER("Error occured: " + m_lastError);
}

bool FileSaver::waitingRequestHeader(std::string& line)
{
    //Если всё делать правильно то никогда не должны попасть в эту функцию
    setLastError("The request header was not read properly");
    return false;
}

bool FileSaver::wasReadRequestHeader()
{
    //Функция для перехода в состояние WaitingBoundary
    setState(WaitingBoundary);
    return true;
}

bool FileSaver::waitingBoundary(std::string& line)
{
    //В этом состоянии мы ожидаем boundary, но сама обработка будет в wasReadBoundary
    return true;
}

bool FileSaver::wasReadBoundary(std::string& line)
{
    //Завершаем текущий файл если он открыт
    closeFileAndResetValues();

    //После чтения boundary переходим к ожиданию Content-Disposition
    setState(WaitingContentDisposition);
    return true;
}

bool FileSaver::waitingContentDisposition(std::string& line)
{
    //В этом состоянии мы ожидаем Content-Disposition, но обработка будет в wasReadContentDisposition
    return true;
}

bool FileSaver::wasReadContentDisposition(std::string& line)
{
    //Извлекаем имя файла из Content-Disposition
    m_filename = extractFilenameFromContentDisposition(line);

    //Открываем файл
    if(!m_file.is_open())
    {
        if(m_filename.empty())
        {
            m_filename = "upload_" + std::to_string(std::time(nullptr)) + ".dat";
        }

        m_file.open(m_dir + "/" + m_filename, std::ios::binary);
        if(!m_file.is_open())
        {
            setLastError("Cannot open file: " + m_dir + "/" + m_filename);
            return false;
        }

        m_fileSize = 0;
    }

    //После чтения Content-Disposition переходим к ожиданию новой строки
    setState(WaitingNewLine);
    return true;
}

bool FileSaver::waitingNewLine(std::string& line)
{
    //В этом состоянии мы ожидаем новую строку, но обработка будет в wasReadNewLine
    return true;
}

bool FileSaver::wasReadNewLine(std::string& line)
{
    //После чтения новой строки переходим к чтению данных
    setState(WaitingData);
    return true;
}

bool FileSaver::waitingData(std::string& line)
{
    return false;
}

bool FileSaver::wasReadData(std::string& line)
{
    if(!m_newline.empty())
    {
        bool result = writeLineToFile(m_newline);

        m_newline.clear();

        if(!result)
        {
            return result;
        }
    }


    if(line.size() >= 2)
    {
        if(line[line.size() - 2] == '\r' && line[line.size() - 1] == '\n')
        {
            line.resize(line.size() - 2);

            m_newline = "\r\n";
        }
    }


    bool result = writeLineToFile(line);

    if(!result)
    {
        return result;
    }


    setState(WaitingBoundaryEnd);
    return true;
}

bool FileSaver::waitingBoundaryEnd(std::string& line)
{
    return true;
}

bool FileSaver::wasReadBoundaryEnd(std::string& line)
{
    //Завершаем текущий файл если он открыт
    closeFileAndResetValues();

    //Сбрасываем
    m_boundary.clear();
    m_boundaryExtended.clear();
    m_boundaryEnd.clear();

    //Переходим в конечное состояние
    setState(FinishedRead);
    return true;
}

bool FileSaver::finishedRead(std::string& line)
{
    //В конечном состоянии не ожидаем больше данных
    setLastError("Extra data after finishing reading");
    return false;
}

bool FileSaver::errorState(std::string& line)
{
    setLastError("Impossible state");
    return false;
}

bool FileSaver::analyzeLine(std::string& line)
{
    switch(m_state)
    {
        case WaitingRequestHeader:
        {
            return waitingRequestHeader(line);
        }
        case WasReadRequestHeader:
        {
            return wasReadRequestHeader();
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
        case WaitingData:
        {
            return waitingData(line);
        }
        case WasReadData:
        {
            return wasReadData(line);
        }
        case WaitingBoundaryEnd:
        {
            return waitingBoundaryEnd(line);
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
            setLastError("Unknown state: " + std::to_string(m_state));
            return false;
    }
}

bool FileSaver::readLineFromBuffer(std::istream& stream, std::string& line)
{
    line.clear();

    if(std::getline(stream, line))
    {
        //Добавляем символ новой строки, так как он был удалён при getline
        line.push_back('\n');

        return true;
    }

    return false;
}

bool FileSaver::writeLineToFile(std::string& line)
{
    if(!m_file.is_open())
    {
        setLastError("File" + m_dir + "/" + m_filename + " not open");
        return false;
    }

    //Записываем данные с переводом строки
    m_file << line;
    m_fileSize += line.size();

    return true;
}

void FileSaver::closeFileAndResetValues()
{
    if(m_file.is_open())
    {
        m_file.close();

        json descriptionFile = {
                                   {"filename", m_filename},
                                   {"size", m_fileSize}
                               };

        addFileToDescriptionUploadedFiles(descriptionFile);

        WRITE_TO_LOGGER("Was saved file: " + m_filename + ", size: " + std::to_string(m_fileSize));

        //Сбрасываем для следующего файла
        m_newline.clear();
        m_filename.clear();
        m_fileSize = 0;
    }
}

FileSaver::TypeLine FileSaver::getLineType(std::string& line)
{
    //Проверка на NewLine (пустая строка)
    if(line.size() == 2)
    {
        if(line == "\r\n")
        {
            return NewLine;
        }
    }

    //Проверка на Boundary (начинается с "-")
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

std::string FileSaver::extractFilenameFromContentDisposition(std::string& line)
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
