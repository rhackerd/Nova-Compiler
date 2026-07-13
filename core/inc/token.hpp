#pragma once

#include <cstdint>
#include <string_view>
#include <string>
#include <vector>
#include <source_location>


namespace Compiler::Lexer {
    enum class DiagKind {Error, Warning};

    struct SourceSpan {
        uint32_t line;
        uint32_t col_start;
        uint32_t col_end;
        uint16_t file_id;
    };

    struct Diagnostic {
        DiagKind kind;
        Lexer::SourceSpan span;
        std::string message;
        size_t line = 0;
        size_t col = 0;
        std::string file;
    };

    inline void MakeError(std::vector<Diagnostic>* diags, 
                        Lexer::SourceSpan span, 
                        std::string msg, 
                        const std::source_location location = std::source_location::current()) {
        
        diags->push_back(Diagnostic { 
            Lexer::DiagKind::Error, 
            span, 
            std::move(msg), 
            location.line(), 
            location.column() ,
            location.file_name()
        });
    }

    enum class Kind {
        // Keywords
        Func,   // func
        Ret,    // ret
        Let,    // let
        Mut,    // mut
        Assign, // =
        Extern, // ext
        Struct, // struct
        If,
        Else,
        Loop,
        Break,
        Continue,

        True,
        False,

        Void,
        U8,
        U16,
        U32,
        U64,
        Bool,

        LParent,    // (
        RParent,    // )
        LBrace,     // {
        RBrace,     // }
        Arrow,      // ->
        Semicolon,  // ;
        Comma,      // ,
        Colon,      // :
        At,         // @
        Dot,        // .
        EqEq,       // ==
        NotEq,      // !=
        Lt,         // <
        Gt,         // >
        LtEq,       // <=
        GtEq,       // >=
        PlusEq,     // +=
        MinusEq,    // -=
        StarEq,     // *=
        SlashEq,    // /=

        Plus,       // +
        Minus,      // -
        Star,       // *
        Slash,      // /
        
        Lit_Int,    // 123
        Lit_Str,    // "..."

        Ident,
        End,
        Unknown
    };

    struct Token {
        Kind kind;
        std::string_view lexeme;
        SourceSpan span;
    };

    constexpr std::string_view token_kind_name(Kind kind) {
        switch (kind) {
            // Keywords
            case Kind::Func: return "func";
            case Kind::Ret: return "ret";
            case Kind::Let: return "let";
            case Kind::Mut: return "mut";
            case Kind::Extern: return "extern";
            case Kind::Struct: return "struct";
            case Kind::If: return "if";
            case Kind::Else: return "else";
            case Kind::Loop: return "loop";
            case Kind::Break: return "break";
            case Kind::Continue: return "continue";

            case Kind::True: return "true";
            case Kind::False: return "false";

            case Kind::At: return "@";
            case Kind::Assign: return "=";
            case Kind::Void: return "void";
            case Kind::U8: return "u8";
            case Kind::U16: return "u16";
            case Kind::U32: return "u32";
            case Kind::U64: return "u64";
            case Kind::Bool: return "bool";
            case Kind::LParent: return "(";
            case Kind::RParent: return ")";
            case Kind::LBrace: return "{";
            case Kind::RBrace: return "}";
            case Kind::Arrow: return "->";
            case Kind::Semicolon: return ";";
            case Kind::Comma: return ",";
            case Kind::Colon: return ":";
            case Kind::Plus: return "+";
            case Kind::Minus: return "-";
            case Kind::Star: return "*";
            case Kind::Slash: return "/";
            case Kind::Dot: return ".";
            case Kind::EqEq: return "==";
            case Kind::NotEq: return "!=";
            case Kind::Lt: return "<";
            case Kind::Gt: return ">";
            case Kind::LtEq: return "<=";
            case Kind::GtEq: return ">=";
            case Kind::PlusEq: return "+=";
            case Kind::MinusEq: return "-=";
            case Kind::StarEq: return "*=";
            case Kind::SlashEq: return "/=";
            
            
            case Kind::Lit_Int: return "<int>";
            case Kind::Lit_Str: return "<str>";
            case Kind::Ident:   return "<ident>";
            case Kind::End:     return "<eof>";
            case Kind::Unknown: return "<unknown>";
            default:            return "<unknown>";
        }
        return "<invalid>";
    }
};