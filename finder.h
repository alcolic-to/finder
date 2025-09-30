#ifndef FINDER_H
#define FINDER_H

#include <algorithm>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <ranges>
#include <sstream>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

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
    explicit Options(std::string root, std::vector<std::string> ignore_list,
                     std::vector<std::string> include_list, bool files, bool symbols,
                     bool stat_only, bool verbose)
        : m_root{std::move(root)}
        , m_ignore_list{std::move(ignore_list)}
        , m_include_list{std::move(include_list)}
        , m_files{files}
        , m_symbols{symbols}
        , m_stat_only{stat_only}
        , m_verbose{verbose}
    {
    }

    [[nodiscard]] const std::string& root() const noexcept { return m_root; }

    [[nodiscard]] bool files_allowed() const noexcept { return m_files; }

    [[nodiscard]] bool symbols_allowed() const noexcept { return m_symbols; }

    [[nodiscard]] bool stats_only() const noexcept { return m_stat_only; }

    [[nodiscard]] bool verbose() const noexcept { return m_verbose; }

    [[nodiscard]] const std::vector<std::string>& ignore_list() const noexcept
    {
        return m_ignore_list;
    }

    [[nodiscard]] const std::vector<std::string>& include_list() const noexcept
    {
        return m_include_list;
    }

private:
    std::string m_root;
    std::vector<std::string> m_ignore_list;
    std::vector<std::string> m_include_list;
    bool m_files;
    bool m_symbols;
    bool m_stat_only;
    bool m_verbose;
};

class Finder {
public:
    using dir_iter = fs::recursive_directory_iterator;

    explicit Finder(const Options& opt)
        : m_root{opt.root()}
        , m_ignore_list{opt.ignore_list()}
        , m_include_list{opt.include_list()}
        , m_files_allowed(opt.files_allowed())
        , m_symbols_allowed(opt.symbols_allowed())
        , m_stat_only(opt.stats_only())
        , m_verbose(opt.verbose())
    {
        constexpr auto it_opt = fs::directory_options::skip_permission_denied;

        std::error_code ec;
        dir_iter it{m_root, it_opt, ec};

        for (; it != dir_iter{}; it.increment(ec)) {
            if (!check_iteration(it, ec))
                continue;

            fs::path path = it->path(); // Need copy for make_prefrred.
            FileInfo* file = m_files.insert(path.make_preferred()).get();

            if (!m_symbols_allowed || !supported_file(it))
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
        if (m_stat_only)
            std::exit(0); // NOLINT
    }

    [[nodiscard]] Symbols& symbols() noexcept { return m_symbols; }

    [[nodiscard]] Files& files() noexcept { return m_files; }

    [[nodiscard]] const fs::path& dir() const noexcept { return m_root; }

    auto find_files_partial(const std::string& regex, sz slice_count,
                            sz slice_number) const noexcept
    {
        return m_files.partial_search(regex, slice_count, slice_number);
    }

    auto find_files(const std::string& regex) { return m_files.search(regex); }

    Symbol* find_symbols(const std::string& symbol_name) { return m_symbols.search(symbol_name); }

private:
    void print_stats()
    {
        m_files.print_stats();

        if (m_symbols_allowed)
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
    [[nodiscard]] constexpr bool check_path(const std::string& path) const noexcept
    {
        /* Iterating over /mnt always get stuck for some reason. */
        if (path.starts_with("/mnt"))
            return false;

        const auto ignore_pred = [&](const std::string& s) { return path.starts_with(s); };
        if (std::ranges::find_if(m_ignore_list, ignore_pred) == m_ignore_list.end())
            return true;

        const auto include_pred = [&](const std::string& s) {
            return s.size() >= path.size() ? s.starts_with(path) : path.starts_with(s);
        };

        return std::ranges::find_if(m_include_list, include_pred) != m_include_list.end();
    }

    [[nodiscard]] bool check_iteration(dir_iter& it, std::error_code& ec) const noexcept
    {
        try {
            fs::path path_copy = it->path();
            std::string path{path_copy.make_preferred().string()};

            if (ec) {
                if (m_verbose)
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

            if (!it->is_regular_file() && !it->is_directory())
                return false;
        }
        catch (const std::filesystem::filesystem_error& err) {
            if (m_verbose)
                std::cout << err.what() << "\n";

            return false;
        }
        catch (...) {
            if (m_verbose)
                std::cout << "Path to string conversion failed.\n";

            return false;
        }

        return true;
    }

private: // NOLINT
    Files m_files;
    Symbols m_symbols;
    fs::path m_root;
    std::vector<std::string> m_ignore_list;
    std::vector<std::string> m_include_list;
    bool m_files_allowed;
    bool m_symbols_allowed;
    bool m_stat_only;
    bool m_verbose;
};

#endif // FINDER_H
