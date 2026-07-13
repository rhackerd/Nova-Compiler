#pragma once
#include "token.hpp"
#include <memory>
#include <mutex>
#include <utility>
#include <vector>


namespace Compiler::AST {
    enum class Kind : uint8_t {
        U8, U16, U32, U64,
        Bool,
        Struct,
        Unknown
    };

    constexpr std::string_view type_kind_name(Kind k) {
        switch (k) {
            case Kind::U8: return "u8";
            case Kind::U16: return "u16";
            case Kind::U32: return "u32";
            case Kind::U64: return "u64";
            case Kind::Bool: return "bool";
            case Kind::Struct: return "struct";
            default: return "<unknown>";
        }
        return "<invalid>";
    }

    enum class CompareOpKind : uint8_t {
        Eq, NotEq, Lt, Gt, LtEq, GtEq
    };

    enum class ExprKind {
        Lit_Int,
        Lit_Bool,
        BinaryOp,
        Identifier,
        Call,
        Lit_Struct,
        Field_Access,
        Compare
    };

    enum class BinaryOpKind {
        Add, Sub, Mul, Div
    };

    struct FieldInit {
        std::string_view name;
        uint32_t expr_idx = 0;
    };

    struct Expr { // Expressions
        ExprKind kind = ExprKind::Lit_Int;
        Lexer::SourceSpan span = {};

        uint64_t int_value = 0;
        // Identifier, Call, StructLiteral, FieldAccess
        std::string_view name = {};
        std::string_view callee = {};

        // BinaryOp
        BinaryOpKind op = {};
        uint32_t lhs_idx;
        uint32_t rhs_idx;
        // Compare
        CompareOpKind cmp_op = {};

        std::vector<uint32_t> args = {};

        std::vector<FieldInit> fields = {};


    };

    struct ExprArena {
        std::vector<Expr> nodes;

        uint32_t add(Expr e) {
            uint32_t idx = static_cast<uint32_t>(nodes.size());
            nodes.push_back(std::move(e));
            return idx;
        }

        const Expr& get(uint32_t idx) const { return nodes[idx]; }
        Expr& get(uint32_t idx) { return nodes[idx]; }
    };

    enum class StmtKind:uint8_t {
        Return, 
        VarDecl,
        Assign,
        Expr,
        FieldAssign,
        If,
        Loop,
        Break,
        Continue
    };

    struct Stmt { // Statement
        StmtKind kind = StmtKind::Return;
        Lexer::SourceSpan   span = {};
        uint32_t            expr_idx = 0;

        // VarDecl + Assign + FieldAssign
        std::string_view name = {};

        // VarDecl
        AST::Kind type = AST::Kind::U32;
        std::string_view type_name = {}; // for kind struct
        bool is_mut = false;
        std::string_view field = {};

        std::vector<Stmt> then_body = {};
        std::vector<Stmt> else_body = {};
    };

    struct Param {
        std::string_view    name;
        Kind                type;
        std::string_view type_name;
        Lexer::SourceSpan   span;
    };
    
    struct StructField {
        std::string_view name;
        Kind type;
        std::string_view type_name;
    };

    struct StructDecl {
        std::string_view name;
        std::vector<StructField> fields;
        Lexer::SourceSpan span;
    };

    struct FuncDecl {
        std::string_view    name;
        Kind                return_type;
        std::string_view    return_type_name;
        std::vector<Param>  params;
        std::vector<Stmt>   body;
        Lexer::SourceSpan   span;

        bool is_extern = false;
    };

    struct Program {
        std::vector<FuncDecl> functions;
        std::vector<StructDecl> structs;
    };
};