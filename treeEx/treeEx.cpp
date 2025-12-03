#include <iostream>
#include <filesystem>
#include <string>
#include <vector>
#include <algorithm>
#include <sstream>
#include <fstream>

#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>

#include <shlwapi.h>
#pragma comment(lib, "Shlwapi.lib")

namespace fs = std::filesystem;

using namespace std::literals::string_literals;

#pragma region FunctionDeclaration
/// <summary>
/// 指定したパスのディレクトリ構造を再帰的に整形して出力ストリームに書き込む関数。各階層のインデントや罫線を考慮してツリー表示を行う。
/// </summary>
/// <param name="out">出力先のストリーム。ツリー表示やエラーメッセージはこのストリームに書き込まれる。</param>
/// <param name="path">ツリー表示の対象となるディレクトリのパス。</param>
/// <param name="isLastList">各親階層について、その階層で現在の要素が最後かどうかを示す BOOL の配列。インデントと罫線の描画（縦線を継続するか空白にするか等）に使用される。</param>
void printDirectoryTree(std::ostream& out, const fs::path& path, std::vector<bool> isLastList);

/// <summary>
/// 指定した出力ストリームに1行を出力します。
/// </summary>
/// <param name="out">出力先の std::ostream への参照。出力先のストリームに行を書き込みます。</param>
/// <param name="line">出力する行を表す std::wstring。</param>
void printLine(std::ostream& out, const std::wstring& line);

/// <summary>
/// 指定された出力ストリームに行を書き込み、行末に改行を追加します。
/// </summary>
/// <param name="out">書き込み先の出力ストリーム（std::ostream の参照）。</param>
/// <param name="line">書き込むワイド文字列（const std::wstring の参照）。</param>
void printLineWithNewLine(std::ostream& out, const std::wstring& line);

/// <summary>
/// ディレクトリエントリを比較する関数。
/// ファイルをディレクトリより先に並べ、同種（両方ファイルまたは両方ディレクトリ）の場合は 
/// StrCmpLogicalW による自然順で名前を比較する。
/// </summary>
/// <param name="a">比較対象の最初の fs::directory_entry。
/// 比較時にはエントリの種類（ディレクトリかどうか）とファイル名が使用される。</param>
/// <param name="b">比較対象の2番目の fs::directory_entry。</param>
/// <returns>a が b より前に来る場合に true を返す。
/// ディレクトリとファイルが混在する場合はファイルが先に、
/// 同種の場合は StrCmpLogicalW による自然順での比較結果に従う。</returns>
bool compareEntriesW(const fs::directory_entry& a, const fs::directory_entry& b);

/// <summary>
/// std::wstring を （UTF-8エンコードされた）std::string に変換します。
/// </summary>
/// <param name="wideString">変換対象のワイド文字列（const std::wstring&）。</param>
/// <returns>UTF-8エンコードされた std::string を返します。</returns>
std::string wstringToUtf8String(const std::wstring& wideString);

/// <summary>
/// ANSI コードページ (CP_ACP) の std::string を std::wstring に変換します。
/// </summary>
/// <param name="nString">変換する入力の std::string（const 参照）。</param>
/// <returns>変換された std::wstring を返します。</returns>
std::wstring acpStringToWstring(const std::string& nString);

std::wstring formatMessageW(const DWORD errorCode, const BOOL messageLangJapanese = FALSE);
#pragma endregion

/// <summary>
/// コマンドライン引数で指定した開始ディレクトリのツリーを標準出力または指定ファイルへ出力する。
/// -o または /o オプションで出力ファイル（UTF-8 with BOM）を指定できる。
/// 通常の tree コマンドでは、
/// 1.ファイルにリダイレクトした場合に、ユニコード絵文字が文字化けする不具合にも対応している。
/// 2.隠し属性のファイル、ディレクトリの出力にも対応している。
/// </summary>
/// <param name="argc">コマンドライン引数の個数。</param>
/// <param name="argv">ワイド文字列（wchar_t*）のコマンドライン引数配列。
/// オプションスイッチなしで、実行パスの後に開始ディレクトリを指定する。
/// （省略した場合は、カレントディレクトリ（.））
/// 開始ディレクトリの末尾は、L'\\' であってはならない。
/// -o / -O / /o / /O のいずれかのオプションを指定して、出力ファイルパスを指定する。
/// 指定されたパスが存在する場合、上書きを行う。</param>
/// <returns>正常終了時は EXIT_SUCCESS を返す。指定パスが存在しない、指定パスがディレクトリでない、出力ファイルを開けない等のエラー時は EXIT_FAILURE を返す。</returns>
int wmain(int argc, wchar_t* argv[])
{
    SetConsoleOutputCP(CP_UTF8);

    fs::path startPath{ L"." };
    std::wstring outputFilePath;

    // コマンドライン引数を解析
    for (auto i = 1; i < argc; ++i)
    {
        std::wstring arg = argv[i];
        if ((arg == L"-o" || arg == L"-O"
            || arg == L"/o" || arg == L"/O")
            && i + 1 < argc)
            outputFilePath = argv[++i];
        else
            startPath = arg;
    }

    // ---------- Debug path ----------
    //startPath = LR"(..\TestData)";
    //outputFilePath = LR"(..\FileList.txt)";
    // ------------------------------

    if (!fs::exists(startPath))
    {
        std::wcerr << L"Error : File not found. [" << startPath << L"]" << std::endl;
        return EXIT_FAILURE;
    }
    else if (!fs::is_directory(startPath))
    {
        std::wcerr << L"Error : The specified path is not a directory. [" << startPath << L"]" << std::endl;
        return EXIT_FAILURE;
    }

    if (outputFilePath.empty())
    {
        // ファイル出力が指定されていない場合: 標準出力へ
        printLineWithNewLine(std::cout, startPath.wstring());
        printDirectoryTree(std::cout, startPath, {});
    }
    else
    {
        // ファイル出力が指定されている場合: ファイルへ
        std::ofstream outFile(outputFilePath, std::ios_base::out | std::ios_base::trunc | std::ios_base::binary);
        if (!outFile)
        {
            auto result = GetLastError();
            auto message = formatMessageW(result);

            std::wcerr << L"Error : The file cannot be opened.[" << outputFilePath << L"]\n"
                << message << std::endl;

            return EXIT_FAILURE;
        }

        // BOM
        outFile << "\xEF\xBB\xBF";

        printLineWithNewLine(outFile, startPath.wstring());
        printDirectoryTree(outFile, startPath, {});

        std::wcout << L"The results have been output to \n    "s << outputFilePath << L"."s << std::endl;
    }

    return EXIT_SUCCESS;
}

void printDirectoryTree(std::ostream& out, const fs::path& path, std::vector<bool> isLastList)
{
    std::vector<fs::directory_entry> entries;
    try
    {
        for (const auto& entry : fs::directory_iterator(path))
        {
            entries.push_back(entry);
        }
    }
    catch (const fs::filesystem_error& e)
    {
        std::wstring message = L"└─[Access error : "s + path.filename().wstring() + L"]\n"s;
        message += L"        "s + acpStringToWstring(e.what()) + L"\n"s;
        printLine(out, message);
        return;
    }

    std::sort(entries.begin(), entries.end(), compareEntriesW);

    auto buildPrefix = [&](const std::vector<bool>& lastList)
        {
            std::wstringstream ss;
            for (auto isLast : lastList)
            {
                ss << (isLast ? L"    "s : L"│  "s);
            }
            return ss.str();
        };

    std::wstring basePrefix = buildPrefix(isLastList);

    std::vector<fs::directory_entry> files;
    std::vector<fs::directory_entry> directories;
    for (const auto& entry : entries)
    {
        if (fs::is_directory(entry.status()))
        {
            directories.push_back(entry);
        }
        else
        {
            files.push_back(entry);
        }
    }

    // ファイルを出力
    std::wstring filePrefix = basePrefix + (directories.empty() ? L"  "s : L"│  "s);
    for (const auto& file : files)
    {
        printLineWithNewLine(out, filePrefix + file.path().filename().wstring());
    }

    // ディレクトリを再帰的に処理
    for (size_t i = 0; i < directories.size(); ++i)
    {
        const auto& dir = directories[i];
        const bool isLastDir = (i == directories.size() - 1);

        std::wstringstream line;
        line << basePrefix << (isLastDir ? L"└─"s : L"├─"s) << dir.path().filename().wstring();
        printLineWithNewLine(out, line.str());

        std::vector<bool> nextIsLastList = isLastList;
        nextIsLastList.push_back(isLastDir);
        printDirectoryTree(out, dir.path(), nextIsLastList);
    }
}

void printLine(std::ostream& out, const std::wstring& line)
{
    auto lineU8 = wstringToUtf8String(line);

    out << lineU8;
}

void printLineWithNewLine(std::ostream& out, const std::wstring& line)
{
    printLine(out, line + L"\n");
}

bool compareEntriesW(const fs::directory_entry& a, const fs::directory_entry& b)
{
    bool aIsDir = a.is_directory();
    bool bIsDir = b.is_directory();

    if (aIsDir != bIsDir)
        // aがディレクトリでbがファイルなら、bが前 (false)
        // aがファイルでbがディレクトリなら、aが前 (true)
        return bIsDir;

    return StrCmpLogicalW(a.path().filename().c_str(), b.path().filename().c_str()) < 0;
}

std::string wstringToUtf8String(const std::wstring& wideString)
{
    if (wideString.empty())
    {
        return std::string();
    }

    // Determine the required buffer size for the UTF-8 string (excluding null terminator).
    // The WideCharToMultiByte function returns the number of bytes required for the buffer.
    // For stricter errorCode checking (e.g., to fail on invalid UTF-16 sequences),
    // you could pass the WC_ERR_INVALID_CHARS flag.
    int sizeNeeded = WideCharToMultiByte(
        // Code page: CP_OEMCP
        //CP_OEMCP,
        CP_UTF8,
        // Flags: 0 for default behavior.
        // WC_ERR_INVALID_CHARS : This flag only applies when CodePage is specified as CP_UTF8 or 54936.
        0,
        // Pointer to the wide-character string.
        wideString.data(),
        // Number of characters in the string. -1 if null-terminated.
        static_cast<int>(wideString.size()),
        // Buffer for new string: nullptr to get size requirement.
        nullptr,
        // Size of buffer: 0 to get size requirement.
        0,
        // Pointer to default character (not used with CP_UTF8).
        nullptr,
        // Pointer to flag indicating default char used (not used with CP_UTF8).
        nullptr
    );

    if (sizeNeeded == 0)
    {
        DWORD errorCode = GetLastError();
        throw std::system_error(errorCode, std::system_category(), "WideCharToMultiByte failed to calculate size for wstringToString. Error code: "s + std::to_string(errorCode));
    }

    // Initialize with char
    std::string convertedString(sizeNeeded, '\0');

    // Perform the actual conversion.
    int convertedLength = WideCharToMultiByte(
        //CP_OEMCP,
        CP_UTF8,
        0,
        wideString.data(),
        static_cast<int>(wideString.size()),
        reinterpret_cast<LPSTR>(convertedString.data()),
        sizeNeeded,
        nullptr,
        nullptr
    );

    if (convertedLength == 0)
    {
        DWORD errorCode = GetLastError();
        throw std::system_error(errorCode, std::system_category(), "WideCharToMultiByte failed to convert in wstringToString. Error code: "s + std::to_string(errorCode));
    }

    return convertedString;
}

std::wstring acpStringToWstring(const std::string& nString)
{
    if (nString.empty())
    {
        return std::wstring();
    }

    int sizeNeeded = MultiByteToWideChar(
        CP_ACP,
        0,
        nString.data(),
        static_cast<int>(nString.size()),
        nullptr,
        0
    );

    if (sizeNeeded == 0)
    {
        DWORD errorCode = GetLastError();
        throw std::system_error(errorCode, std::system_category(), "MultiByteToWideChar failed to calculate size for stringToWstring. Error code: "s + std::to_string(errorCode));
    }

    std::wstring wideString(sizeNeeded, L'\0');

    int convertedLength = MultiByteToWideChar(
        CP_ACP,
        0,
        nString.data(),
        static_cast<int>(nString.size()),
        wideString.data(),
        sizeNeeded
    );

    if (convertedLength == 0)
    {
        DWORD errorCode = GetLastError();
        throw std::system_error(errorCode, std::system_category(), "MultiByteToWideChar failed to convert in stringToWstring. Error code: "s + std::to_string(errorCode));
    }

    return wideString;
}

std::wstring formatMessageW(const DWORD errorCode, const BOOL messageLangJapanese)
{
    LPWSTR pBuffer{ nullptr };

    auto needBufferSize = FormatMessageW(
        FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
        nullptr,
        errorCode,
        messageLangJapanese ? MAKELANGID(LANG_JAPANESE, SUBLANG_JAPANESE_JAPAN) : MAKELANGID(LANG_ENGLISH, SUBLANG_ENGLISH_US),
        reinterpret_cast<LPWSTR>(&pBuffer),
        0,
        nullptr
    );

    std::wstring message;
    if (needBufferSize > 0)
    {
        message = pBuffer;
        if (message.ends_with(L"\r\n"))
            message.resize(message.size() - 2);
    }
    else
    {
        message = L"Unknown error code.";
    }

    // Free the allocated buffer
    if (pBuffer)
    {
        LocalFree(pBuffer);
    }

    return message;
}
