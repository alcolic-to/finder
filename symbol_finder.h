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
    using dir_iter = fs::recursive_directory_iterator;
    static constexpr size_t files_search_limit = 128;

    explicit Symbol_finder(const std::string& dir, Options opt = Options{})
        : m_dir{dir}
        , m_options{opt}
    {
        constexpr auto it_opt = fs::directory_options::skip_permission_denied;

        std::error_code ec;
        for (dir_iter it{dir, it_opt, ec}; it != dir_iter{}; it.increment(ec)) {
            if (!files_allowed() || !check_iteration(it, ec))
                continue;

            if (it.depth() < 1)
                std::cout << std::format("Scanning {}\n", it->path().string());

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

    auto find_files(const std::string& regex) { return m_files.search(regex, files_search_limit); }

    Symbol* find_symbols(const std::string& symbol_name) { return m_symbols.search(symbol_name); }

private:
    void print_stats()
    {
        m_files.print_stats();

        if (symbols_allowed())
            m_symbols.print_stats();
    }

    static constexpr bool supported_file(const auto& dir_entry)
    {
        std::string ext{dir_entry->path().extension().string()};
        return ext == ".cpp" || ext == ".c" || ext == ".hpp" || ext == ".h";
    }

    static constexpr bool check_iteration(dir_iter& it, std::error_code& ec)
    {
        try {
            if (ec) {
                std::cerr << std::format("Error accessing {}: {}\n", it->path().string(),
                                         ec.message());
                ec.clear();
                return false;
            }

            // Skip all windows and /mnt (WSL) files in order to save space.
            // Iterating over /mnt always get stuck for some reason.
            std::string path{it->path().string()};
            if (path.starts_with("C:\\Windows\\") || path.starts_with("/mnt")) {
                it.disable_recursion_pending();
                return false;
            }

            if (!it->is_regular_file())
                return false;
        }
        catch (const std::filesystem::filesystem_error& err) {
            std::cout << err.what() << "\n";
            return false;
        }
        catch (...) {
            std::cout << "Path to string conversion failed.\n";
            return false;
        }

        return true;
    }

private: // NOLINT
    Small_string m_dir;
    Files m_files;
    Symbols m_symbols;
    Options m_options;
};

#endif
