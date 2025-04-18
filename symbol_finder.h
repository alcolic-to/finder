#include <filesystem>
#include <fstream>
#include <iostream>
#include <ranges>
#include <sstream>
#include <stdexcept>
#include <string>
#include <type_traits>
#include <utility>
#include <vector>

#include "files.h"
#include "suffix_trie.h"
#include "symbols.h"

#ifndef SYMBOL_FINDER_H
#define SYMBOL_FINDER_H

using namespace std::chrono;
using namespace std::chrono_literals;
using Clock = steady_clock;
using Time_point = std::chrono::time_point<Clock>;

inline Time_point now() noexcept
{
    return Clock::now();
}

template<bool print = true, typename Unit = milliseconds>
class Stopwatch {
public:
    explicit Stopwatch(std::string name = "Stopwatch") noexcept
        : m_name{std::move(name)}
        , m_start{now()}
    {
    }

    ~Stopwatch() noexcept
    {
        if constexpr (print) {
            auto out = elapsed_units().count();
            std::cout << m_name << " elapsed time: " << out << " " << unit_name() << "\n";
        }
    }

    Stopwatch(const Stopwatch& rhs) = delete;
    Stopwatch& operator=(const Stopwatch& rhs) = delete;
    Stopwatch(Stopwatch&& rhs) noexcept = delete;
    Stopwatch& operator=(Stopwatch&& rhs) = delete;

    void restart() noexcept { m_start = now(); }

    [[nodiscard]] auto elapsed() const noexcept { return now() - m_start; }

    [[nodiscard]] auto elapsed_units() const noexcept { return duration_cast<Unit>(elapsed()); }

    [[nodiscard]] std::string unit_name() const noexcept
    {
        // clang-format off
        if      constexpr (std::is_same_v<Unit, hours>)        return "hour(s)";
        else if constexpr (std::is_same_v<Unit, minutes>)      return "minute(s)";
        else if constexpr (std::is_same_v<Unit, seconds>)      return "second(s)";
        else if constexpr (std::is_same_v<Unit, milliseconds>) return "millisecond(s)";
        else if constexpr (std::is_same_v<Unit, microseconds>) return "microsecond(s)";
        else if constexpr (std::is_same_v<Unit, nanoseconds>)  return "nanosecond(s)";
        else                                                   return "unknown unit";
        // clang-format on
    }

private:
    std::string m_name;
    Clock::time_point m_start;
};

const std::vector<std::string> cpp_keywords = {
    // Keywords
    "alignas", "alignof", "and", "and_eq", "asm", "auto", "bitand", "bitor", "bool", "break",
    "case", "catch", "char", "char8_t", "char16_t", "char32_t", "class", "compl", "concept",
    "const", "consteval", "constexpr", "constinit", "const_cast", "continue", "co_await",
    "co_return", "co_yield", "decltype", "default", "delete", "do", "double", "dynamic_cast",
    "else", "enum", "explicit", "export", "extern", "false", "float", "for", "friend", "goto", "if",
    "inline", "int", "long", "mutable", "namespace", "new", "noexcept", "not", "not_eq", "nullptr",
    "operator", "or", "or_eq", "private", "protected", "public", "register", "reinterpret_cast",
    "requires", "return", "short", "signed", "sizeof", "static", "static_assert", "static_cast",
    "struct", "switch", "template", "this", "thread_local", "throw", "true", "try", "typedef",
    "typeid", "typename", "union", "unsigned", "using", "virtual", "void", "volatile", "wchar_t",
    "while", "xor", "xor_eq",

    // Operators
    "+", "-", "*", "/", "%", "++", "--", "=",
    "+=", "-=", "*=", "/=", "%=", "&=", "|=", "^=", "<<=", ">>=", "==", "!=", "<", ">",
    "<=", ">=", "<=>", "!", "&&", "||", "~", "&", "|", "^", "<<", ">>", ".", "->", ".*", "->*",
    "[]", "()", "?:",

    // Punctuators / Syntax
    "{", "}", "[", "]", "(", ")", ";", ",", "::", ".", "->", ":", "...", "#", "##", "=>",

    // Preprocessor
    "#define", "#undef", "#include", "#ifdef", "#ifndef", "#if", "#else", "#elif", "#endif",
    "#error", "#pragma", "#line",

    // Digraphs
    "<%", "%>", "<:", ":>", "%:", "%:%:",

    // Trigraphs
    // "??=", "??(", "??)", "??<", "??>", "??/", "??'", "??!", "??-"
};

static constexpr bool supported_file(const fs::path& file_path)
{
    std::string ext{file_path.extension().string()};
    return ext == ".cpp" || ext == ".c" || ext == ".hpp" || ext == ".h";
}

static constexpr bool is_keyword(const std::string& s)
{
    return std::ranges::find(cpp_keywords, s) != cpp_keywords.end();
}

static bool check_iteration(const auto& it, std::error_code& ec)
{
    try {
        if (ec) {
            std::cerr << "Error accessing " << it->path() << ": " << ec.message() << '\n';
            ec.clear();
            return false;
        }

        // Try converting path to string to see if exception will occur.
        // TODO: use std::wstring which should always succeed.
        [[maybe_unused]] const std::string& p = it->path().string();
    }
    catch (...) {
        std::cout << "Path to string conversion failed.\n";
        return false;
    }

    return true;
}

class Options {
public:
    static constexpr uint32_t files = 1;
    static constexpr uint32_t symbols = 2;
    static constexpr uint32_t all = files | symbols;

    Options() : m_opt{all} {}

    explicit Options(uint32_t opt) : m_opt{opt}
    {
        if ((m_opt & ~all) != 0U)
            throw std::runtime_error{"Invalid options."};

        if (m_opt == 0U)
            m_opt = files;
    }

    [[nodiscard]] bool files_allowed() const noexcept { return (m_opt & files) != 0U; }

    [[nodiscard]] bool symbols_allowed() const noexcept { return (m_opt & symbols) != 0U; }

private:
    uint32_t m_opt;
};

class Symbol_finder {
public:
    explicit Symbol_finder(std::string dir, Options opt = Options{})
        : m_dir{std::move(dir)}
        , m_options{opt}
    {
        using dir_iter = fs::recursive_directory_iterator;
        constexpr auto it_opt = fs::directory_options::skip_permission_denied;

        std::error_code ec;
        for (dir_iter it(m_dir, it_opt, ec); it != dir_iter(); it.increment(ec)) {
            if (!check_iteration(it, ec) || !files_allowed())
                continue;

            File_info* file = m_files.insert(it->path()).get();

            if (!symbols_allowed() || !it->is_regular_file() || !supported_file(*it))
                continue;

            // TODO: Use file_to_string for quick file read.
            //
            std::ifstream ifs{it->path()};
            if (!ifs.is_open()) {
                std::cout << "Problem with openning file " << it->path() << ". Skipping.\n ";
                continue;
            }

            // Parse each line from file and save tokens that are not keywords.
            //
            size_t line_num = 1;
            for (std::string fline, token; std::getline(ifs, fline); ++line_num) {
                std::stringstream ss{fline};
                while (ss >> token) {
                    if (is_keyword(token))
                        continue;

                    // TODO: Save file line too.
                    m_symbols.insert(std::move(token), file, line_num);
                }
            }
        }
    }

    [[nodiscard]] Symbols& symbols() noexcept { return m_symbols; }

    [[nodiscard]] Files& files() noexcept { return m_files; }

    [[nodiscard]] const std::string& dir() const noexcept { return m_dir; }

    [[nodiscard]] bool files_allowed() const noexcept { return m_options.files_allowed(); }

    [[nodiscard]] bool symbols_allowed() const noexcept { return m_options.symbols_allowed(); }

    void find_files(const std::string& regex)
    {
        auto files = [&] {
            Stopwatch<true, microseconds> s{"File search"};
            return m_files.search(regex);
        }();

        for (const auto& file : files)
            std::cout << std::format("{}\\{}\n", file->path(), file->name());
    }

    void find_symbols(const std::string& symbol_name)
    {
        auto* symbol = [&] {
            Stopwatch<true, microseconds> s{"Symbol search"};
            return m_symbols.search(symbol_name);
        }();

        if (symbol == nullptr) {
            std::cout << "Symbol not found.\n";
            return;
        }

        const auto& files = symbol->refs();
        for (const auto& file : files)
            for (size_t line : file.m_lines)
                std::cout << std::format("{}\\{} Line: {}\n", file.m_file->path(),
                                         file.m_file->name(), line);
    }

    void print_memory_usage()
    {
        static constexpr size_t MB = 1024ULL * 1024;

        std::cout << "\nFiles memory usage:\n";
        std::cout << std::format("Files size:         {}MB\n", m_files.files_size() / MB);
        std::cout << std::format("Files paths size:   {}MB\n", m_files.file_paths_size() / MB);
        std::cout << std::format("File finder size:   {}MB\n", m_files.file_finder_size() / MB);

        std::cout << "\nSymbols memory usage:\n";
        std::cout << std::format("Symbols size:       {}MB\n", m_symbols.symbols_size() / MB);
        std::cout << std::format("Symbol finder size: {}MB\n", m_symbols.symbol_finder_size() / MB);
    }

private:
    std::string m_dir;
    Files m_files;
    Symbols m_symbols;
    Options m_options;
};

#endif
