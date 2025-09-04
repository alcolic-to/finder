#ifndef FINDER_H
#define FINDER_H

#include <algorithm>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <iterator>
#include <ranges>
#include <sstream>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

#include "file_finder.h"
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
    static constexpr u32 opt_files = 1;      // Search files.
    static constexpr u32 opt_symbols = 2;    // Search both files and symbols.
    static constexpr u32 opt_stats_only = 4; // Print stats and quit.
    static constexpr u32 opt_verbose = 8;    // Print stats and quit.
    static constexpr u32 opt_all = opt_files | opt_symbols | opt_stats_only | opt_verbose;

    Options() : m_opt{opt_all} {}

    explicit Options(const std::string& opt)
    {
        std::stringstream ss{opt};
        std::string s;

        while (ss >> s) {
            if (s.starts_with("--")) {
                s = s.substr(2);
                if (s.starts_with("ignore="))
                    m_ignore_list = string_split(s.substr(sizeof("ignore")), ",");
                else if (s.starts_with("include="))
                    m_include_list = string_split(s.substr(sizeof("include")), ",");
                else
                    throw std::runtime_error{std::format("Invalid option {}.", s)};
            }
            else if (s.starts_with("-")) {
                s = s.substr(1);

                if (s.find('f') != std::string::npos)
                    m_opt |= Options::opt_files;

                if (s.find('s') != std::string::npos)
                    m_opt |= Options::opt_files | Options::opt_symbols;

                if (s.find('e') != std::string::npos)
                    m_opt |= Options::opt_stats_only;

                if (s.find('v') != std::string::npos)
                    m_opt |= Options::opt_verbose;
            }
        }

        if ((m_opt & ~opt_all) != 0U)
            throw std::runtime_error{"Invalid options."};

        if (m_opt == 0U)
            m_opt = opt_files;
    }

    [[nodiscard]] bool files_allowed() const noexcept { return (m_opt & opt_files) != 0U; }

    [[nodiscard]] bool symbols_allowed() const noexcept { return (m_opt & opt_symbols) != 0U; }

    [[nodiscard]] bool stats_only() const noexcept { return (m_opt & opt_stats_only) != 0U; }

    [[nodiscard]] bool verbose() const noexcept { return (m_opt & opt_verbose) != 0U; }

    [[nodiscard]] std::vector<std::string> ignore_list() const noexcept { return m_ignore_list; }

    [[nodiscard]] std::vector<std::string> include_list() const noexcept { return m_include_list; }

private:
    std::vector<std::string> m_ignore_list;
    std::vector<std::string> m_include_list;
    u32 m_opt = 0;
};

class Finder {
public:
    static constexpr sz files_search_limit = 128;

    using dir_iter = fs::recursive_directory_iterator;

    explicit Finder(const std::string& dir, Options opt = Options{})
        : m_dir{dir}
        , m_options{std::move(opt)}
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
                std::cout << std::format("Problem with openning file {}.\n", it->path().string());
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

    [[nodiscard]] FileFinder& files() noexcept { return m_files; }

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

    // For symbol finder, we only support cpp files.
    //
    static constexpr bool supported_file(const auto& dir_entry)
    {
        std::string ext{dir_entry->path().extension().string()};
        return ext == ".cpp" || ext == ".c" || ext == ".hpp" || ext == ".h";
    }

    /**
     * Checks whether provided path is supported for finder.
     * We will skip some OS specific paths to save space.
     * If user provided ignore list, we will ignore path if it is contained in ignore list and
     * it is not contained in include list.
     * Note that if include list contains path, we must not break iterations on that path, hence we
     * check whether ignore list paths starts with provided path.
     */
    constexpr bool check_path(const std::string& path)
    {
        // Skip all windows and /mnt (WSL) files in order to save space.
        // Iterating over /mnt always get stuck for some reason.
        if (path.starts_with("C:\\Windows") || path.starts_with("/Windows") ||
            path.starts_with("/mnt"))
            return false;

        const std::vector<std::string>& ignore_list = m_options.ignore_list();
        const std::vector<std::string>& include_list = m_options.include_list();

        const auto ignore_pred = [&](const std::string& s) { return path.starts_with(s); };
        if (std::ranges::find_if(ignore_list, ignore_pred) == ignore_list.end())
            return true;

        const auto include_pred = [&](const std::string& s) {
            return s.size() >= path.size() ? s.starts_with(path) : path.starts_with(s);
        };
        return std::ranges::find_if(include_list, include_pred) != include_list.end();
    }

    constexpr bool check_iteration(dir_iter& it, std::error_code& ec)
    {
        try {
            std::string path{it->path().string()};

            if (ec) {
                if (m_options.verbose())
                    std::cerr << std::format("Error accessing {}: {}\n", path, ec.message());

                ec.clear();
                return false;
            }

            if (!check_path(path)) {
                std::cout << std::format("Skipping: {}\n", path);
                it.disable_recursion_pending();
                return false;
            }

            if (it->is_directory() && it.depth() == 0)
                std::cout << std::format("Scanning: {}\n", path);

            // TODO: Check whether whis is needed.
            //
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
    FileFinder m_files;
    Symbols m_symbols;
    Options m_options;
};

#endif // FINDER_H
