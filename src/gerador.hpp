#pragma once

#include <vector>
#include <string>
#include <sstream>
#include <map>
#include <assert.h>

#include "parser.hpp"


struct Variable {
    size_t stack_pos;
};


class Generator {
    public:
        inline Generator(node::Program program) 
            : m_program(std::move(program))
        {}

        /*
        Método responsável por escrever o código em assembly
        referente aos nós de termos. Assim, implementa um visitor
        pattern para checar qual tipo de termo está lidando com,
        e, assim, adaptar a geração de código.
        PARÂMETROS:
        - term (const node::Term*): ponteiro para o nó da AST
        referente ao código que será gerado.
        RETURNS:
        */
        inline void generate_term(const node::Term* term) {
            struct TermVisitor {
                Generator& generator;
                void operator()(const node::TermIntLit* term_int_lit) {
                    generator.m_out << "    mov rax, " << term_int_lit->token_int.valor.value() << '\n';
                    std::cout << "esta aqui para o valor: " << term_int_lit->token_int.valor.value() << '\n';
                    generator.push("rax");
                }
                void operator()(const node::TermIdentif* term_identif) {
                    int contem = 0;
                    int i_og_scope;
                    for (int i = 0; i < (generator.num_scopes); i++) {
                        if (generator.m_scopes.at(i).contains(term_identif->token_identif.valor.value())) {
                            contem = 1;
                            i_og_scope = i;
                            break;
                        }
                    }
                    if (contem == 1) {
                        auto &var = generator.m_scopes.at(i_og_scope).at(term_identif->token_identif.valor.value());
                        std::stringstream offset;
                        /*
                        A stack é organizada para que cada elemento tenha um tamanho de 8 bytes. Assim, usamos o operador
                        QWORD para especificar que vamos acessar um local na memória de 8 bytes de tamanho. Para mais,
                        multiplicamos o offset por 8, pois ele por si só nos da o número de elementos entre o desejado e
                        aquele no topo. Porém, cada elemento da stack possui 8 bytes de espaço, independente do tipo do
                        dado que ali está. Portanto, multiplicamos por 8 para "descer" o número correto de bytes até o
                        endereço desejado.
                        */
                        std::cout << "stack_size: " << generator.m_stack_size << " var: " << term_identif->token_identif.valor.value() << " pos na stack: " << var.stack_pos << std::endl;
                        offset << "QWORD [rsp + " << (generator.m_stack_size - var.stack_pos - 1) * 8 << "]"; // stack e registrador %rsp crescem pra baixo.
                        generator.push(offset.str());
                    } else {
                        std::cerr << "Identificador '" << term_identif->token_identif.valor.value() << "' não inicializado." << std::endl;
                        exit(EXIT_FAILURE);
                    }
                }
                void operator()(const node::TermParen* term_paren) {
                    generator.generate_expr(term_paren->expr);
                }
            };
            TermVisitor visitor {.generator = *this};
            std::visit(visitor, term->variant_term);
        }

        /*
        Método responsável pela geração de código assembly dos
        nós de expressões gerados pelo parseamento. Implementa
        um visitor pattern para reconhecer qual tipo de expressão
        está lidando, e, assim, adaptar a geração de código.
        PARÂMETROS:
        - expr (const node::Expr*): ponteiro para o nó da AST
        que servirá de base para a geração de código
        RETURNS:
        */
        inline void generate_expr(const node::Expr* expr) {
            struct ExprVisitor {
                Generator& generator;
                void operator()(const node::Term* term) {
                    generator.generate_term(term);
                }
                void operator()(const node::BinExpr* bin_expr) {
                    generator.generate_bin_expr(bin_expr);
                }
            };

            ExprVisitor visitor {.generator = *this};
            std::visit(visitor, expr->variant_expr);
        }

        /*
        Método responsável pela geração de código assembly
        para os nós de expressões binárias. Implementa um 
        visitor pattern para adaptar a geração de código
        conforme o tipo de expressão que está lidando.
        PARÂMETROS:
        - bin_expr (const node::BinExpr*): ponteiro para o
        nó da AST referente à expressão binária que servirá de
        base para a geração de código.
        RETURNS:
        */
        inline void generate_bin_expr(const node::BinExpr* bin_expr) {
            switch (bin_expr->token.tipo) {
                case TipoToken::mais:
                    generate_expr(bin_expr->lado_esquerdo);
                    generate_expr(bin_expr->lado_direito);
                    pop("rax");
                    pop("rbx");
                    m_out << "    add rax, rbx\n";
                    push("rax");
                    break;
                case TipoToken::menos:
                    generate_expr(bin_expr->lado_esquerdo);
                    generate_expr(bin_expr->lado_direito);
                    pop("rax");
                    pop("rbx");
                    m_out << "    sub rbx, rax\n";
                    push("rbx");
                    break;
                case TipoToken::asterisco:
                    generate_expr(bin_expr->lado_esquerdo);
                    generate_expr(bin_expr->lado_direito);
                    pop("rax");
                    pop("rbx");
                    m_out << "    mul rbx\n";
                    push("rax");
                    break;
                case TipoToken::barra_div:
                    generate_expr(bin_expr->lado_esquerdo);
                    generate_expr(bin_expr->lado_direito);
                    pop("rbx");
                    pop("rax");
                    m_out << "    div rbx\n";
                    push("rax");
                    break;
                case TipoToken::maior:
                case TipoToken::menor:
                case TipoToken::maior_igual:
                case TipoToken::menor_igual:
                    generate_expr(bin_expr->lado_esquerdo);
                    generate_expr(bin_expr->lado_direito);
                    pop("rbx");
                    pop("rax");
                    m_out << "    cmp rax, rbx\n";
                    break;
            }
        }

        /*
        Método responsável por lidar com escopos na geração de 
        código assembly. Assim, inicia e finaliza um escopo, 
        enquanto, no processo, chama a geração de código para
        os statement encontrados no interior do escopo em questão.
        PARÂMETROS:
        - scope (const node::Scope*): ponteiro para o nó da AST
        referente ao escopo que está interpretando.
        RETURNS:
        */
        inline void generate_scope(const node::Scope* scope) {
            begin_scope();
            for (const node::Statmt* statmt : scope->statmts_scope) {
                generate_statmt(statmt);
            }
            end_scope();
        }

        /*
        Método que gera o código assembly para os diferentes tipos
        de statements possíveis (checar grammar.md). Implementa
        um visitor pattern para reconhecer qual tipo de statement
        dentro do std::variant esta lidando com, e, assim, adaptar
        a geração de código.
        PARÂMETROS:
        - statmt (const node::Statmt*): ponteiro para o nó da AST
        que servirá de base para a geração de código.
        RETURNS:
        */
        inline void generate_statmt(const node::Statmt* statmt) {
            struct StatmtVisitor {
                Generator& generator;
                void operator()(const node::StatmtExit* statmt_exit) {
                    generator.generate_expr(statmt_exit->expr);
                    generator.m_out << "    mov rax, 60\n";
                    generator.pop("rdi");
                    generator.m_out << "    syscall\n";
                }
                void operator()(const node::StatmtVar* statmt_var) {
                    struct VarVisitor {
                        Generator& generator;
                        void operator()(const node::NewVar* new_var) {
                            if (new_var->token_identif.valor.has_value()) {
                                for (int i = 0; i < generator.m_scopes.size(); i++) {
                                    if (generator.m_scopes.at(i).contains(new_var->token_identif.valor.value())) {
                                        std::cerr << "Identificador '" << new_var->token_identif.valor.value() << "' já utilizado." << std::endl;
                                        exit(EXIT_FAILURE);
                                    }
                                }
                                Variable nova_var = {.stack_pos = generator.m_stack_size};
                                generator.m_scopes.back().insert({new_var->token_identif.valor.value(), nova_var});
                                generator.generate_expr(new_var->expr);
                            }
                        }
                        void operator()(const node::ReassVar* reass_var) {
                            if (reass_var->token_identif.valor.has_value()) {
                                int contem = 0;
                                int i_og_scope;
                                for (int i = 0; i < generator.num_scopes; i++) {
                                    if (generator.m_scopes.at(i).contains(reass_var->token_identif.valor.value())) {
                                        contem = 1;
                                        i_og_scope = i;
                                        break;
                                    }
                                }
                                if (contem == 1) {
                                    // !!!!!!!!!
                                    /*
                                    O problema está aqui:
                                    na hora que eu encontro uma expressão de reassignment, eu estou gerando o código do valor
                                    que a variável vai receber e depois eu mexo o ponteiro da stack. Nâo é assim que deve ser feito.
                                    Eu primeiro preciso mover o ponteiro da stack para a posição na stack da variável que vai
                                    receber o reassignment, pra depois dar push no valor em si. Depois mexo novamente o ponteiro para
                                    a base da stack.
                                    O problema é: como vou fazer isso sem afetar a geração de código de, por exemplo, expressões binárias?
                                    Por que isso foi generalizado para ser feito na função de geração de termos, sendo um termo um inteiro
                                    ou um identificador. Então toda vez que eu encontrar um identificador, vou para essa função, não
                                    necessariamente sendo um reassignment.
                                    */
                                    

                                    auto &var = generator.m_scopes.at(i_og_scope).at(reass_var->token_identif.valor.value());
                                    // std::stringstream offset;
                                    // std::cout << "ESTOU NO NOVO QWORD" << std::endl;
                                    std::cout << "stack_size: " << generator.m_stack_size << " var: " << reass_var->token_identif.valor.value() << " pos na stack: " << var.stack_pos << std::endl;
                                    // offset << "QWORD [rsp + " << (generator.m_stack_size - var.stack_pos) * 8 << "]"; // stack e registrador %rsp crescem pra baixo.
                                    // generator.push(offset.str());
                                    
                                    generator.m_out << "    add rsp, " << (generator.m_stack_size - var.stack_pos) * 8 << '\n';
                                    generator.generate_expr(reass_var->expr);

                                    // TESTAR CHAMAR QWORD AQUI
                                } else {
                                    std::cerr << "Identificador '" << reass_var->token_identif.valor.value() << "' não inicializado." << std::endl;
                                    exit(EXIT_FAILURE);
                                }
                            }
                        }
                    };
                    std::visit(VarVisitor {.generator = generator}, statmt_var->variant_var);
                }
                void operator()(const node::Scope* scope) {
                    generator.generate_scope(scope);
                }
                void operator()(const node::StatmtIf* statmt_if) {
                    struct StatmtIfVisitor {
                        Generator& generator;
                        std::string label;
                        void operator()(const node::Term* term) {
                            generator.pop("rax");
                            generator.m_out << "    cmp rax, 0\n";
                            generator.m_out << "    jle " << label << '\n';
                        }
                        void operator()(const node::BinExpr* bin_expr) {
                            switch (bin_expr->token.tipo) {
                                case TipoToken::mais:
                                case TipoToken::menos:
                                case TipoToken::asterisco:
                                case TipoToken::barra_div:
                                    generator.m_out << "    jz " << label << '\n';
                                    break;
                                case TipoToken::maior:
                                    std::cout << generator.m_stack_size << std::endl;
                                    generator.m_out << "    jle " << label << '\n';
                                    break;
                                case TipoToken::menor:
                                    generator.m_out << "    jge " << label << '\n';
                                    break;
                                case TipoToken::maior_igual:
                                    generator.m_out << "    jl " << label << '\n';
                                    break;
                                case TipoToken::menor_igual:
                                    generator.m_out << "    jg " << label << '\n';
                                    break;
                            }
                        }
                    };
                    std::string label = generator.create_label();
                    generator.generate_expr(statmt_if->expr);
                    std::visit(StatmtIfVisitor {.generator = generator, .label = label}, statmt_if->expr->variant_expr);
                    generator.generate_scope(statmt_if->scope);
                    generator.m_out << label << ":\n";
                }
            };
            StatmtVisitor visitor {.generator = *this};
            std::visit(visitor, statmt->variant_statmt);
        }

        /*
        Função base para a geração de código assembly. Para isso,
        faz uso de um 'for' que atravessa o vetor de nós da AST
        representando as diversas statements do programa. No final, 
        escreve a syscall padrão de saída do assembly, caso não
        seja encontrado a função "exit()" ao longo do código.
        PARÂMETROS:
        RETURNS:
        - out (std::string): formato em string de uma stringstream
        que contêm o código em assembly correspondente ao código 
        em .ml.
        */
        inline std::string generate_program() {
            m_scopes.push_back(m_variables);
            m_out << "global _start\n_start:\n";
                for (const node::Statmt* statmt : m_program.statmts) {   
                    generate_statmt(statmt);
                }
            m_out << "    mov rax, 60\n"; //código da expressão de saída para o assembly
            m_out << "    mov rdi, 0\n"; // código de saída do programa
            m_out << "    syscall\n";
            return m_out.str();
        }


    private:
        const node::Program m_program; // Nó referente ao início do programa
        std::stringstream m_out; // Stringstream que contêm todo o código assembly
        size_t m_stack_size = 0; // Número de informações na stack
        std::map<std::string, Variable> m_variables; // HashMap com todas as variáveis
        std::vector<std::map<std::string, Variable>> m_scopes; // Vetor com os maps de cada escopo
        size_t num_scopes = 1;
        int m_label_count = 0;

        /*
        Método que escreve o código para o "push" do valor
        guardado por um registrador para a stack. No final, 
        incrementa 1 no valor de m_stack_size.
        PARÂMETROS:
        - reg (const std::string&): string que contêm o nome
        do registrador que se deseja envolver na operação de push.
        RETURNS:
        */
        inline void push(const std::string& reg) {
            m_out << "    push " << reg << '\n';
            m_stack_size++;
        }

        /*
        Método que escreve o código para o "pop" de um valor
        no topo da stack para um registrador. No final, decrementa
        1 no valor de m_stack_size.
        PARÂMETROS:
        - reg (const std::string&): string que contêm o nome
        do registrador que se deseja envolver na operação de pop.
        RETURNS:
        */
        inline void pop(const std::string& reg) {
            m_out << "    pop " << reg << '\n';
            m_stack_size--;
        }

        /*
        Função que representa o início de um escopo. Na prática,
        apenas salva o número de variáveis do escopo antigo, para
        ter um ponto de "checkpoint" para quando o escopo novo 
        for finalizado.
        PARÂMETROS:
        RETURNS:
        */
        inline void begin_scope() {
            std::map<std::string, Variable> variables_scope;
            m_scopes.push_back(variables_scope);
            num_scopes++;
        }

        /*
        Função que encerra o escopo atual. Assim, remove as
        variáveis implementadas nele e atualiza o valor do 
        ponteiro da stack para "voltar" ao fim do escopo
        anterior, de modo a ignorar o que foi adicionado no 
        escopo destruído e permitir sobrescrita.
        PARÂMETROS:
        RETURNS:
        */
        inline void end_scope() {
            size_t num_pops = m_scopes.back().size(); //Número de variáveis adicionadas no escopo
            m_out << "    add rsp, " << num_pops * 8 << "\n";
            m_stack_size -= num_pops;
            m_scopes.pop_back();
            num_scopes--;
        }

        /*
        Método que sinaliza no arquivo em assembly o local 
        da label para a implementação de 'ifs'.
        PARÂMETROS:
        RETURNS:
        - (std::string): string que contêm o número da label
        nova.
        */
        inline std::string create_label() {
            return "label" + std::to_string(m_label_count++);
        }
};