#ifndef FILE_SAVER_H
#define FILE_SAVER_H

#include <string>
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
        ReadingRequestHeader,
        WaitingBoundaryHeader,
        ReadingBoundaryHeader,
        WaitingData,
        ReadingData,
        FinishedRead,
        ErrorState,
        QuantityParserState     //Количество состояний
    };

    //Типы строк
    enum TypeLine : uint8_t
    {
        Boundary,
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
    size_t m_fileSize;
    CaseInsensitiveMultimap m_requestHeadersMap;

    //Таблица переходов состояний
    FileSaverState m_transitionTable[QuantityParserState][QuantityTypeLine] =
    {
        //Boundary           NewLine                    Data                      BoundaryEnd
        { ReadingBoundaryHeader, WaitingRequestHeader,   ReadingRequestHeader,     ReadingBoundaryHeader }, //WaitingRequestHeader
        { ReadingBoundaryHeader, ReadingRequestHeader,   ReadingRequestHeader,     ReadingBoundaryHeader }, //ReadingRequestHeader
        { ReadingBoundaryHeader, WaitingBoundaryHeader,  ReadingBoundaryHeader,    ReadingBoundaryHeader }, //WaitingBoundaryHeader
        { ReadingBoundaryHeader, WaitingData,            ReadingBoundaryHeader,    ReadingBoundaryHeader }, //ReadingBoundaryHeader
        { ReadingBoundaryHeader, WaitingData,            ReadingData,              ReadingBoundaryHeader }, //WaitingData
        { ReadingBoundaryHeader, ReadingData,            ReadingData,              FinishedRead },         //ReadingData
        { FinishedRead,         FinishedRead,           FinishedRead,             FinishedRead },         //FinishedRead
        { ErrorState,           ErrorState,             ErrorState,               ErrorState }            //ErrorState
    };

    bool readLineFromBuffer(std::istream& stream, std::string& line);

    TypeLine checkLineType(const std::string& line);

    bool isRequestHeaderStart(const std::string& line);

    void extractFilename(const std::string& line);

    void analyzeBoundaryHeader(const std::string& line);
};

#endif //FILE_SAVER_H