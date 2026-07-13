#include "codegen.hpp"
#include "ast.hpp"
#include "parser.hpp"
#include "token.hpp"

#include <llvm/IR/BasicBlock.h>
#include <llvm/IR/Constants.h>
#include <llvm/Support/raw_ostream.h>
#include <llvm/Support/FileSystem.h>

#include <string>
#include <string_view>
#include <unordered_map>

namespace Compiler::Codegen {
    Codegen::Codegen(const AST::Program& program,
                    const AST::ExprArena& arena,
                    const Module::ModuleTable& module_table,
                    const std::string& module_name)
        : program(program)
        , arena(arena)
        , module_table(module_table)
        , ctx(std::make_unique<llvm::LLVMContext>())
        , module(std::make_unique<llvm::Module>(module_name, *ctx))
        , builder(std::make_unique<llvm::IRBuilder<>>(*ctx))
    {}

    void Codegen::build_struct_registry() {
        // From program's own struct decls
        for (const auto& s : program.structs) {
            std::vector<llvm::Type*> field_types;
            for (const auto& f : s.fields)
                field_types.push_back(llvm_type(f.type)); // TODO: nested structs
            auto* st = llvm::StructType::create(*ctx, field_types,
                                                std::string(s.name));
            struct_types[std::string(s.name)] = st;
        }

        // From module table
        for (const auto& [mod_name, entry] : module_table.entries) {
            for (const auto& s : entry.structs) {
                if (struct_types.count(s.name)) continue; // already registered
                std::vector<llvm::Type*> field_types;
                for (uint8_t i = 0; i < s.field_count; i++)
                    field_types.push_back(llvm_type(s.fields[i].type));
                auto* st = llvm::StructType::create(*ctx, field_types, s.name);
                struct_types[s.name] = st;
            }
        }
    }

    llvm::StructType* Codegen::get_struct_type(std::string_view name) {
        auto it = struct_types.find(std::string(name));
        if (it != struct_types.end()) return it->second;
        return nullptr;
    }

    llvm::Type* Codegen::llvm_type(AST::Kind k) const {
        switch (k) {
            case AST::Kind::U8:   return llvm::Type::getInt8Ty(*ctx);
            case AST::Kind::U16:  return llvm::Type::getInt16Ty(*ctx);
            case AST::Kind::U32:  return llvm::Type::getInt32Ty(*ctx);
            case AST::Kind::U64:  return llvm::Type::getInt64Ty(*ctx);
            case AST::Kind::Bool: return llvm::Type::getInt1Ty(*ctx);
        }
        return llvm::Type::getInt32Ty(*ctx); // unreachable
    }

    llvm::Value* Codegen::emit_expr(uint32_t idx,
                                AST::Kind expected_type,
                                std::string_view expected_type_name,
                                std::unordered_map<std::string_view, SymbolEntry>& symbols,
                                std::vector<Parser::Diagnostic>& diags) {
        const AST::Expr& expr = arena.get(idx);

        switch (expr.kind) {
            case AST::ExprKind::Lit_Int: {
                return llvm::ConstantInt::get(llvm_type(expected_type), expr.int_value);
            }
            case AST::ExprKind::Lit_Bool: {
                // Always i1 — bool is not numeric, never coerce to expected_type
                return llvm::ConstantInt::get(llvm::Type::getInt1Ty(*ctx), expr.int_value);
            }
            case AST::ExprKind::Identifier: {
                auto it = symbols.find(expr.name);
                if (it == symbols.end()) {
                    diags.push_back({Lexer::DiagKind::Error, expr.span,
                        "undefined variable '" + std::string(expr.name) + "'"});
                    return nullptr;
                }
                if (it->second.alloca == nullptr)
                    return it->second.value; // param
                // load — for structs load the whole thing
                return builder->CreateLoad(
                    it->second.alloca->getAllocatedType(),
                    it->second.alloca,
                    std::string(expr.name));
            }
            case AST::ExprKind::Lit_Struct: {
                // expr.name = struct type name
                llvm::StructType* st = get_struct_type(expr.name);
                if (!st) {
                    diags.push_back({Lexer::DiagKind::Error, expr.span,
                        "undefined struct '" + std::string(expr.name) + "'"});
                    return nullptr;
                }

                // Find the struct decl to get field order + types
                // Check program first, then module table
                struct FieldInfo { std::string_view name; AST::Kind type; };
                std::vector<FieldInfo> field_order;

                for (const auto& sd : program.structs) {
                    if (sd.name == expr.name) {
                        for (const auto& f : sd.fields)
                            field_order.push_back({f.name, f.type});
                        break;
                    }
                }
                if (field_order.empty()) {
                    for (const auto& [mn, me] : module_table.entries) {
                        for (const auto& cs : me.structs) {
                            if (cs.name == expr.name) {
                                for (uint8_t i = 0; i < cs.field_count; i++)
                                    field_order.push_back({cs.fields[i].name, cs.fields[i].type});
                                break;
                            }
                        }
                    }
                }

                // Alloca a temporary, store each field
                llvm::AllocaInst* tmp = builder->CreateAlloca(st, nullptr, "struct_tmp");
                for (size_t i = 0; i < field_order.size(); i++) {
                    // Find matching initializer in expr.fields
                    llvm::Value* field_val = nullptr;
                    for (const auto& fi : expr.fields) {
                        if (fi.name == field_order[i].name) {
                            field_val = emit_expr(fi.expr_idx, field_order[i].type,
                                                {}, symbols, diags);
                            break;
                        }
                    }
                    if (!field_val) {
                        // field not provided in literal — zero init
                        field_val = llvm::Constant::getNullValue(
                            llvm_type(field_order[i].type));
                    }
                    llvm::Value* gep = builder->CreateStructGEP(st, tmp, i);
                    builder->CreateStore(field_val, gep);
                }
                // Load the whole struct to return as a value
                return builder->CreateLoad(st, tmp, "struct_val");
            }
            case AST::ExprKind::Field_Access: {
                // expr.lhs_idx = base, expr.name = field name
                const AST::Expr& base_expr = arena.get(expr.lhs_idx);

                if (base_expr.kind != AST::ExprKind::Identifier) {
                    diags.push_back({Lexer::DiagKind::Error, expr.span,
                        "field access only supported on variables (for now)"});
                    return nullptr;
                }

                auto it = symbols.find(base_expr.name);
                if (it == symbols.end()) {
                    diags.push_back({Lexer::DiagKind::Error, expr.span,
                        "undefined variable for field access"});
                    return nullptr;
                }

                llvm::AllocaInst* base_alloca = it->second.alloca;
                if (!base_alloca) {
                    diags.push_back({Lexer::DiagKind::Error, expr.span,
                        "cannot access field on a non-struct value"});
                    return nullptr;
                }

                // Get struct type from symbol's type_name
                std::string_view sname = it->second.type_name;
                llvm::StructType* st = get_struct_type(sname);
                if (!st) {
                    diags.push_back({Lexer::DiagKind::Error, expr.span,
                        "unknown struct type '" + std::string(sname) + "'"});
                    return nullptr;
                }

                // Find field index
                uint32_t field_idx = 0;
                bool found = false;

                for (const auto& sd : program.structs) {
                    if (sd.name != sname) continue;
                    for (size_t i = 0; i < sd.fields.size(); i++) {
                        if (sd.fields[i].name == expr.name) {
                            field_idx = i; found = true; break;
                        }
                    }
                }
                if (!found) {
                    for (const auto& [mn, me] : module_table.entries) {
                        for (const auto& cs : me.structs) {
                            if (cs.name != sname) continue;
                            for (uint8_t i = 0; i < cs.field_count; i++) {
                                if (cs.fields[i].name == expr.name) {
                                    field_idx = i; found = true; break;
                                }
                            }
                        }
                    }
                }
                if (!found) {
                    diags.push_back({Lexer::DiagKind::Error, expr.span,
                        "struct '" + std::string(sname) +
                        "' has no field '" + std::string(expr.name) + "'"});
                    return nullptr;
                }

                llvm::Value* gep = builder->CreateStructGEP(st, base_alloca, field_idx,
                                                            std::string(expr.name));
                // Get field type for load
                llvm::Type* field_type = st->getElementType(field_idx);
                return builder->CreateLoad(field_type, gep, std::string(expr.name));
            }

            case AST::ExprKind::BinaryOp: {
                llvm::Value* lhs_val = emit_expr(expr.lhs_idx, expected_type,
                                                expected_type_name, symbols, diags);
                llvm::Value* rhs_val = emit_expr(expr.rhs_idx, expected_type,
                                                expected_type_name, symbols, diags);
                if (!lhs_val || !rhs_val) return nullptr;
                switch (expr.op) {
                    case AST::BinaryOpKind::Add:  return builder->CreateAdd(lhs_val, rhs_val);
                    case AST::BinaryOpKind::Sub:  return builder->CreateSub(lhs_val, rhs_val);
                    case AST::BinaryOpKind::Mul:  return builder->CreateMul(lhs_val, rhs_val);
                    case AST::BinaryOpKind::Div:  return builder->CreateUDiv(lhs_val, rhs_val);
                }
            }
            case AST::ExprKind::Compare: {
                llvm::Value* lhs_val = emit_expr(expr.lhs_idx, AST::Kind::U64,
                                                {}, symbols, diags);
                llvm::Value* rhs_val = emit_expr(expr.rhs_idx, AST::Kind::U64,
                                                {}, symbols, diags);
                if (!lhs_val || !rhs_val) return nullptr;

                llvm::Value* cmp = nullptr;
                switch (expr.cmp_op) {
                    case AST::CompareOpKind::Eq:    cmp = builder->CreateICmpEQ(lhs_val, rhs_val);  break;
                    case AST::CompareOpKind::NotEq: cmp = builder->CreateICmpNE(lhs_val, rhs_val);  break;
                    case AST::CompareOpKind::Lt:    cmp = builder->CreateICmpULT(lhs_val, rhs_val); break;
                    case AST::CompareOpKind::Gt:    cmp = builder->CreateICmpUGT(lhs_val, rhs_val); break;
                    case AST::CompareOpKind::LtEq:  cmp = builder->CreateICmpULE(lhs_val, rhs_val); break;
                    case AST::CompareOpKind::GtEq:  cmp = builder->CreateICmpUGE(lhs_val, rhs_val); break;
                }

                // Returns i1 — callers that need a wider type must cast explicitly.
                // The if-condition handler uses i1 directly, no roundtrip needed.
                return cmp;
            }
            case AST::ExprKind::Call: {
                llvm::Function* callee = module->getFunction(std::string(expr.name));

                // Not found locally — check module table and emit a declaration
                if (!callee) {
                    const auto* cached = module_table.find_func(expr.name);
                    if (!cached) {
                        diags.push_back(Lexer::Diagnostic {
                            Lexer::DiagKind::Error, expr.span,
                            "undefined function '" + std::string(expr.name) + "'"
                        });
                        return nullptr;
                    }

                    // Emit forward declaration from cached signature
                    std::vector<llvm::Type*> param_types;
                    for (uint8_t i = 0; i < cached->param_count; i++)
                        param_types.push_back(llvm_type(cached->params[i].type));

                    llvm::FunctionType* fn_type = llvm::FunctionType::get(
                        llvm_type(cached->return_type), param_types, false);

                    callee = llvm::Function::Create(
                        fn_type,
                        llvm::Function::ExternalLinkage,
                        cached->name,
                        *module

                    );
                }

                // arg count check
                if (callee->arg_size() != expr.args.size()) {
                    diags.push_back(Lexer::Diagnostic {
                        Lexer::DiagKind::Error, expr.span,
                        "function '" + std::string(expr.name) + "' expects " +
                        std::to_string(callee->arg_size()) + " arguments but got " +
                        std::to_string(expr.args.size())
                    });
                    return nullptr;
                }

                std::vector<llvm::Value*> arg_vals;
                arg_vals.reserve(expr.args.size());
                size_t i = 0;
                for (auto& param : callee->args()) {
                    llvm::Value* v = emit_expr(expr.args[i++], expected_type, expected_type_name,symbols, diags);
                    if (!v) return nullptr;
                    arg_vals.push_back(v);
                }

                return builder->CreateCall(callee, arg_vals, "call");
            }
        }

        return nullptr;
    }

    void Codegen::emit_stmt(const AST::Stmt& stmt,
                        AST::Kind return_type,
                        std::string_view return_type_name,
                        std::unordered_map<std::string_view, SymbolEntry>& symbols,
                        std::vector<Parser::Diagnostic>& diags,
                        llvm::BasicBlock* break_target,
                        llvm::BasicBlock* continue_target) {
        switch (stmt.kind) {
            case AST::StmtKind::Return: {
                llvm::Value* val = emit_expr(stmt.expr_idx, return_type,{}, symbols, diags);
                if (val) builder->CreateRet(val);
                break;
            }

            case AST::StmtKind::VarDecl: {
                llvm::Type* ty = nullptr;
                if (stmt.type == AST::Kind::Struct) {
                    ty = get_struct_type(stmt.type_name);
                    if (!ty) {
                        diags.push_back({Lexer::DiagKind::Error, stmt.span,
                            "undefined struct type '" + std::string(stmt.type_name) + "'"});
                        return;
                    }
                } else {
                    ty = llvm_type(stmt.type);
                }

                llvm::AllocaInst* alloc = builder->CreateAlloca(ty, nullptr,
                                                                std::string(stmt.name));
                llvm::Value* init = emit_expr(stmt.expr_idx, stmt.type,
                                            stmt.type_name, symbols, diags);
                if (!init) return;
                builder->CreateStore(init, alloc);
                symbols[stmt.name] = SymbolEntry { alloc, nullptr, stmt.is_mut, stmt.type_name };
                break;
            }
            case AST::StmtKind::FieldAssign: {
                auto it = symbols.find(stmt.name);
                if (it == symbols.end()) {
                    diags.push_back({Lexer::DiagKind::Error, stmt.span,
                        "undefined variable '" + std::string(stmt.name) + "'"});
                    return;
                }
                if (!it->second.is_mut) {
                    diags.push_back({Lexer::DiagKind::Error, stmt.span,
                        "cannot assign field of immutable variable '" +
                        std::string(stmt.name) + "'"});
                    return;
                }

                std::string_view sname = it->second.type_name;
                llvm::StructType* st = get_struct_type(sname);
                if (!st) {
                    diags.push_back({Lexer::DiagKind::Error, stmt.span,
                        "unknown struct type '" + std::string(sname) + "'"});
                    return;
                }

                // Find field index
                uint32_t field_idx = 0;
                bool found = false;
                AST::Kind field_type_kind = AST::Kind::U64;

                for (const auto& sd : program.structs) {
                    if (sd.name != sname) continue;
                    for (size_t i = 0; i < sd.fields.size(); i++) {
                        if (sd.fields[i].name == stmt.field) {
                            field_idx = i;
                            field_type_kind = sd.fields[i].type;
                            found = true; break;
                        }
                    }
                }
                if (!found) {
                    for (const auto& [mn, me] : module_table.entries) {
                        for (const auto& cs : me.structs) {
                            if (cs.name != sname) continue;
                            for (uint8_t i = 0; i < cs.field_count; i++) {
                                if (cs.fields[i].name == stmt.field) {
                                    field_idx = i;
                                    field_type_kind = cs.fields[i].type;
                                    found = true; break;
                                }
                            }
                        }
                    }
                }
                if (!found) {
                    diags.push_back({Lexer::DiagKind::Error, stmt.span,
                        "struct '" + std::string(sname) +
                        "' has no field '" + std::string(stmt.field) + "'"});
                    return;
                }

                llvm::Value* val = emit_expr(stmt.expr_idx, field_type_kind, {}, symbols, diags);
                if (!val) return;
                llvm::Value* gep = builder->CreateStructGEP(st, it->second.alloca, field_idx);
                builder->CreateStore(val, gep);
                break;
            }
            case AST::StmtKind::Expr: {
                emit_expr(stmt.expr_idx, return_type, {}, symbols, diags);
                break;
            }

            case AST::StmtKind::Assign: {
                auto it = symbols.find(stmt.name);
                if (it == symbols.end()) {
                    diags.push_back(Parser::Diagnostic { Lexer::DiagKind::Error, stmt.span,
                        "undefined variable '" + std::string(stmt.name) + "'" });
                    return;
                }
                if (!it->second.is_mut) {
                    diags.push_back(Parser::Diagnostic { Lexer::DiagKind::Error, stmt.span,
                        "cannot assign to immutable variable '" + std::string(stmt.name) + "'" });
                    return;
                }
                llvm::Value* val = emit_expr(stmt.expr_idx, return_type, {}, symbols, diags);
                if (!val) return;
                builder->CreateStore(val, it->second.alloca);
                break;
            }
            case AST::StmtKind::If: {
                llvm::Value* cond = emit_expr(stmt.expr_idx, AST::Kind::Bool, {},
                                            symbols, diags);
                if (!cond) return;

                // cond is i1 directly — compare returns i1, bool literals are i1
                llvm::Value* cond_i1 = cond;

                llvm::Function* fn        = builder->GetInsertBlock()->getParent();
                llvm::BasicBlock* then_bb = llvm::BasicBlock::Create(*ctx, "then", fn);
                llvm::BasicBlock* else_bb = llvm::BasicBlock::Create(*ctx, "else", fn);
                llvm::BasicBlock* merge_bb = llvm::BasicBlock::Create(*ctx, "merge", fn);

                builder->CreateCondBr(cond_i1, then_bb, stmt.else_body.empty() ? merge_bb : else_bb);

                // then block
                builder->SetInsertPoint(then_bb);
                bool then_terminated = false;
                for (const auto& s : stmt.then_body) {
                    emit_stmt(s, return_type, return_type_name, symbols, diags,
                            break_target, continue_target);
                    if (!diags.empty()) return;
                    if (s.kind == AST::StmtKind::Return ||
                        s.kind == AST::StmtKind::Break  ||
                        s.kind == AST::StmtKind::Continue) {
                        then_terminated = true; break;
                    }
                }
                if (!then_terminated) builder->CreateBr(merge_bb);

                // else block
                builder->SetInsertPoint(else_bb);
                bool else_terminated = false;
                if (!stmt.else_body.empty()) {
                    for (const auto& s : stmt.else_body) {
                        emit_stmt(s, return_type, return_type_name, symbols, diags,
                                break_target, continue_target);
                        if (!diags.empty()) return;
                        if (s.kind == AST::StmtKind::Return ||
                            s.kind == AST::StmtKind::Break  ||
                            s.kind == AST::StmtKind::Continue) {
                            else_terminated = true; break;
                        }
                    }
                    if (!else_terminated) builder->CreateBr(merge_bb);
                } else {
                    builder->CreateBr(merge_bb);
                }

                builder->SetInsertPoint(merge_bb);
                break;
            }

            case AST::StmtKind::Loop: {
                llvm::Function*   fn        = builder->GetInsertBlock()->getParent();
                llvm::BasicBlock* loop_bb   = llvm::BasicBlock::Create(*ctx, "loop",  fn);
                llvm::BasicBlock* exit_bb   = llvm::BasicBlock::Create(*ctx, "loop_exit", fn);

                builder->CreateBr(loop_bb);
                builder->SetInsertPoint(loop_bb);

                bool body_terminated = false;
                for (const auto& s : stmt.then_body) {
                    emit_stmt(s, return_type, return_type_name, symbols, diags,
                            exit_bb, loop_bb);  // break→exit, continue→loop
                    if (!diags.empty()) return;
                    if (s.kind == AST::StmtKind::Return ||
                        s.kind == AST::StmtKind::Break  ||
                        s.kind == AST::StmtKind::Continue) {
                        body_terminated = true; break;
                    }
                }
                if (!body_terminated) builder->CreateBr(loop_bb); // back-edge

                builder->SetInsertPoint(exit_bb);
                break;
            }

            case AST::StmtKind::Break: {
                if (!break_target) {
                    diags.push_back({Lexer::DiagKind::Error, stmt.span,
                        "break outside of loop"});
                    return;
                }
                builder->CreateBr(break_target);
                break;
            }

            case AST::StmtKind::Continue: {
                if (!continue_target) {
                    diags.push_back({Lexer::DiagKind::Error, stmt.span,
                        "continue outside of loop"});
                    return;
                }
                builder->CreateBr(continue_target);
                break;
            }
        }
    }


    llvm::Function* Codegen::emit_func(const AST::FuncDecl& func,
                                    std::vector<Parser::Diagnostic>& diags) {
        // Build LLVM param type list
        std::vector<llvm::Type*> param_types;
        param_types.reserve(func.params.size());
        for (const auto& p : func.params) {
            if (p.type == AST::Kind::Struct) {
                llvm::StructType* st = get_struct_type(p.type_name);
                if (!st) {
                    diags.push_back({Lexer::DiagKind::Error, func.span,
                        "unknown struct type '" + std::string(p.type_name) + "' in param"});
                    return nullptr;
                }
                param_types.push_back(st);
            } else {
                param_types.push_back(llvm_type(p.type));
            }
        }

        
    
        // Function type: rettype (param, param, ...)
        llvm::FunctionType* fn_type = llvm::FunctionType::get(
            llvm_type(func.return_type),
            param_types,
            /*isVarArg=*/false
        );
    
        // Create the function in the module with external linkage
        llvm::Function* fn = llvm::Function::Create(
            fn_type,
            llvm::Function::ExternalLinkage,
            std::string(func.name),
            *module
        );

        

        if (func.is_extern) return fn;
    
        // Name the arguments — makes IR readable and helps debuggers
        size_t i = 0;
        for (auto& arg : fn->args())
            arg.setName(std::string(func.params[i++].name));
    
        // Entry basic block
        llvm::BasicBlock* entry = llvm::BasicBlock::Create(*ctx, "entry", fn);
        builder->SetInsertPoint(entry);
    
        std::unordered_map<std::string_view, SymbolEntry> symbols;
        for (size_t i = 0; i < func.params.size(); i++) {
            auto& arg = *std::next(fn->arg_begin(), i);
            const auto& param = func.params[i];

            if (param.type == AST::Kind::Struct) {
                // Struct params: alloca + store so field access via GEP works
                llvm::StructType* st = get_struct_type(param.type_name);
                if (!st) {
                    diags.push_back({Lexer::DiagKind::Error, func.span,
                        "unknown struct type '" + std::string(param.type_name) + "' in param"});
                    fn->eraseFromParent();
                    return nullptr;
                }
                llvm::AllocaInst* slot = builder->CreateAlloca(st, nullptr,
                                                                std::string(param.name));
                builder->CreateStore(&arg, slot);
                symbols[param.name] = SymbolEntry { slot, nullptr, false, param.type_name };
            } else {
                // Primitive params: direct value, no alloca needed
                symbols[param.name] = SymbolEntry { nullptr, &arg, false, {} };
            }
        }

        // Emit body statements
        // Only return statements exist for now.
        bool has_return = false;
        bool block_terminated = false;
        for (const auto& stmt : func.body) {
            if (stmt.kind == AST::StmtKind::Return) has_return = true;

            // If both branches terminate, the merge block is the new current block
            // and we don't need unreachable — but we also don't need a ret
            // Just track that we emitted a terminator at the top level
            if (stmt.kind == AST::StmtKind::If &&
                !stmt.then_body.empty() &&
                !stmt.else_body.empty()) {
                // Both branches present — may fully terminate, handle below
                has_return = true; // optimistic — verifyFunction will catch bad cases
            }

            emit_stmt(stmt, func.return_type, func.return_type_name, symbols, diags,
                    nullptr, nullptr);
            if (!diags.empty()) { fn->eraseFromParent(); return nullptr; }

            if (stmt.kind == AST::StmtKind::Return  ||
                stmt.kind == AST::StmtKind::Break   ||
                stmt.kind == AST::StmtKind::Continue) {
                block_terminated = true;
                has_return = true;
                break;
            }
        }

        if (!has_return)
            builder->CreateUnreachable();
    
        // Verify the function — catches malformed IR immediately.
        // This is the main reason to use the API over textual IR.
        std::string err;
        llvm::raw_string_ostream err_stream(err);
        if (llvm::verifyFunction(*fn, &err_stream)) {
            Lexer::SourceSpan span = func.span;
            diags.push_back(Parser::Diagnostic {
                Lexer::DiagKind::Error,
                span,
                "LLVM IR verification failed for '" + std::string(func.name) + "': " + err
            });
            fn->eraseFromParent();
            return nullptr;
        }
    
        return fn;
    }

    CodegenResult Codegen::emit() {
        CodegenResult result;

        build_struct_registry();
    
        for (const auto& func : program.functions) {
            emit_func(func, result.diagnostics);
            if (!result.ok()) break; // stop on first bad function
        }
    
        // Verify the whole module if all functions passed
        if (result.ok()) {
            std::string err;
            llvm::raw_string_ostream err_stream(err);
            if (llvm::verifyModule(*module, &err_stream)) {
                result.diagnostics.push_back(Parser::Diagnostic {
                    Lexer::DiagKind::Error,
                    Lexer::SourceSpan{},
                    "LLVM module verification failed: " + err
                });
            }
        }
    
        return result;
    }

    bool Codegen::write_ll(const std::string& path) const {
        std::error_code ec;
        llvm::raw_fd_ostream out(path, ec, llvm::sys::fs::OF_Text);
        if (ec) return false;
        module->print(out, nullptr);
        return true;
    }
};