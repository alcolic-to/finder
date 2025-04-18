#include <filesystem>
#include <memory>
#include <ranges>
#include <vector>

#include "files.h"
#include "suffix_trie.h"

#ifndef SYMBOLS_H
#define SYMBOLS_H

// NOLINTBEGIN

class Symbol_refs {
public:
    Symbol_refs(File_info* file, size_t line) : m_file{file}, m_lines{line} {}

    File_info* m_file;
    std::vector<std::size_t> m_lines;
};

class Symbol {
public:
    Symbol(std::string name, File_info* file, size_t line)
        : m_name{std::move(name)}
        , m_refs{{file, line}}
    {
    }

    [[nodiscard]] const std::string& name() const noexcept { return m_name; }

    [[nodiscard]] const auto& refs() const noexcept { return m_refs; }

    std::string& name() noexcept { return m_name; }

    auto& refs() noexcept { return m_refs; }

private:
    std::string m_name;
    std::vector<Symbol_refs> m_refs;
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
    result insert(std::string symbol_name, File_info* file, size_t line)
    {
        auto* r = m_symbol_finder.search(symbol_name);
        if (r == nullptr)
            return insert_non_existing(std::move(symbol_name), file, line);

        Symbol* symbol = r->value();
        auto& sym_refs = symbol->refs();

        auto files_it =
            std::ranges::find_if(sym_refs, [&](Symbol_refs& ref) { return ref.m_file == file; });

        if (files_it == sym_refs.end()) {
            sym_refs.emplace_back(file, line);
            return {symbol, false};
        }

        auto& lines = files_it->m_lines;
        if (std::ranges::find(lines, line) == lines.end())
            lines.push_back(line);

        return {symbol, false};
    }

    result insert_non_existing(std::string symbol, File_info* file, size_t line)
    {
        m_symbols.push_back(std::make_unique<Symbol>(std::move(symbol), file, line));
        Symbol* new_symbol = m_symbols.back().get();

        m_symbol_finder.insert(new_symbol->name(), new_symbol);

        return {new_symbol, true};
    }

    void erase(const std::string& symbol_name, File_info* file, size_t line)
    {
        auto* r = m_symbol_finder.search(symbol_name);
        if (r == nullptr)
            return;

        Symbol* symbol = r->value();
        auto& sym_refs = symbol->refs();

        auto sym_refs_it =
            std::ranges::find_if(sym_refs, [&](Symbol_refs& ref) { return ref.m_file == file; });

        if (sym_refs_it == sym_refs.end())
            return;

        auto& lines = sym_refs_it->m_lines;
        auto lines_it = std::ranges::find(lines, line);
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

    auto search(const std::string& symbol_name)
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

private:
    std::vector<std::unique_ptr<Symbol>> m_symbols;

    // Trie that holds all suffixes of all symbols, which enables symbol search by symbol name.
    // Symbol name is not unique, so we must hold vector of symbol pointers.
    //
    // suffix_trie::Suffix_trie<std::vector<Symbol*>> m_symbol_finder;

    // Using art for now, because we don't want to allow symbol search by prefix in order to save
    // space.
    // TODO: CHeck memory usage with suffix trie of symbols.
    //
    art::ART<Symbol*> m_symbol_finder;
};

// NOLINTEND

#endif
