#pragma once
#include "ast.hpp"
#include "module.hpp"
#include "parser.hpp"
#include "token.hpp"

#include <llvm/IR/DerivedTypes.h>
#include <llvm/IR/LLVMContext.h>
#include <llvm/IR/Module.h>
#include <llvm/IR/IRBuilder.h>
#include <llvm/IR/Function.h>
#include <llvm/IR/Type.h>
#include <llvm/IR/Value.h>
#include <llvm/IR/Verifier.h>

#include <unordered_map>
#include <vector>
#include <string>
#include <memory>

namespace Compiler::Codegen {
    struct CodegenResult {
        std::vector<Parser::Diagnostic> diagnostics;
        bool ok() const { return diagnostics.empty();}
    };

    struct SymbolEntry {
        llvm::AllocaInst* alloca;
        llvm::Value* value;
        bool is_mut;
        std::string_view type_name;
    };

    struct Codegen {
        explicit Codegen(const AST::Program& program,
                        const AST::ExprArena& arena,
                        const Module::ModuleTable& module_table,
                        const std::string& module_name = "nova_module");

        CodegenResult emit();

        bool write_ll(const std::string& path) const;
        
        llvm::Module& get_module() { return *module; }

        private:
            const AST::Program& program;
            const AST::ExprArena& arena;
            const Module::ModuleTable& module_table;

            std::unordered_map<std::string, llvm::StructType*> struct_types;

            llvm::StructType* get_struct_type(std::string_view name);
            void build_struct_registry();

            std::unique_ptr<llvm::LLVMContext> ctx;
            std::unique_ptr<llvm::Module> module;
            std::unique_ptr<llvm::IRBuilder<>> builder;

            llvm::Type* llvm_type(AST::Kind k) const;
            llvm::Value* emit_expr(uint32_t idx,
                                AST::Kind expected_type,
                                std::string_view expected_type_name,
                                std::unordered_map<std::string_view, SymbolEntry>& symbols,
                                std::vector<Parser::Diagnostic>& diags);
            void emit_stmt(const AST::Stmt& stmt,
                        AST::Kind return_type,
                        std::string_view return_type_name,
                        std::unordered_map<std::string_view, SymbolEntry>& symbols,
                        std::vector<Parser::Diagnostic>& diags,
                        llvm::BasicBlock* break_target    = nullptr,
                        llvm::BasicBlock* continue_target = nullptr);
            llvm::Function* emit_func(const AST::FuncDecl& func, std::vector<Parser::Diagnostic>& diags);
    };
};