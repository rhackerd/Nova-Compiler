#pragma once
#include "module.hpp"
#include "token.hpp"
#include "ast.hpp"
#include <vector>
#include <string>
#include <optional>

namespace Compiler::Parser {

    using Diagnostic = Lexer::Diagnostic;

    struct ParseResult {
        AST::Program            program;
        AST::ExprArena          expr_arena;
        std::vector<Diagnostic> diagnostics;
        bool ok() const {return diagnostics.empty();}
    };

    struct Parser {
        const std::vector<Lexer::Token>& tokens;
        Compiler::Module::ModuleTable& module_table;
        std::filesystem::path source_path;
        size_t cursor = 0;

        explicit Parser(const std::vector<Lexer::Token>& tokens,
                                Compiler::Module::ModuleTable& module_table,
                                std::filesystem::path source_path)
            : tokens(tokens),
                module_table(module_table),
                source_path(std::move(source_path))
        {}

        ParseResult parse();

        private:
            void handle_directive(std::vector<Diagnostic>& diags);

            const Lexer::Token& peek(size_t dist = 0) const;
            const Lexer::Token& advance();
            bool at_end() const;

            std::optional<Lexer::Token> expect(Lexer::Kind kind, std::vector<Diagnostic>& diags, const std::source_location loc = std::source_location::current());

            std::optional<AST::FuncDecl> parse_func_decl(AST::ExprArena& arena, std::vector<Diagnostic>& diags);
            std::optional<AST::Param> parse_param(std::vector<Diagnostic>& diags);
            std::optional<AST::Stmt> parse_stmt(AST::ExprArena& arena, std::vector<Diagnostic>& diags);
            std::optional<uint32_t> parse_expr(AST::ExprArena& arena,
                                   std::vector<Diagnostic>& diags,
                                   uint8_t bp = 0,
                                   bool allow_struct_literal = true);
            std::optional<AST::Kind> parse_type(std::vector<Diagnostic>& diags, std::string_view* type_name_out);
            std::optional<AST::StructDecl> parse_struct_decl(std::vector<Lexer::Diagnostic>& diags);
    };
};