#pragma once

#include <cstdint>
#include <string_view>
#include <vector>
#include "token.hpp"
namespace Compiler::Lexer {

    struct Lexer {
        std::string_view source;
        size_t cursor = 0;
        uint32_t line = 1;
        uint32_t col = 1;
        uint16_t file_id = 0;

        explicit Lexer(std::string_view source, uint16_t file_id = 0) : source(source), file_id(file_id) {}

        std::vector<Token> tokenize();

        private:
            char peek(size_t dist = 0) const;
            char advance();
            void skip_whitespace_and_comments();

            Token make_token(Kind kind, size_t start, uint32_t col_start) const;

            Token lex_ident_or_keyword();
            Token lex_int();
            Token lex_symbol();
    };
}