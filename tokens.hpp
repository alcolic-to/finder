#ifndef FINDER_TOKENS_HPP
#define FINDER_TOKENS_HPP

#include <cassert>
#include <cstdint>
#include <string>

// NOLINTBEGIN

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

        // Take just one token if we are the bracket to avoid taking ); or }; or (" etc.
        //
        bool non_bracket = *c != '(' && *c != ')' && *c != '[' && *c != ']' && *c != '{' &&
                           *c != '}' && *c != '<' && *c != '>';

        t.str() += *c++;

        if (non_bracket)
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

    // TODO: Make multicomment /**/ extract this too, because this will take
    // line even if it is multiplication.
    //
    bool ext_part_comment(Token& t)
    {
        if (*c != '*')
            return false;

        while (*c)
            t.str() += *c++;

        t.type() = Token_t::comment;
        return true;
    }

    bool ext_comment(Token& t)
    {
        return ext_single_comment(t) || ext_multi_comment(t) || ext_part_comment(t);
    }

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
        skip_spaces();

        while (std::isalnum(*c))
            t.str() += *c++;

        t.type() = Token_t::prep;
        return true;
    }

    const char* m_src = nullptr;
    const char* c = nullptr;
};

// NOLINTEND

#endif // FINDER_TOKENS_HPP
