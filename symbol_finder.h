#include <cctype>
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

static constexpr bool is_keyword(const std::string& s)
{
    return std::ranges::find(cpp_keywords, s) != cpp_keywords.end();
}

enum class Token_t : uint8_t {
    invalid = 0,
    prep,
    comment,
    number,
    char_lit,
    str_lit,
    non_word,
    word
};

class Token {
public:
    std::string& str() noexcept { return m_str; }

    const std::string& str() const noexcept { return m_str; }

    Token_t& type() noexcept { return m_type; }

    const Token_t& type() const noexcept { return m_type; }

    size_t& pos() noexcept { return m_pos; }

    const size_t& pos() const noexcept { return m_pos; }

    void reset() noexcept
    {
        m_str.clear();
        m_type = Token_t::invalid;
        m_pos = 0;
    }

private:
    std::string m_str;
    Token_t m_type = Token_t::invalid;
    size_t m_pos = 0;
};

// NOLINTBEGIN

// Not even close to real tokenizer, but it returns some kind of tokens.
//
class NECTR_Tokenizer {
public:
    NECTR_Tokenizer() = default;

    explicit NECTR_Tokenizer(std::string& s) : m_src{s.data()}, c{s.data()} {}

    NECTR_Tokenizer& operator=(const std::string& s)
    {
        m_src = c = s.data();
        return *this;
    }

    // clang-format off
    //
    bool operator>>(Token& t)
    {
        t.reset();

        if (!skip_spaces())
            return false;

        t.pos() = pos();

        if (ext_preprocessor(t))   return true;
        if (ext_comment(t))        return true;
        if (ext_number(t))         return true;
        if (ext_string_literal(t)) return true;
        if (ext_char_literal(t))   return true;
        if (ext_non_word(t))       return true;
        if (ext_word(t))           return true;

        assert(!"Unreachable!");
        return false;
    }
    //
    // clang-format on

private:
    [[nodiscard]] size_t pos() const noexcept { return c - m_src; }

    bool skip_spaces()
    {
        while (*c && std::isspace(*c))
            ++c;

        return *c;
    }

    static constexpr bool valid_word_ch(char ch) noexcept
    {
        return std::isalnum(ch) || ch == '$' || ch == '_';
    }

    bool ext_non_word(Token& t)
    {
        if (!*c || std::isspace(*c) || valid_word_ch(*c))
            return false;

        while (*c && !valid_word_ch(*c) && !std::isspace(*c))
            t.str() += *c++;

        t.type() = Token_t::non_word;
        return true;
    }

    bool ext_word(Token& t)
    {
        if (!*c || std::isspace(*c) || !valid_word_ch(*c))
            return false;

        while (valid_word_ch(*c))
            t.str() += *c++;

        t.type() = Token_t::word;
        return true;
    }

    bool ext_single_comment(Token& t)
    {
        if (*c != '/' || *(c + 1) != '/')
            return false;

        while (*c)
            t.str() += *c++;

        t.type() = Token_t::comment;
        return true;
    }

    bool ext_multi_comment(Token& t)
    {
        if (*c != '/' || *(c + 1) != '*')
            return false;

        while (*c && !(*c == '*' && *(c + 1) == '/'))
            t.str() += *c++;

        if (*c)
            t.str() += *c++;

        if (*c)
            t.str() += *c++;

        t.type() = Token_t::comment;
        return true;
    }

    bool ext_comment(Token& t) { return ext_single_comment(t) || ext_multi_comment(t); }

    bool ext_number(Token& t)
    {
        if (!std::isdigit(*c))
            return false;

        while (std::isdigit(*c))
            t.str() += *c++;

        t.type() = Token_t::number;
        return true;
    }

    bool ext_char_literal(Token& t)
    {
        if (*c != '\'')
            return false;

        t.str() += *c++;
        while (*c && *c != '\'')
            t.str() += *c++;

        if (*c == '\'')
            t.str() += *c++;

        t.type() = Token_t::char_lit;
        return true;
    }

    bool ext_string_literal(Token& t)
    {
        if (*c != '"')
            return false;

        t.str() += *c++;
        while (*c && *c != '"')
            t.str() += *c++;

        if (*c == '"')
            t.str() += *c++;

        t.type() = Token_t::str_lit;
        return true;
    }

    bool ext_preprocessor(Token& t)
    {
        if (*c != '#')
            return false;

        t.str() += *c++;
        while (std::isalnum(*c))
            t.str() += *c++;

        t.type() = Token_t::prep;
        return true;
    }

    const char* m_src = nullptr;
    const char* c = nullptr;
};

// NOLINTEND

static constexpr bool supported_file(const auto& dir_entry)
{
    if (!dir_entry->is_regular_file())
        return false;

    std::string ext{dir_entry->path().extension().string()};
    return ext == ".cpp" || ext == ".c" || ext == ".hpp" || ext == ".h";
}

static constexpr bool check_iteration(const auto& it, std::error_code& ec)
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

        // Skip all windows files in order to save space.
        // if (p.starts_with("C:\\Windows\\"))
        // return false;
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
            if (!files_allowed() || !check_iteration(it, ec))
                continue;

            File_info* file = m_files.insert(it->path()).get();

            if (!symbols_allowed() || !supported_file(it))
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
            NECTR_Tokenizer tokenizer;
            Token token;

            size_t line_num = 1;
            for (std::string fline; std::getline(ifs, fline); ++line_num) {
                tokenizer = fline;
                while (tokenizer >> token) {
                    if (token.type() != Token_t::word || is_keyword(token.str()))
                        continue;

                    // TODO: Save file line too.
                    m_symbols.insert(std::move(token.str()), file, line_num);
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
        std::cout << std::format("Files paths leaves: {}\n", m_files.file_paths_leaves_count());
        std::cout << std::format("File finder size: F {}MB\n", m_files.file_finder_size() / MB);
        std::cout << std::format("File finder size:   {}MB\n",
                                 m_files.file_finder_size(false) / MB);
        std::cout << std::format("Files finder leaves:{}\n", m_files.file_finder_leaves_count());

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
