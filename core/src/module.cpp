#include "module.hpp"
#include "lexer.hpp"
#include <fstream>
#include <sstream>
#include <cstring>

namespace Compiler::Module {

// ------------------------------------------------------------------ //
//  ModuleEntry lookup helpers                                         //
// ------------------------------------------------------------------ //

const CachedFunc* ModuleEntry::find_func(std::string_view name) const {
    for (const auto& f : funcs)
        if (f.name == name) return &f;
    return nullptr;
}

const CachedStruct* ModuleEntry::find_struct(std::string_view name) const {
    for (const auto& s : structs)
        if (s.name == name) return &s;
    return nullptr;
}

// ------------------------------------------------------------------ //
//  ModuleTable global lookup                                          //
// ------------------------------------------------------------------ //

const CachedFunc* ModuleTable::find_func(std::string_view name) const {
    for (const auto& [key, entry] : entries) {
        if (auto* f = entry.find_func(name)) return f;
    }
    return nullptr;
}

const CachedStruct* ModuleTable::find_struct(std::string_view name) const {
    for (const auto& [key, entry] : entries) {
        if (auto* s = entry.find_struct(name)) return s;
    }
    return nullptr;
}

// ------------------------------------------------------------------ //
//  .nh file parser                                                    //
//  Minimal — only declarations, no bodies.                           //
//  Handles: @name, ext func, func (signature only), struct           //
// ------------------------------------------------------------------ //

static AST::Kind parse_kind(std::string_view s) {
    if (s == "u8")   return AST::Kind::U8;
    if (s == "u16")  return AST::Kind::U16;
    if (s == "u32")  return AST::Kind::U32;
    if (s == "u64")  return AST::Kind::U64;
    if (s == "bool") return AST::Kind::Bool;
    if (s == "i8")   return AST::Kind::U8;  // placeholder until signed types land
    if (s == "i16") return AST::Kind::U16;
    if (s == "i32") return AST::Kind::U32;
    if (s == "i64") return AST::Kind::U64;
    return AST::Kind::Struct; // fallback // TODO: make this flag as a hard error instead of fallback
}

static void copy_name(char* dst, std::string_view src, size_t max) {
    size_t len = std::min(src.size(), max - 1);
    std::memcpy(dst, src.data(), len);
    dst[len] = '\0';
}

const ModuleEntry* ModuleTable::parse_and_register(
    const std::filesystem::path& path,
    std::vector<Lexer::Diagnostic>& diags)
{
    // Read file
    std::ifstream file(path);
    if (!file.is_open()) {
        diags.push_back(Lexer::Diagnostic {
            Lexer::DiagKind::Error, {},
            "cannot open header file: " + path.string()
        });
        return nullptr;
    }
    std::stringstream buf;
    buf << file.rdbuf();
    std::string source = buf.str();

    // Lex
    Compiler::Lexer::Lexer lexer(source, 0);
    auto tokens = lexer.tokenize();

    // Build entry
    ModuleEntry entry;
    entry.source_path = path;
    entry.logical_name = path.stem().string(); // default: filename stem

    size_t cursor = 0;
    auto peek = [&](size_t dist = 0) -> const Lexer::Token& {
        size_t idx = cursor + dist;
        if (idx >= tokens.size()) return tokens.back();
        return tokens[idx];
    };
    auto advance = [&]() -> const Lexer::Token& {
        return tokens[cursor < tokens.size() - 1 ? cursor++ : cursor];
    };
    auto expect_kind = [&](Lexer::Kind k) -> bool {
        if (peek().kind == k) { advance(); return true; }
        return false;
    };

    while (peek().kind != Lexer::Kind::End) {
        // @name directive
        if (peek().kind == Lexer::Kind::At) {
            advance(); // consume '@'
            if (peek().lexeme == "name") {
                advance(); // consume 'name'
                if (peek().kind == Lexer::Kind::Ident) {
                    entry.logical_name = std::string(peek().lexeme);
                    advance();
                }
            } else if (peek().lexeme == "module") {
                advance(); // consume 'module'
                // Header depending on another header
                if (peek().kind == Lexer::Kind::Ident ||
                    peek().kind == Lexer::Kind::Lit_Str) {
                    std::string dep = std::string(peek().lexeme);
                    advance();
                    load(dep, path, diags);
                }
            } else {
                advance(); // unknown directive, skip
            }
            continue;
        }

        // ext func name(params) -> type;
        bool is_extern = false;
        if (peek().kind == Lexer::Kind::Extern) {
            is_extern = true;
            advance();
        }

        if (peek().kind == Lexer::Kind::Func) {
            advance(); // consume 'func'

            if (peek().kind != Lexer::Kind::Ident) { advance(); continue; }
            std::string_view fname = peek().lexeme;
            advance();

            if (!expect_kind(Lexer::Kind::LParent)) continue;

            CachedFunc func;
            copy_name(func.name, fname, 64);
            func.is_extern = is_extern;

            // params
            while (peek().kind != Lexer::Kind::RParent &&
                   peek().kind != Lexer::Kind::End) {
                if (peek().kind != Lexer::Kind::Ident) { advance(); continue; }
                std::string_view pname = peek().lexeme;
                advance();
                if (!expect_kind(Lexer::Kind::Colon)) break;
                std::string_view type_lexeme = peek().lexeme;
                AST::Kind ptype = parse_kind(type_lexeme);
                advance();

                if (func.param_count < 16) {
                    auto& p = func.params[func.param_count++];
                    copy_name(p.name, pname, 64);
                    p.type = ptype;
                    if (ptype == AST::Kind::Struct)
                        copy_name(p.type_name, type_lexeme, 64);
                } else {
                    diags.push_back(Lexer::Diagnostic {
                        Lexer::DiagKind::Error, {},
                        std::string("function '") + func.name +
                        "' exceeds 16 parameter limit — redesign recommended"
                    });
                }

                if (peek().kind == Lexer::Kind::Comma) advance();
            }

            expect_kind(Lexer::Kind::RParent);
            expect_kind(Lexer::Kind::Arrow);

            func.return_type = parse_kind(peek().lexeme);
            advance();

            expect_kind(Lexer::Kind::Semicolon);
            entry.funcs.push_back(func);
            continue;
        }

        // struct name { fields }
        if (peek().kind == Lexer::Kind::Struct) {
            advance(); // consume 'struct'

            if (peek().kind != Lexer::Kind::Ident) { advance(); continue; }
            std::string_view sname = peek().lexeme;
            advance();

            if (!expect_kind(Lexer::Kind::LBrace)) continue;

            CachedStruct s;
            copy_name(s.name, sname, 64);

            while (peek().kind != Lexer::Kind::RBrace &&
                   peek().kind != Lexer::Kind::End) {
                if (peek().kind != Lexer::Kind::Ident) { advance(); continue; }
                std::string_view fname = peek().lexeme;
                advance();
                if (!expect_kind(Lexer::Kind::Colon)) break;
                AST::Kind ftype = parse_kind(peek().lexeme);
                advance();
                expect_kind(Lexer::Kind::Semicolon);

                if (s.field_count < 32) {
                    auto& f = s.fields[s.field_count++];
                    copy_name(f.name, fname, 64);
                    f.type = ftype;
                } else {
                    diags.push_back(Lexer::Diagnostic {
                        Lexer::DiagKind::Error, {},
                        std::string("struct '") + s.name +
                        "' exceeds 32 field limit — redesign recommended"
                    });
                }
            }

            expect_kind(Lexer::Kind::RBrace);
            entry.structs.push_back(s);
            continue;
        }

        advance(); // skip unrecognized token
    }

    // Check for duplicate logical name
    if (entries.count(entry.logical_name)) {
        diags.push_back(Lexer::Diagnostic {
            Lexer::DiagKind::Error, {},
            "duplicate module name '" + entry.logical_name + "': already loaded from " +
            entries[entry.logical_name].source_path.string() +
            ", conflicts with " + path.string()
        });
        return nullptr;
    }

    auto& stored = entries[entry.logical_name] = std::move(entry);
    return &stored;
}

// ------------------------------------------------------------------ //
//  ModuleTable::load                                                  //
// ------------------------------------------------------------------ //

const ModuleEntry* ModuleTable::load(
    const std::string& name_or_path,
    const std::filesystem::path& from_file,
    std::vector<Lexer::Diagnostic>& diags)
{
    // 1. Already loaded by logical name?
    auto it = entries.find(name_or_path);
    if (it != entries.end()) return &it->second;

    // 2. Circular dependency check
    for (const auto& s : processing_stack) {
        if (s == name_or_path) {
            diags.push_back(Lexer::Diagnostic {
                Lexer::DiagKind::Error, {},
                "circular module dependency detected: '" + name_or_path + "'"
            });
            return nullptr;
        }
    }

    // 3. Resolve path relative to from_file's directory
    std::filesystem::path base = from_file.parent_path();
    std::filesystem::path candidate = base / name_or_path;

    // Append .nh if no extension given
    if (!candidate.has_extension())
        candidate += ".nh";

    if (!std::filesystem::exists(candidate)) {
        diags.push_back(Lexer::Diagnostic {
            Lexer::DiagKind::Error, {},
            "cannot find module '" + name_or_path + "' (looked for: " +
            candidate.string() + ")"
        });
        return nullptr;
    }

    // 4. Parse it
    processing_stack.push_back(name_or_path);
    auto* result = parse_and_register(candidate, diags);
    processing_stack.pop_back();

    return result;
}

} // namespace Compiler::Module