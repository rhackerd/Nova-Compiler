#include "lexer.hpp"
#include <cctype>

char Compiler::Lexer::Lexer::peek(size_t dist) const {
    if (cursor + dist >= source.size()) return '\0';
    return source[cursor + dist];
}

char Compiler::Lexer::Lexer::advance() {
    char c = source[cursor++];
    if (c == '\n') {line++; col = 1;}
    else {col++;}
    return c;
}

void Compiler::Lexer::Lexer::skip_whitespace_and_comments() {
    while (cursor < source.size()) {
        char c = peek();
 
        // whitespace
        if (c == ' ' || c == '\t' || c == '\r' || c == '\n') {
            advance();
            continue;
        }
 
        // line comment: //
        if (c == '/' && peek(1) == '/') {
            while (cursor < source.size() && peek() != '\n')
                advance();
            continue;
        }
 
        break;
    }
}

Compiler::Lexer::Token Compiler::Lexer::Lexer::make_token(Compiler::Lexer::Kind kind, size_t start, uint32_t col_start) const {
    std::string_view lexeme = source.substr(start, cursor - start);
    SourceSpan span { line, col_start, col, file_id };
    return Token { kind, lexeme, span };
}

static Compiler::Lexer::Kind keyword_lookup(std::string_view s) {
    if (s == "func") return Compiler::Lexer::Kind::Func;
    if (s == "ret")  return Compiler::Lexer::Kind::Ret;
    if (s == "let")  return Compiler::Lexer::Kind::Let;
    if (s == "mut")  return Compiler::Lexer::Kind::Mut;
    if (s == "ext") return Compiler::Lexer::Kind::Extern;
    if (s == "struct") return Compiler::Lexer::Kind::Struct;
    if (s == "if") return Compiler::Lexer::Kind::If;
    if (s == "else") return Compiler::Lexer::Kind::Else;
    if (s == "loop") return Compiler::Lexer::Kind::Loop;
    if (s == "break") return Compiler::Lexer::Kind::Break;
    if (s == "continue") return Compiler::Lexer::Kind::Continue;

    if (s == "void")  return Compiler::Lexer::Kind::Void;
    if (s == "u8")    return Compiler::Lexer::Kind::U8;
    if (s == "u16")   return Compiler::Lexer::Kind::U16;
    if (s == "u32")   return Compiler::Lexer::Kind::U32;
    if (s == "u64")   return Compiler::Lexer::Kind::U64;
    if (s == "bool")  return Compiler::Lexer::Kind::Bool;
    if (s == "true")  return Compiler::Lexer::Kind::True;
    if (s == "false") return Compiler::Lexer::Kind::False;
    return Compiler::Lexer::Kind::Ident;
}

Compiler::Lexer::Token Compiler::Lexer::Lexer::lex_ident_or_keyword() {
    size_t   start     = cursor;
    uint32_t col_start = col;
 
    while (cursor < source.size()) {
        char c = peek();
        if (!std::isalnum(c) && c != '_') break;
        advance();
    }
 
    std::string_view lexeme = source.substr(start, cursor - start);
    Kind kind = keyword_lookup(lexeme);
    SourceSpan span { line, col_start, col, file_id };
    return Token { kind, lexeme, span };
}
 
Compiler::Lexer::Token Compiler::Lexer::Lexer::lex_int() {
    size_t   start     = cursor;
    uint32_t col_start = col;
 
    while (cursor < source.size() && std::isdigit(peek()))
        advance();
 
    // Reject immediately if the literal runs into an identifier character.
    // e.g. "123abc" is not valid — emit UNKNOWN so the parser can diagnose it.
    if (cursor < source.size() && (std::isalpha(peek()) || peek() == '_')) {
        while (cursor < source.size() && (std::isalnum(peek()) || peek() == '_'))
            advance();
        return make_token(Kind::Unknown, start, col_start);
    }
 
    return make_token(Kind::Lit_Int, start, col_start);
}

Compiler::Lexer::Token Compiler::Lexer::Lexer::lex_symbol() {
    size_t   start     = cursor;
    uint32_t col_start = col;
    char     c         = advance();
 
    switch (c) {
        case '(': return make_token(Kind::LParent,    start, col_start);
        case ')': return make_token(Kind::RParent,    start, col_start);
        case '{': return make_token(Kind::LBrace,    start, col_start);
        case '}': return make_token(Kind::RBrace,    start, col_start);
        case ';': return make_token(Kind::Semicolon, start, col_start);
        case ',': return make_token(Kind::Comma,     start, col_start);
        case '+':
            if (peek() == '=') { advance(); return make_token(Kind::PlusEq,  start, col_start); }
            return make_token(Kind::Plus, start, col_start);
        case '-':
            if (peek() == '>') { advance(); return make_token(Kind::Arrow,   start, col_start); }
            if (peek() == '=') { advance(); return make_token(Kind::MinusEq, start, col_start); }
            return make_token(Kind::Minus, start, col_start);
        case '*':
            if (peek() == '=') { advance(); return make_token(Kind::StarEq,  start, col_start); }
            return make_token(Kind::Star, start, col_start);
        case '/':
            if (peek() == '=') { advance(); return make_token(Kind::SlashEq, start, col_start); }
            return make_token(Kind::Slash, start, col_start);
        case ':': return make_token(Kind::Colon, start, col_start);
        case '=': {
            if (peek() == '=') { advance(); return make_token(Kind::EqEq,  start, col_start); }
            return make_token(Kind::Assign, start, col_start);
        }
        case '@': return make_token(Kind::At, start, col_start);
        case '.': return make_token(Kind::Dot, start, col_start);
        case '!': {
            if (peek() == '=') { advance(); return make_token(Kind::NotEq, start, col_start); }
            return make_token(Kind::Unknown, start, col_start);
        }
        case '<': {
            if (peek() == '=') { advance(); return make_token(Kind::LtEq,  start, col_start); }
            return make_token(Kind::Lt, start, col_start);
        }
        case '>': {
            if (peek() == '=') { advance(); return make_token(Kind::GtEq,  start, col_start); }
            return make_token(Kind::Gt, start, col_start);
        }

        case '"': {
            size_t start = cursor;
            uint32_t col_start = col;

            while (cursor < source.size() && peek() != '"') {
                advance();
            }

            size_t end = cursor;

            if (cursor < source.size()) {
                advance(); //fast fix for the extra closing quote (xxx") -> (xxx)
            } else {
                return make_token(Kind::Unknown, start, col_start);
            }

            std::string_view lexeme = source.substr(start, end - start);

            SourceSpan span { line, col_start, col, file_id };

            return Token {
                Kind::Lit_Str,
                lexeme,
                span
            };
        }

        default:
            return make_token(Kind::Unknown, start, col_start);
    }
}

std::vector<Compiler::Lexer::Token> Compiler::Lexer::Lexer::tokenize() {
    std::vector<Token> tokens;
    // Reserve a rough estimate to avoid repeated reallocation.
    // Source length / 3 is a reasonable heuristic.
    tokens.reserve(source.size() / 3);
 
    while (true) {
        skip_whitespace_and_comments();
 
        if (cursor >= source.size()) {
            SourceSpan span { line, col, col, file_id };
            tokens.push_back(Token { Kind::End, {}, span });
            break;
        }
 
        char c = peek();
        Token tok = [&]() -> Token {
            if (std::isalpha(c) || c == '_') return lex_ident_or_keyword();
            if (std::isdigit(c))             return lex_int();
            return lex_symbol();
        }();
 
        bool is_unknown = tok.kind == Kind::Unknown;
        tokens.push_back(tok);
 
        // Stop on UNKNOWN — do not silently continue past bad input.
        // The parser will emit the diagnostic with the span.
        if (is_unknown) break;
    }
 
    return tokens;
}