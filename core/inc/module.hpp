#pragma once
#include "ast.hpp"
#include <token.hpp>
#include <string>
#include <string_view>
#include <vector>
#include <unordered_map>
#include <filesystem>

namespace Compiler::Module {

struct CachedParam {
    char      name[64]  = {};
    AST::Kind type      = {};
    char type_name[64]  = {};
};

struct CachedFunc {
    char        name[64]        = {};
    AST::Kind   return_type     = {};
    uint8_t     param_count     = 0;
    CachedParam params[16]      = {};
    bool        is_extern       = false;
};

struct CachedStruct {
    char        name[64]        = {};
    uint8_t     field_count     = 0;
    CachedParam fields[32]      = {};
};

struct ModuleEntry {
    std::string logical_name;       // from @name directive, or filename stem
    std::filesystem::path source_path;

    std::vector<CachedFunc>   funcs;
    std::vector<CachedStruct> structs;

    // lookup helpers
    const CachedFunc*   find_func(std::string_view name) const;
    const CachedStruct* find_struct(std::string_view name) const;
};


struct ModuleTable {
    std::unordered_map<std::string, ModuleEntry> entries;

    std::vector<std::string> processing_stack;

    const ModuleEntry* load(const std::string& name_or_path,
                            const std::filesystem::path& from_file,
                            std::vector<Lexer::Diagnostic>& diags);

    const CachedFunc* find_func(std::string_view name) const;
    const CachedStruct* find_struct(std::string_view name) const;

private:
    const ModuleEntry* parse_and_register(const std::filesystem::path& path,
                                          std::vector<Lexer::Diagnostic>& diags);
};

} 