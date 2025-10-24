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
        ErrorState,
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
        //Boundary         ContentDisposition         NewLine         Data            BoundaryEnd
        { ErrorState,      ErrorState,                ErrorState,     ErrorState,     ErrorState         }, //WaitingRequestHeader
        { WasReadBoundary, ErrorState,                ErrorState,     ErrorState,     ErrorState         }, //WaitingBoundary
        { ErrorState,      ErrorState,                ErrorState,     ErrorState,     ErrorState         }, //WasReadBoundary
        { ErrorState,      WasReadContentDisposition, ErrorState,     ErrorState,     ErrorState         }, //WaitingContentDisposition
        { ErrorState,      ErrorState,                ErrorState,     ErrorState,     ErrorState         }, //WasReadContentDisposition
        { ErrorState,      ErrorState,                WasReadNewLine, WaitingNewLine, ErrorState         }, //WaitingNewLine
        { ErrorState,      ErrorState,                ErrorState,     ErrorState,     ErrorState         }, //WasReadNewLine
        { WasReadBoundary, ReadingData,               ReadingData,    ReadingData,    WasReadBoundaryEnd }, //ReadingData
        { ErrorState,      ErrorState,                ErrorState,     ErrorState,     ErrorState         }, //WasReadBoundaryEnd
        { ErrorState,      ErrorState,                ErrorState,     ErrorState,     ErrorState         }, //FinishedRead
        { ErrorState,      ErrorState,                ErrorState,     ErrorState,     ErrorState         }  //ErrorState
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
