#include "lexer.hpp"
#include "module.hpp"
#include "parser.hpp"
#include "codegen.hpp"
#include <iostream>
#include <fstream>
#include <sstream>
#include <filesystem>
#include <llvm/Support/TargetSelect.h>

#include <stdexcept>
#include <execinfo.h>

// Helper to read the whole file into a string
std::string read_file(const std::string& path) {
    std::ifstream file(path);
    if (!file.is_open()) {
        throw std::runtime_error("Could not open file: " + path);
    }
    std::stringstream buffer;
    buffer << file.rdbuf();
    return buffer.str();
}

int main(int argc, char* argv[]) {
    // 1. Check for command line arguments
    if (argc < 2) {
        std::cerr << "Usage: novac <filename.nl>\n";
        argv[1] = "test.nl";
    }

    std::string input_path = argv[1];

    // 2. Initialize LLVM Targets
    LLVMInitializeAArch64TargetInfo();
    LLVMInitializeAArch64Target();
    LLVMInitializeAArch64TargetMC();
    LLVMInitializeAArch64AsmPrinter();

    try {
        // 3. Read the source code from file
        std::string source = read_file(input_path);

        // 4. Lexical Analysis
        // Note: You can now pass the string to your lexer
        Compiler::Lexer::Lexer lexer(source, 0);
        auto tokens = lexer.tokenize();

        printf("Lexer done\n");

        Compiler::Module::ModuleTable module_table;
        std::filesystem::path src_path = std::filesystem::absolute(input_path);
        // Print the module table

        // 5. Parsing
        Compiler::Parser::Parser parser(tokens, module_table, src_path);
        auto result = parser.parse();

        printf("Parser done\n");

        // If no errors, and -parser argument is passed, print the AST
        if (argc > 2 && std::string(argv[2]) == "-lexer") {
            std::cout << "--- Tokens ---\n";
            for (const auto& t : tokens) {
                std::cout << "Token: " << token_kind_name(t.kind);
                if (!t.lexeme.empty()) {
                    std::cout << " | Lexeme: '" << t.lexeme << "'";
                }
                std::cout << " [Line: " << t.span.line << ", Col: " << t.span.col_start << "]\n";
            }
        }

        if (!result.ok()) {
            for (auto& d : result.diagnostics)
                std::cerr << input_path << ":" << d.span.line << ":" << d.span.col_start
                          << " error: " << d.message << " [" << d.line << ":" << d.col << ", " << d.file << "]\n";
        }

        // 6. Codegen
        printf("Starting codegen\n");
        Compiler::Codegen::Codegen codegen(result.program, result.expr_arena, module_table);
        auto ir = codegen.emit();

        if (!ir.ok()) {
            for (auto& d : ir.diagnostics)
                std::cerr << input_path << ":" << d.span.line << ":" << d.span.col_start
                          << " error: " << d.message << "\n";
            return -1;
        }else {
            printf("Codegen done\n");
        }

        // 7. Output - Using the input filename for the output
        std::filesystem::path p(input_path);
        std::string output_name = p.stem().string() + ".ll";
        
        codegen.write_ll(output_name);
        std::cout << "Successfully compiled " << input_path << " -> " << output_name << "\n";

    } catch (const std::exception& e) {
        std::cerr << "Fatal error: " << e.what() << "\n";
        return 1;
    }

    return 0;
}