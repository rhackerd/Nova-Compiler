#include "parser.hpp"
#include "ast.hpp"
#include "module.hpp"
#include "token.hpp"
#include <algorithm>
#include <iostream>
#include <optional>
#include <source_location>
#include <sstream>

namespace Compiler::Parser {
    const Lexer::Token& Parser::peek(size_t dist) const {
        size_t idx = cursor + dist;
        if (idx >= tokens.size()) return tokens.back();
        return tokens[idx];
    }

    const Lexer::Token& Parser::advance() {
        const Lexer::Token& t = tokens[cursor];
        if (cursor < tokens.size() - 1) cursor++;
        return t;
    }

    bool Parser::at_end() const {
        return peek().kind == Lexer::Kind::End;
    }

    std::optional<AST::StructDecl> Parser::parse_struct_decl(
        std::vector<Lexer::Diagnostic>& diags) {

        Lexer::SourceSpan span = peek().span;
        advance(); // consume 'struct'

        auto name_tok = expect(Lexer::Kind::Ident, diags);
        if (!name_tok) return std::nullopt;

        if (!expect(Lexer::Kind::LBrace, diags)) return std::nullopt;

        std::vector<AST::StructField> fields;
        while (peek().kind != Lexer::Kind::RBrace &&
            peek().kind != Lexer::Kind::End) {

            if (peek().kind != Lexer::Kind::Ident) { advance(); continue; }
            std::string_view fname = peek().lexeme;
            advance();

            if (!expect(Lexer::Kind::Colon, diags)) return std::nullopt;

            std::string_view type_name;
            auto kind = parse_type(diags, &type_name);
            if (!kind) return std::nullopt;

            // accept , or ;
            if (peek().kind == Lexer::Kind::Comma ||
                peek().kind == Lexer::Kind::Semicolon)
                advance();

            fields.push_back(AST::StructField { fname, *kind, type_name });
        }

        if (!expect(Lexer::Kind::RBrace, diags)) return std::nullopt;

        return AST::StructDecl { name_tok->lexeme, std::move(fields), span };
    }

    void Parser::handle_directive(std::vector<Diagnostic>& diags) {
        // consume '@'
        advance();

        if (peek().kind != Lexer::Kind::Ident) {
            Lexer::MakeError(&diags, peek().span, "Expected directive name after '@'.");
            return;
        }

        std::string_view directive = peek().lexeme;
        advance();

        if (directive == "name") {
            // @name in a .nl file is ignored — only meaningful in .nh
            if (peek().kind == Lexer::Kind::Ident) advance();
            return;
        }

        if (directive == "module") {
            // @module path_or_name
            if (peek().kind != Lexer::Kind::Ident) {
                Lexer::MakeError(&diags, peek().span, "Expected module name or path after '@module'.");
                return;
            }
            std::string name = std::string(peek().lexeme);
            advance();

            // Handle path segments: @module ../inc/math
            // The lexer won't give us the full path as one token,
            // so for now module names are single identifiers.
            // Path support comes when string literals land.

            module_table.load(name, source_path, diags);
            return;
        }

        Lexer::MakeError(&diags, peek().span, "Unknown directive '@'" + std::string(directive) + "'");
    }

    std::optional<Lexer::Token> Parser::expect(Lexer::Kind kind, std::vector<Diagnostic>& diag, const std::source_location loc) {
        const Lexer::Token& t = peek();
        if (t.kind == kind) {
            advance();
            return t;
        }
        std::ostringstream msg;
        msg << "expected '" << token_kind_name(kind)
            << "' but found '" << token_kind_name(t.kind) << "'";
        if (!t.lexeme.empty()) msg << " ('" << t.lexeme << "')";
        Lexer::MakeError(&diag, t.span, msg.str(), loc);
        return std::nullopt;
    }


    std::optional<AST::Kind> Parser::parse_type(std::vector<Lexer::Diagnostic>& diags,
                                             std::string_view* type_name_out) {
        const Lexer::Token& t = peek();
        switch (t.kind) {
            case Lexer::Kind::U8:   advance(); return AST::Kind::U8;
            case Lexer::Kind::U16:  advance(); return AST::Kind::U16;
            case Lexer::Kind::U32:  advance(); return AST::Kind::U32;
            case Lexer::Kind::U64:  advance(); return AST::Kind::U64;
            case Lexer::Kind::Bool: advance(); return AST::Kind::Bool;
            case Lexer::Kind::Ident:
                // struct type — name is the identifier
                if (type_name_out) *type_name_out = t.lexeme;
                advance();
                return AST::Kind::Struct;
            default:
                std::ostringstream msg;
                msg << "expected a type but found '" << token_kind_name(t.kind) << "'";
                if (!t.lexeme.empty()) msg << " ('" << t.lexeme << "')";
                Lexer::MakeError(&diags, t.span, msg.str());
                return std::nullopt;
        }
    }

    static uint8_t infix_bp(Lexer::Kind kind) {
        switch (kind) {
            case Lexer::Kind::EqEq:
            case Lexer::Kind::NotEq:
            case Lexer::Kind::Lt:
            case Lexer::Kind::Gt:
            case Lexer::Kind::LtEq:
            case Lexer::Kind::GtEq:  return 5;  // lower than arithmetic
            case Lexer::Kind::Plus:
            case Lexer::Kind::Minus: return 10;
            case Lexer::Kind::Star:
            case Lexer::Kind::Slash: return 20;
            default:                 return 0;
        }
    }

    static std::optional<AST::CompareOpKind> to_cmpop(Lexer::Kind kind) {
        switch (kind) {
            case Lexer::Kind::EqEq:  return AST::CompareOpKind::Eq;
            case Lexer::Kind::NotEq: return AST::CompareOpKind::NotEq;
            case Lexer::Kind::Lt:    return AST::CompareOpKind::Lt;
            case Lexer::Kind::Gt:    return AST::CompareOpKind::Gt;
            case Lexer::Kind::LtEq:  return AST::CompareOpKind::LtEq;
            case Lexer::Kind::GtEq:  return AST::CompareOpKind::GtEq;
            default:                 return std::nullopt;
        }
    }
    
    static AST::BinaryOpKind to_binop(Lexer::Kind kind) {
        switch (kind) {
            case Lexer::Kind::Plus:  return AST::BinaryOpKind::Add;
            case Lexer::Kind::Minus: return AST::BinaryOpKind::Sub;
            case Lexer::Kind::Star:  return AST::BinaryOpKind::Mul;
            case Lexer::Kind::Slash: return AST::BinaryOpKind::Div;
            default:                 return AST::BinaryOpKind::Add;
        }
    }

    std::optional<uint32_t> Parser::parse_expr(AST::ExprArena& arena,
                                   std::vector<Diagnostic>& diags,
                                   uint8_t bp,
                                   bool allow_struct_literal) {
        // --- Parse left-hand side (prefix position) ---
        const Lexer::Token& t = peek();
        std::optional<uint32_t> lhs;
    
        if (t.kind == Lexer::Kind::Lit_Int) {
            advance();
            uint64_t value = 0;
            for (char c : t.lexeme) {
                uint64_t prev = value;
                value = value * 10 + (c - '0');
                if (value < prev) {
                    Lexer::MakeError(&diags, t.span, "Integer literal overflows u64.");
                    return std::nullopt;
                }
            }
            AST::Expr node;
            node.kind      = AST::ExprKind::Lit_Int;
            node.span      = t.span;
            node.int_value = value;
            lhs = arena.add(std::move(node));
        } else if (t.kind == Lexer::Kind::True || t.kind == Lexer::Kind::False) {
            advance();
            AST::Expr node;
            node.kind      = AST::ExprKind::Lit_Bool;
            node.span      = t.span;
            node.int_value = (t.kind == Lexer::Kind::True) ? 1 : 0;
            lhs = arena.add(std::move(node));
        } else if (t.kind == Lexer::Kind::Ident) {
            advance();

            if (peek().kind == Lexer::Kind::LParent) {
                // Function call
                advance(); // consume '('
                AST::Expr node;
                node.kind   = AST::ExprKind::Call;
                node.span   = t.span;
                node.name   = t.lexeme;

                // Parse argument list
                if (peek().kind != Lexer::Kind::RParent) {
                    auto arg = parse_expr(arena, diags);
                    if (!arg) return std::nullopt;
                    node.args.push_back(*arg);

                    while (peek().kind == Lexer::Kind::Comma) {
                        advance(); // consume ','
                        auto next = parse_expr(arena, diags);
                        if (!next) return std::nullopt;
                        node.args.push_back(*next);
                    }
                }

                if (!expect(Lexer::Kind::RParent, diags)) return std::nullopt;
                lhs = arena.add(std::move(node));
            } else if (allow_struct_literal &&
                peek().kind == Lexer::Kind::LBrace &&
                peek().span.line == t.span.line) {
                // struct literal: Vec2 { x: 1, y: 2 }
                advance(); // consume '{'
                AST::Expr node;
                node.kind = AST::ExprKind::Lit_Struct;
                node.span = t.span;
                node.name = t.lexeme; // struct type name

                while (peek().kind != Lexer::Kind::RBrace &&
                    peek().kind != Lexer::Kind::End) {
                    if (peek().kind != Lexer::Kind::Ident) { advance(); continue; }
                    std::string_view fname = peek().lexeme;
                    advance();
                    if (!expect(Lexer::Kind::Colon, diags)) return std::nullopt;
                    auto val = parse_expr(arena, diags);
                    if (!val) return std::nullopt;
                    node.fields.push_back(AST::FieldInit { fname, *val });
                    // accept either , or ;
                    if (peek().kind == Lexer::Kind::Comma ||
                        peek().kind == Lexer::Kind::Semicolon)
                        advance();
                }
                if (!expect(Lexer::Kind::RBrace, diags)) return std::nullopt;
                lhs = arena.add(std::move(node));
            }else {
                // Plain identifier
                AST::Expr node;
                node.kind = AST::ExprKind::Identifier;
                node.span = t.span;
                node.name = t.lexeme;
                lhs = arena.add(std::move(node));
            }
        } else {
            std::ostringstream msg;
            msg << "expected an expression but found '" << token_kind_name(t.kind) << "'";
            if (!t.lexeme.empty()) msg << " ('" << t.lexeme << "')";
            Lexer::MakeError(&diags, t.span, msg.str());
            return std::nullopt;
        }
    
        // --- Parse infix operators as long as they bind tighter than bp ---
        while (true) {
            Lexer::Kind op_kind = peek().kind;
            uint8_t     power   = infix_bp(op_kind);
            if (power <= bp) break;

            Lexer::SourceSpan op_span = peek().span;
            advance();

            auto rhs = parse_expr(arena, diags, power, allow_struct_literal);
            if (!rhs) return std::nullopt;

            if (auto cmp = to_cmpop(op_kind)) {
                AST::Expr node;
                node.kind    = AST::ExprKind::Compare;
                node.span    = op_span;
                node.cmp_op  = *cmp;
                node.lhs_idx = *lhs;
                node.rhs_idx = *rhs;
                lhs = arena.add(std::move(node));
            } else {
                AST::Expr node;
                node.kind    = AST::ExprKind::BinaryOp;
                node.span    = op_span;
                node.op      = to_binop(op_kind);
                node.lhs_idx = *lhs;
                node.rhs_idx = *rhs;
                lhs = arena.add(std::move(node));
            }
        }

        // Field access chaining: expr.field
        while (peek().kind == Lexer::Kind::Dot) {
            advance(); // consume '.'
            if (peek().kind != Lexer::Kind::Ident) {
                Lexer::MakeError(&diags, peek().span, "Expected field name after '.'");
                return std::nullopt;
            }
            std::string_view fname = peek().lexeme;
            Lexer::SourceSpan fspan = peek().span;
            advance();

            AST::Expr node;
            node.kind    = AST::ExprKind::Field_Access;
            node.span    = fspan;
            node.lhs_idx = *lhs;
            node.name    = fname;
            lhs = arena.add(std::move(node));
        }


        return lhs;
    }

    std::optional<AST::Stmt> Parser::parse_stmt(AST::ExprArena& arena,
                                                std::vector<Diagnostic>& diags) {
        const Lexer::Token& t = peek();

        // --- return ---
        if (t.kind == Lexer::Kind::Ret) {
            Lexer::SourceSpan span = t.span;
            advance();
            auto expr_idx = parse_expr(arena, diags);
            if (!expr_idx) return std::nullopt;
            if (!expect(Lexer::Kind::Semicolon, diags)) return std::nullopt;
            AST::Stmt s;
            s.kind     = AST::StmtKind::Return;
            s.span     = span;
            s.expr_idx = *expr_idx;
            return s;
        }

        // --- let / let mut ---
        if (t.kind == Lexer::Kind::Let) {
            Lexer::SourceSpan span = t.span;
            advance(); // consume 'let'

            bool is_mut = false;
            if (peek().kind == Lexer::Kind::Mut) {
                is_mut = true;
                advance(); // consume 'mut'
            }

            auto name_tok = expect(Lexer::Kind::Ident, diags);
            if (!name_tok) return std::nullopt;

            if (!expect(Lexer::Kind::Colon, diags)) return std::nullopt;

            std::string_view type_name;
            auto type = parse_type(diags, &type_name);
            if (!type) return std::nullopt;

            // Require initializer — no uninitialized bindings
            if (peek().kind != Lexer::Kind::Assign) {
                Lexer::MakeError(&diags, peek().span, "Let bindings require an initializer, add '= <expr>'.");
                return std::nullopt;
            }
            advance(); // consume '='

            auto expr_idx = parse_expr(arena, diags);
            if (!expr_idx) return std::nullopt;

            if (!expect(Lexer::Kind::Semicolon, diags)) return std::nullopt;

            AST::Stmt s;
            s.kind     = AST::StmtKind::VarDecl;
            s.span     = span;
            s.name     = name_tok->lexeme;
            s.type     = *type;
            s.type_name = type_name;
            s.is_mut   = is_mut;
            s.expr_idx = *expr_idx;
            return s;
        }

        // field assign: IDENT.field = expr;
        if (t.kind == Lexer::Kind::Ident &&
            peek(1).kind == Lexer::Kind::Dot &&
            peek(2).kind == Lexer::Kind::Ident &&
            peek(3).kind == Lexer::Kind::Assign) {
            Lexer::SourceSpan span = t.span;
            std::string_view base  = t.lexeme;
            advance(); // consume base ident
            advance(); // consume '.'
            std::string_view field = peek().lexeme;
            advance(); // consume field name
            advance(); // consume '='

            auto expr_idx = parse_expr(arena, diags);
            if (!expr_idx) return std::nullopt;
            if (!expect(Lexer::Kind::Semicolon, diags)) return std::nullopt;

            AST::Stmt s;
            s.kind     = AST::StmtKind::FieldAssign;
            s.span     = span;
            s.name     = base;
            s.field    = field;
            s.expr_idx = *expr_idx;
            return s;
        }

        // --- assignment: IDENT = expr; ---
        if (t.kind == Lexer::Kind::Ident && peek(1).kind == Lexer::Kind::Assign) {
            Lexer::SourceSpan span = t.span;
            std::string_view name  = t.lexeme;
            advance(); // consume ident
            advance(); // consume '='

            auto expr_idx = parse_expr(arena, diags);
            if (!expr_idx) return std::nullopt;

            if (!expect(Lexer::Kind::Semicolon, diags)) return std::nullopt;

            AST::Stmt s;
            s.kind     = AST::StmtKind::Assign;
            s.span     = span;
            s.name     = name;
            s.expr_idx = *expr_idx;
            return s;
        }

        if (t.kind == Lexer::Kind::Ident && peek(1).kind == Lexer::Kind::LParent) {
            auto expr_idx = parse_expr(arena, diags);
            if (!expr_idx) return std::nullopt;
            if (!expect(Lexer::Kind::Semicolon, diags)) return std::nullopt;
            AST::Stmt s;
            s.kind     = AST::StmtKind::Expr;  // new kind — discard return value
            s.span     = t.span;
            s.expr_idx = *expr_idx;
            return s;
        }
        // --- if ---
        if (t.kind == Lexer::Kind::If) {
            Lexer::SourceSpan span = t.span;
            advance();

            auto cond_idx = parse_expr(arena, diags, 0, false);
            if (!cond_idx) return std::nullopt;

            if (arena.get(*cond_idx).kind != AST::ExprKind::Compare) {
                Lexer::MakeError(&diags, span, "If condition must be comparison expression (ex. a == b).");
                return std::nullopt;
            }

            if (!expect(Lexer::Kind::LBrace, diags)) return std::nullopt;  // only once

            std::vector<AST::Stmt> then_body;
            while (peek().kind != Lexer::Kind::RBrace && !at_end()) {
                auto s = parse_stmt(arena, diags);
                if (!s) return std::nullopt;
                then_body.push_back(std::move(*s));
            }
            if (!expect(Lexer::Kind::RBrace, diags)) return std::nullopt;

            std::vector<AST::Stmt> else_body;
            if (peek().kind == Lexer::Kind::Else) {
                advance();
                if (!expect(Lexer::Kind::LBrace, diags)) return std::nullopt;
                while (peek().kind != Lexer::Kind::RBrace && !at_end()) {
                    auto s = parse_stmt(arena, diags);
                    if (!s) return std::nullopt;
                    else_body.push_back(std::move(*s));
                }
                if (!expect(Lexer::Kind::RBrace, diags)) return std::nullopt;
            }

            AST::Stmt s;
            s.kind      = AST::StmtKind::If;
            s.span      = span;
            s.expr_idx  = *cond_idx;
            s.then_body = std::move(then_body);
            s.else_body = std::move(else_body);
            return s;
        }

        // --- loop ---
        if (t.kind == Lexer::Kind::Loop) {
            Lexer::SourceSpan span = t.span;
            advance(); // consume 'loop'

            if (!expect(Lexer::Kind::LBrace, diags)) return std::nullopt;

            std::vector<AST::Stmt> body;
            while (peek().kind != Lexer::Kind::RBrace && !at_end()) {
                auto s = parse_stmt(arena, diags);
                if (!s) return std::nullopt;
                body.push_back(std::move(*s));
            }
            if (!expect(Lexer::Kind::RBrace, diags)) return std::nullopt;

            AST::Stmt s;
            s.kind      = AST::StmtKind::Loop;
            s.span      = span;
            s.then_body = std::move(body);
            return s;
        }

        // --- break ---
        if (t.kind == Lexer::Kind::Break) {
            Lexer::SourceSpan span = t.span;
            advance();
            if (!expect(Lexer::Kind::Semicolon, diags)) return std::nullopt;
            AST::Stmt s;
            s.kind = AST::StmtKind::Break;
            s.span = span;
            return s;
        }

        // --- continue ---
        if (t.kind == Lexer::Kind::Continue) {
            Lexer::SourceSpan span = t.span;
            advance();
            if (!expect(Lexer::Kind::Semicolon, diags)) return std::nullopt;
            AST::Stmt s;
            s.kind = AST::StmtKind::Continue;
            s.span = span;
            return s;
        }

        if (t.kind == Lexer::Kind::Ident) {
            auto name = t.lexeme;
            Lexer::SourceSpan span = t.span;

            Lexer::Kind op_kind = peek(1).kind;
            AST::BinaryOpKind bin_op;
            bool is_compound = true;

            switch (op_kind) {
                case Lexer::Kind::PlusEq:  bin_op = AST::BinaryOpKind::Add; break;
                case Lexer::Kind::MinusEq: bin_op = AST::BinaryOpKind::Sub; break;
                case Lexer::Kind::StarEq:  bin_op = AST::BinaryOpKind::Mul; break;
                case Lexer::Kind::SlashEq: bin_op = AST::BinaryOpKind::Div; break;
                default: is_compound = false; break;
            }

            if (is_compound) {
                advance(); // consume ident
                advance(); // consume +=/-=/*=/=/=

                auto rhs = parse_expr(arena, diags);
                if (!rhs) return std::nullopt;
                if (!expect(Lexer::Kind::Semicolon, diags)) return std::nullopt;

                // Desugar: x += expr → x = x + expr
                AST::Expr lhs_ident;
                lhs_ident.kind = AST::ExprKind::Identifier;
                lhs_ident.span = span;
                lhs_ident.name = name;
                auto lhs_idx = arena.add(std::move(lhs_ident));

                AST::Expr binop;
                binop.kind    = AST::ExprKind::BinaryOp;
                binop.span    = span;
                binop.op      = bin_op;
                binop.lhs_idx = lhs_idx;
                binop.rhs_idx = *rhs;
                auto binop_idx = arena.add(std::move(binop));

                AST::Stmt s;
                s.kind     = AST::StmtKind::Assign;
                s.span     = span;
                s.name     = name;
                s.expr_idx = binop_idx;
                return s;
            }
        }

        std::ostringstream msg;
        msg << "expected a statement but found '" << token_kind_name(t.kind) << "'";
        if (!t.lexeme.empty()) msg << " ('" << t.lexeme << "')";
        Lexer::MakeError(&diags, t.span, msg.str());
        return std::nullopt;
    }

    std::optional<AST::Param> Parser::parse_param(std::vector<Diagnostic>& diags) {
        // IDENT ':' type
        auto name_tok = expect(Lexer::Kind::Ident, diags);
        if (!name_tok) return std::nullopt;
    
        if (!expect(Lexer::Kind::Colon, diags)) return std::nullopt;
    
        std::string_view type_name;
        auto type = parse_type(diags, &type_name);
        if (!type) return std::nullopt;
    
        Lexer::SourceSpan span = name_tok->span;
        span.col_end = peek(0).span.col_start; // approximate end
    
        return AST::Param { name_tok->lexeme, *type, type_name,name_tok->span };
    }

    std::optional<AST::FuncDecl> Parser::parse_func_decl(AST::ExprArena& arena,
                                                        std::vector<Diagnostic>& diags) {
        Lexer::SourceSpan decl_span = peek().span;

        bool is_extern = false;
        if (peek().kind == Lexer::Kind::Extern) {
            is_extern = true;
            advance();
        }
    
        if (!expect(Lexer::Kind::Func, diags))  return std::nullopt;
    
        auto name_tok = expect(Lexer::Kind::Ident, diags);
        if (!name_tok) return std::nullopt;
    
        if (!expect(Lexer::Kind::LParent, diags)) return std::nullopt;
    
        std::vector<AST::Param> params;
        if (peek().kind != Lexer::Kind::RParent) {
            auto param = parse_param(diags);
            if (!param) return std::nullopt;
            params.push_back(*param);
    
            while (peek().kind == Lexer::Kind::Comma) {
                advance();
                auto next = parse_param(diags);
                if (!next) return std::nullopt;
                params.push_back(*next);
            }
        }
        AST::Kind return_type = AST::Kind::Unknown;
        if (!expect(Lexer::Kind::RParent, diags)) return std::nullopt;
        // if (!expect(Lexer::Kind::Arrow,   diags)) return std::nullopt;
        std::string_view return_type_name;
        if (peek().kind == Lexer::Kind::Arrow) {
            advance(); // consume '->'
            auto rt = parse_type(diags, &return_type_name);
            if (!rt) return std::nullopt;
            return_type = *rt;
        } else {
            const Module::CachedFunc* f = module_table.find_func(name_tok->lexeme);
            if (f) {
                return_type = f->return_type;
            } else {
                Lexer::MakeError(&diags, peek().span,
                    "missing return type: expected '->' or a prior declaration in a .nh header");
                return std::nullopt;
            }
        }
        
        if (is_extern) {
            if (!expect(Lexer::Kind::Semicolon, diags)) return std::nullopt;
            AST::FuncDecl decl;
            decl.name        = name_tok->lexeme;
            decl.return_type = return_type;
            decl.params      = std::move(params);
            decl.span        = decl_span;
            decl.is_extern   = true;
            decl.return_type_name = return_type_name;
            return decl;
        }

        if (!expect(Lexer::Kind::LBrace, diags)) return std::nullopt;
    
        std::vector<AST::Stmt> body;
        while (peek().kind != Lexer::Kind::RBrace && !at_end()) {
            auto stmt = parse_stmt(arena, diags);
            if (!stmt) return std::nullopt;
            body.push_back(*stmt);
        }
    
        auto rbrace = expect(Lexer::Kind::RBrace, diags);
        if (!rbrace) return std::nullopt;
    
        decl_span.col_end = rbrace->span.col_end;
        return AST::FuncDecl {
            name_tok->lexeme,
            return_type,
            return_type_name,
            std::move(params),
            std::move(body),
            decl_span
        };
    }

    ParseResult Parser::parse() {
        ParseResult result;

        while (!at_end()) {
            if (peek().kind == Lexer::Kind::At) {
                handle_directive(result.diagnostics);
                continue;
            }

            if (peek().kind == Lexer::Kind::Struct) {
                auto decl = parse_struct_decl(result.diagnostics);
                if (decl) {
                    result.program.structs.push_back(std::move(*decl));
                } else break;
                continue;
            }

            if (peek().kind == Lexer::Kind::Func ||
                peek().kind == Lexer::Kind::Extern) {
                auto decl = parse_func_decl(result.expr_arena, result.diagnostics);
                if (decl) {
                    result.program.functions.push_back(std::move(*decl));
                } else break;
                continue;
            }

            Lexer::MakeError(&result.diagnostics, peek().span, "Expected 'func', 'ext func', 'struct', or '@directive' at top level");
            break;
        }
        return result;
    }
};