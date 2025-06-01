#ifndef SYMBOL_FINDER_H
#define SYMBOL_FINDER_H

#include <filesystem>
#include <fstream>
#include <iostream>
#include <ranges>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

#include "files.h"
#include "small_string.h"
#include "symbols.h"
#include "tokens.h"
#include "util.h"

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
    explicit Symbol_finder(const std::string& dir, Options opt = Options{})
        : m_dir{dir}
        , m_options{opt}
    {
        using dir_iter = fs::recursive_directory_iterator;
        constexpr auto it_opt = fs::directory_options::skip_permission_denied;

        std::error_code ec;
        for (dir_iter it{dir, it_opt, ec}; it != dir_iter{}; it.increment(ec)) {
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

            // Parse each line from file and save tokens.
            //
            NECTR_Tokenizer tokenizer;
            Token token;

            size_t line_num = 1;
            for (std::string fline; std::getline(ifs, fline); ++line_num) {
                tokenizer = fline;
                while (tokenizer >> token) {
                    if (token.type() != Token_t::word || is_keyword(token.str()))
                        continue;

                    m_symbols.insert(token.str(), file, line_num, fline);
                }
            }
        }

        print_stats();
    }

    [[nodiscard]] Symbols& symbols() noexcept { return m_symbols; }

    [[nodiscard]] Files& files() noexcept { return m_files; }

    [[nodiscard]] const char* dir() const noexcept { return m_dir; }

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
            for (const auto& line : file.lines())
                std::cout << std::format("{}\\{} {}: {}\n", file.file()->path(),
                                         file.file()->name(), line.line(),
                                         std::string(line.preview()));
    }

    void print_stats()
    {
        m_files.print_stats();

        if (symbols_allowed())
            m_symbols.print_stats();
    }

private:
    Small_string m_dir;
    Files m_files;
    Symbols m_symbols;
    Options m_options;
};

#endif
