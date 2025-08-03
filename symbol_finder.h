#ifndef SYMBOL_FINDER_H
#define SYMBOL_FINDER_H

#include <algorithm>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <iterator>
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

static const std::vector<std::string> cpp_keywords = {
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

static constexpr bool is_cpp_keyword(const std::string& s)
{
    return std::ranges::find(cpp_keywords, s) != cpp_keywords.end();
}

class Options {
public:
    static constexpr uint32_t opt_files = 1;      // Search files.
    static constexpr uint32_t opt_symbols = 2;    // Search both files and symbols.
    static constexpr uint32_t opt_stats_only = 4; // Print stats and quit.
    static constexpr uint32_t opt_verbose = 8;    // Print stats and quit.
    static constexpr uint32_t opt_all = opt_files | opt_symbols | opt_stats_only | opt_verbose;

    Options() : m_opt{opt_all} {}

    explicit Options(const std::string& opt)
    {
        if (opt.find('f') != std::string::npos)
            m_opt |= Options::opt_files;

        if (opt.find('s') != std::string::npos)
            m_opt |= Options::opt_files | Options::opt_symbols;

        if (opt.find('e') != std::string::npos)
            m_opt |= Options::opt_stats_only;

        if (opt.find('v') != std::string::npos)
            m_opt |= Options::opt_stats_only;

        if ((m_opt & ~opt_all) != 0U)
            throw std::runtime_error{"Invalid options."};

        if (m_opt == 0U)
            m_opt = opt_files;
    }

    [[nodiscard]] bool files_allowed() const noexcept { return (m_opt & opt_files) != 0U; }

    [[nodiscard]] bool symbols_allowed() const noexcept { return (m_opt & opt_symbols) != 0U; }

    [[nodiscard]] bool stats_only() const noexcept { return (m_opt & opt_stats_only) != 0U; }

    [[nodiscard]] bool verbose() const noexcept { return (m_opt & opt_verbose) != 0U; }

private:
    uint32_t m_opt = 0;
};

class Symbol_finder {
public:
    static constexpr size_t files_search_limit = 128;

    using dir_iter = fs::recursive_directory_iterator;

    explicit Symbol_finder(const std::string& dir, Options opt = Options{})
        : m_dir{dir}
        , m_options{opt}
    {
        constexpr auto it_opt = fs::directory_options::skip_permission_denied;

        std::error_code ec;
        dir_iter it{dir, it_opt, ec};

        for (; it != dir_iter{}; it.increment(ec)) {
            if (!check_iteration(it, ec))
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
                    if (token.type() != Token_t::word || is_cpp_keyword(token.str()))
                        continue;

                    m_symbols.insert(token.str(), file, line_num, fline);
                }
            }
        }

        print_stats();
        if (m_options.stats_only())
            std::exit(0); // NOLINT
    }

    [[nodiscard]] Symbols& symbols() noexcept { return m_symbols; }

    [[nodiscard]] Files& files() noexcept { return m_files; }

    [[nodiscard]] const fs::path& dir() const noexcept { return m_dir; }

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

    constexpr bool check_iteration(dir_iter& it, std::error_code& ec)
    {
        try {
            if (ec) {
                if (m_options.verbose())
                    std::cerr << std::format("Error accessing {}: {}\n", it->path().string(),
                                             ec.message());
                ec.clear();
                return false;
            }

            // Skip all windows and /mnt (WSL) files in order to save space.
            // Iterating over /mnt always get stuck for some reason.
            std::string path{it->path().string()};
            if (path.starts_with("C:\\Windows") || path.starts_with("/Windows") ||
                path.starts_with("/mnt")) {
                std::cout << std::format("Skipping: {}\n", it->path().string());
                it.disable_recursion_pending();
                return false;
            }

            if (it->is_directory() && it.depth() == 0)
                std::cout << std::format("Scanning: {}\n", it->path().string());

            if (!it->is_regular_file() && !it->is_directory())
                return false;
        }
        catch (const std::filesystem::filesystem_error& err) {
            if (m_options.verbose())
                std::cout << err.what() << "\n";

            return false;
        }
        catch (...) {
            if (m_options.verbose())
                std::cout << "Path to string conversion failed.\n";

            return false;
        }

        return true;
    }

private: // NOLINT
    fs::path m_dir;
    Files m_files;
    Symbols m_symbols;
    Options m_options;
};

#endif
