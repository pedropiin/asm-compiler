#pragma once

#include <vector>
#include <string>
#include <sstream>
#include <unordered_map>

#include "parser.hpp"


struct Variable {
    size_t stack_pos;
};


class Generator {
    public:
        inline Generator(node::Program program) 
            : m_program(std::move(program))
        {}


        inline void generate_expr(const node::Expr& expr) {
            struct ExprVisitor {
                Generator* generator;
                void operator()(const node::ExprIntLit& expr_int_lit) {
                    generator->m_out << "    mov rax, " << expr_int_lit.token_int.valor.value() << '\n';
                    generator->push("rax");
                }
                void operator()(const node::ExprIdentif& expr_identif) {
                    if (generator->m_variables.contains(expr_identif.token_identif.valor.value())) {
                        auto& var = generator->m_variables.at(expr_identif.token_identif.valor.value());
                        std::stringstream offset;
                        offset << "QWORD [rsp + " << (generator->m_stack_size - var.stack_pos) * 4 << "]\n"; //stack e registrador %rsp crescem pra baixo. 
                    } else {
                        std::cerr << "Identificador '" << expr_identif.token_identif.valor.value() << "' não inicializado." << std::endl;
                        exit(EXIT_FAILURE);
                    }
                }
            };

            ExprVisitor visitor {.generator = this};
            std::visit(visitor, expr.variant_expr);
        } 

        /*
        
        */
        inline void generate_statmt(const node::Statmt& statmt) {
            struct StatmtVisitor {
                Generator* generator;
                void operator()(const node::StatmtExit& statmt_exit) {
                    generator->generate_expr(statmt_exit.expr);
                    generator->m_out << "    mov rax, 60\n";
                    generator->pop("rdi");
                    generator->m_out << "    syscall\n";
                }
                void operator()(const node::StatmtVar& statmt_var) {
                    // if (!generator->m_variables.contains(statmt_var.token_identif.valor.value())) {
                    //     std::cout << "problema aqui" << std::endl;
                    //     Variable new_var = {.stack_pos = generator->m_stack_size};
                    //     std::cout << "problema aqui 2" << std::endl;
                    //     generator->m_variables.insert({statmt_var.token_identif.valor.value(), new_var}); // apenas guardo posição da variável/valor na stack
                    //     std::cout << "problema aqui 3" << std::endl;
                    //     generator->generate_expr(statmt_var.expr); // após identificador encontramos uma expressão, seja essa um inteiro ou otura variável
                    //     std::cout << "problema aqui 5" << std::endl;
                    // } else {
                    //     std::cerr << "Identificador '" << statmt_var.token_identif.valor.value() << "' já utilizado." << std::endl;
                    //     exit(EXIT_FAILURE);
                    // }
                    if (generator->m_variables.contains(statmt_var.token_identif.valor.value()))
                    {
                        std::cerr << "Identificador '" << statmt_var.token_identif.valor.value() << "' já utilizado." << std::endl;
                        exit(EXIT_FAILURE);
                    }
                    std::cout << "problema aqui" << std::endl;
                    Variable new_var = {.stack_pos = generator->m_stack_size};
                    std::cout << "problema aqui 2" << std::endl;
                    generator->m_variables.insert({statmt_var.token_identif.valor.value(), new_var}); // apenas guardo posição da variável/valor na stack
                    std::cout << "problema aqui 3" << std::endl;
                    generator->generate_expr(statmt_var.expr); // após identificador encontramos uma expressão, seja essa um inteiro ou otura variável
                    std::cout << "problema aqui 5" << std::endl;
                }
            };

            std::cout << "problema aqui 8" << std::endl;
            StatmtVisitor visitor {.generator = this};
            std::cout << "problema aqui 9" << std::endl;
            std::visit(visitor, statmt.variant_statmt);
            std::cout << "problema aqui 10" << std::endl;
        }


        /*Função que converte os tokens obtidos pela tokenização
        no respectivo código em assembly.
        PARÂMETROS:
        - tokens_file (std::vector<Token>): vetor de tokens lexicais
        do arquivo .ml
        RETURNS:
        - out (std::string): formato em string de uma stringstream
        que contêm o código em asm correspondente ao código em ml
        */
        inline std::string generate_program() {
            m_out << "global _start\n_start:\n";

                for (const node::Statmt &statmt : m_program.statmts)
                {   
                    generate_statmt(statmt);
                }

            m_out << "    mov rax, 60\n";
            m_out << "    mov rdi, 0\n";
            m_out << "    syscall\n";
            return m_out.str();
        }


    private:

        inline void push(const std::string& reg) {
            m_out << "    push " << reg << '\n';
            m_stack_size++;
        }

        inline void pop(const std::string& reg) {
            m_out << "    pop " << reg << '\n';
            m_stack_size--;
        }

        const node::Program m_program;
        std::stringstream m_out;
        size_t m_stack_size = 0;
        std::unordered_map<std::string, Variable> m_variables;
};