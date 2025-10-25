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
        WasReadRequestHeader,
        WaitingBoundary,
        WasReadBoundary,
        WaitingContentDisposition,
        WasReadContentDisposition,
        WaitingNewLine,
        WasReadNewLine,
        WaitingData,
        WasReadData,
        WaitingBoundaryEnd,
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
    std::string m_newline;

    size_t m_fileSize;
    CaseInsensitiveMultimap m_requestHeadersMap;

    std::string lastError;

    json descriptionUploadedFiles;
    void addFileToDescriptionUploadedFiles(json& newDescriptionFile);

    void setState(FileSaverState newState);

    //Таблица переходов состояний
    FileSaverState m_transitionTable[QuantityParserState][QuantityTypeLine] =
    {
        //Boundary         ContentDisposition         NewLine                      Data            BoundaryEnd
        { ErrorState,      ErrorState,                ErrorState,                  ErrorState,     ErrorState         }, //WaitingRequestHeader
        { ErrorState,      ErrorState,                ErrorState,                  ErrorState,     ErrorState         }, //WasReadRequestHeader
        { WasReadBoundary, ErrorState,                ErrorState,                  ErrorState,     ErrorState         }, //WaitingBoundary
        { ErrorState,      ErrorState,                ErrorState,                  ErrorState,     ErrorState         }, //WasReadBoundary
        { ErrorState,      WasReadContentDisposition, ErrorState,                  ErrorState,     ErrorState         }, //WaitingContentDisposition
        { ErrorState,      ErrorState,                ErrorState,                  ErrorState,     ErrorState         }, //WasReadContentDisposition
        { ErrorState,      ErrorState,                WasReadNewLine,              WaitingNewLine, ErrorState         }, //WaitingNewLine
        { ErrorState,      ErrorState,                ErrorState,                  ErrorState,     ErrorState         }, //WasReadNewLine
        { WasReadBoundary, WasReadData,               WasReadData,                 WasReadData,    WasReadBoundaryEnd }, //WaitingData
        { ErrorState,      ErrorState,                ErrorState,                  ErrorState,     ErrorState         }, //WasReadData
        { WasReadBoundary, WasReadData,               WasReadData,                 WasReadData,    WasReadBoundaryEnd }, //WaitingBoundaryEnd
        { ErrorState,      ErrorState,                ErrorState,                  ErrorState,     ErrorState         }, //WasReadBoundaryEnd
        { ErrorState,      ErrorState,                ErrorState,                  ErrorState,     ErrorState         }, //FinishedRead
        { ErrorState,      ErrorState,                ErrorState,                  ErrorState,     ErrorState         }  //ErrorState
    };

    bool waitingRequestHeader(std::string& line);
    bool wasReadRequestHeader();
    bool waitingBoundary(std::string& line);
    bool wasReadBoundary(std::string& line);
    bool waitingContentDisposition(std::string& line);
    bool wasReadContentDisposition(std::string& line);
    bool waitingNewLine(std::string& line);
    bool wasReadNewLine(std::string& line);
    bool waitingData(std::string& line);
    bool wasReadData(std::string& line);
    bool waitingBoundaryEnd(std::string& line);
    bool wasReadBoundaryEnd(std::string& line);
    bool finishedRead(std::string& line);
    bool errorState(std::string& line);

    bool analyzeLine(std::string& line);

    bool readLineFromBuffer(std::istream& stream, std::string& line);

    bool writeLineToFile(std::string& line);

    TypeLine getLineType(std::string& line);

    std::string extractNameFromContentType(std::string& line);
    std::string extractFilenameFromContentDisposition(std::string& line);
};

#endif //FILE_SAVER_H
