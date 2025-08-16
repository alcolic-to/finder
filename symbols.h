#ifndef SYMBOLS_H
#define SYMBOLS_H

#include <filesystem>
#include <memory>
#include <ranges>
#include <vector>

#include "files.h"
#include "small_string.h"
#include "suffix_trie.h"
#include "util.h"

// NOLINTBEGIN

class Line {
public:
    Line(sz line_number, const std::string& preview) : m_number{line_number}, m_preview{preview} {}

    sz number() const noexcept { return m_number; }

    const char* preview() const noexcept { return m_preview; }

private:
    sz m_number;
    Small_string m_preview; // Line preview which will be displayed with symbol in print.
};

class Symbol_file_refs {
public:
    Symbol_file_refs(File_info* file, sz line, const std::string& preview)
        : m_file{file}
        , m_lines{Line{line, preview}}
    {
    }

    const File_info* file() const noexcept { return m_file; }

    const std::vector<Line>& lines() const noexcept { return m_lines; }

    std::vector<Line>& lines() noexcept { return m_lines; }

private:
    File_info* m_file;
    std::vector<Line> m_lines;
};

class Symbol {
public:
    Symbol(const std::string& name, File_info* file, sz line_number, const std::string& preview)
        : m_name{name}
        , m_refs{Symbol_file_refs{file, line_number, preview}}
    {
    }

    [[nodiscard]] const char* name() const noexcept { return m_name; }

    [[nodiscard]] const auto& refs() const noexcept { return m_refs; }

    auto& refs() noexcept { return m_refs; }

private:
    Small_string m_name;
    std::vector<Symbol_file_refs> m_refs;
};

// Class that wraps insert result.
// It holds a pointer to Leaf and a bool flag representing whether insert succeeded (read insert
// for more details).
//
class result {
public:
    result(Symbol* value, bool ok) : m_value{value}, m_ok{ok} { assert(m_value != nullptr); }

    Symbol* get() noexcept { return m_value; }

    [[nodiscard]] const Symbol* get() const noexcept { return m_value; }

    [[nodiscard]] bool ok() const noexcept { return m_ok; }

    Symbol* operator->() noexcept { return get(); }

    const Symbol* operator->() const noexcept { return get(); }

    constexpr operator bool() const noexcept { return ok(); }

private:
    Symbol* m_value;
    bool m_ok;
};

class Symbols {
public:
    result insert(const std::string& symbol_name, File_info* file, sz line_number,
                  const std::string& line_preview)
    {
        auto* r = m_symbol_finder.search(symbol_name);
        if (r == nullptr)
            return insert_non_existing(symbol_name, file, line_number, line_preview);

        Symbol* symbol = r->value();
        auto& sym_refs = symbol->refs();

        // m_symbol_searcher.insert_suffix(symbol_name, symbol);

        auto files_it = std::ranges::find_if(
            sym_refs, [&](Symbol_file_refs& ref) { return ref.file() == file; });

        if (files_it == sym_refs.end()) {
            sym_refs.emplace_back(file, line_number, line_preview);
            return {symbol, false};
        }

        auto& lines = files_it->lines();
        if (std::ranges::find_if(lines, [&](const Line& l) { return l.number() == line_number; }) ==
            lines.end())
            lines.push_back(Line{line_number, line_preview});

        return {symbol, false};
    }

    result insert_non_existing(const std::string& symbol, File_info* file, sz line,
                               const std::string& preview)
    {
        m_symbols.push_back(std::make_unique<Symbol>(symbol, file, line, preview));
        Symbol* new_symbol = m_symbols.back().get();

        m_symbol_finder.insert(new_symbol->name(), new_symbol);
        // m_symbol_searcher.insert_suffix(new_symbol->name(), new_symbol);

        return {new_symbol, true};
    }

    void erase(const std::string& symbol_name, File_info* file, sz line_number)
    {
        auto* r = m_symbol_finder.search(symbol_name);
        if (r == nullptr)
            return;

        Symbol* symbol = r->value();
        auto& sym_refs = symbol->refs();

        auto sym_refs_it = std::ranges::find_if(
            sym_refs, [&](Symbol_file_refs& ref) { return ref.file() == file; });

        if (sym_refs_it == sym_refs.end())
            return;

        auto& lines = sym_refs_it->lines();
        auto lines_it =
            std::ranges::find_if(lines, [&](Line& l) { return l.number() == line_number; });
        if (lines_it == lines.end())
            return;

        lines.erase(lines_it);
        if (lines.empty())
            sym_refs.erase(sym_refs_it);

        if (sym_refs.empty())
            m_symbol_finder.erase(symbol_name);

        auto symbols_it =
            std::ranges::find_if(m_symbols, [&](std::unique_ptr<Symbol>& unique_symbol) {
                return unique_symbol.get() == symbol;
            });

        assert(symbols_it != m_symbols.end());
        m_symbols.erase(symbols_it);
    }

    Symbol* search(const std::string& symbol_name)
    {
        auto* symbol = m_symbol_finder.search(symbol_name);
        return symbol != nullptr ? symbol->value() : nullptr;
    }

    auto symbols_size()
    {
        return m_symbols.size() * (sizeof(std::unique_ptr<Symbol>) + sizeof(Symbol));
    }

    auto symbol_finder_size(bool full_leaves = true)
    {
        return m_symbol_finder.size_in_bytes(full_leaves);
    }

    void print_stats()
    {
        std::cout << "---------------------------------------\n";
        std::cout << "Symbols count: " << m_symbols.size() << "\n";

        std::cout << "Symbol finder stats:\n";
        m_symbol_finder.print_stats();

        std::cout << "Symbol searcher stats:\n";
        // m_symbol_searcher.print_stats();
        std::cout << "---------------------------------------\n";
    }

public:
    std::vector<std::unique_ptr<Symbol>> m_symbols;

    // Trie that holds all suffixes of all symbols, which enables symbol search by symbol name.
    // Symbol name is not unique, so we must hold vector of symbol pointers.
    //
    // suffix_trie::Suffix_trie<std::vector<Symbol*>> m_symbol_finder;

    // Using art for now, because we don't want to allow symbol search by prefix in order to save
    // space.
    // TODO: Check memory usage with suffix trie of symbols.
    //
    art::ART<Symbol*> m_symbol_finder;

    // suffix_trie::Suffix_trie<Symbol*> m_symbol_searcher;
};

// NOLINTEND

#endif
