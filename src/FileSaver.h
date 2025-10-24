#ifndef FILE_SAVER_H
#define FILE_SAVER_H

#include <string>
#include <list>
#include <cstdint>

#include <nlohmann/json.hpp>

#include <fstream>
#include <unordered_map>

#include "utility.hpp"


using json = nlohmann::json;
using namespace SimpleWeb;

class FileSaver
{
public:
    //Состояния FileSaver
    enum FileSaverState : uint8_t
    {
        WaitingRequestHeader,
        WaitingBoundary,
        WasReadBoundary,
        WaitingContentDisposition,
        WasReadContentDisposition,
        WaitingNewLine,
        WasReadNewLine,
        ReadingData,
        WasReadBoundaryEnd,
        FinishedRead,
        Error,
        QuantityParserState     //Количество состояний
    };

    //Типы строк
    enum TypeLine : uint8_t
    {
        Boundary,
        ContentDisposition,
        NewLine,
        Data,
        BoundaryEnd,
        QuantityTypeLine     //Количество типов
    };

    FileSaver();

    void setRequestHeader(const CaseInsensitiveMultimap& headers);
    json processStream(std::istream& stream);

private:
    FileSaverState m_state;
    std::string m_filename;
    std::ofstream m_file;
    std::string m_boundary;
    std::string m_boundaryExtended;
    std::string m_boundaryEnd;

    size_t m_fileSize;
    CaseInsensitiveMultimap m_requestHeadersMap;

    std::string lastError;

    json descriptionUploadedFiles;
    void addFileToDescriptionUploadedFiles(json& newDescriptionFile);

    void setState(FileSaverState newState);

    //Таблица переходов состояний
    FileSaverState m_transitionTable[QuantityParserState][QuantityTypeLine] =
    {
          //Boundary       ContentDisposition         NewLine         Data            BoundaryEnd
        { Error,           Error,                     Error,          Error,          Error              }, //WaitingRequestHeader
        { WasReadBoundary, Error,                     Error,          Error,          Error              }, //WaitingBoundary
        { Error,           Error,                     Error,          Error,          Error              }, //WasReadBoundary
        { Error,           WasReadContentDisposition, Error,          Error,          Error              }, //WaitingContentDisposition
        { Error,           Error,                     Error,          Error,          Error              }, //WasReadContentDisposition
        { Error,           Error,                     WasReadNewLine, WaitingNewLine, Error              }, //WaitingNewLine
        { Error,           Error,                     Error,          Error,          Error              }, //WasReadNewLine
        { WasReadBoundary, ReadingData,               ReadingData,    ReadingData,    WasReadBoundaryEnd }, //ReadingData
        { Error,           Error,                     Error,          Error,          Error              }, //WasReadBoundaryEnd
        { Error,           Error,                     Error,          Error,          Error              }, //FinishedRead
        { Error,           Error,                     Error,          Error,          Error              }  //Error
    };

    bool waitingRequestHeader(const std::string& line);
    bool waitingBoundary(const std::string& line);
    bool wasReadBoundary(const std::string& line);
    bool waitingContentDisposition(const std::string& line);
    bool wasReadContentDisposition(const std::string& line);
    bool waitingNewLine(const std::string& line);
    bool wasReadNewLine(const std::string& line);
    bool readingData(const std::string& line);
    bool wasReadBoundaryEnd(const std::string& line);
    bool finishedRead(const std::string& line);
    bool errorState(const std::string& line);

    bool analyzeLine(const std::string& line);

    bool readLineFromBuffer(std::istream& stream, std::string& line);

    TypeLine getLineType(const std::string& line);

    std::string extractNameFromContentType(const std::string& line);
    std::string extractFilenameFromContentDisposition(const std::string& line);
};

#endif //FILE_SAVER_H
