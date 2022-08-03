#include <fstream>
#include <iostream>
#include <streambuf>
#include <memory>
#include <functional>

#include "exit_codes/exit_codes.h"

#include "lox/errorhandler.h"
#include "lox/scanner.h"
#include "lox/ast.h"
#include "lox/parser.h"
#include "lox/interpreter.h"
#include "lox/printhandler.h"


struct PrintErrors : lox::PrintHandler
{
    explicit PrintErrors(std::string_view source)
        : lox::PrintHandler(source)
    {
    }

    void
    on_line(std::string_view line) override
    {
        std::cerr << line << "\n";
    }
};

enum class RunError
{
    no_error, syntax_error, runtime_error
};

struct CodeRunner
{
    virtual ~CodeRunner() = default;

    virtual RunError run_code(lox::Interpreter* interpreter, const std::string& str) = 0;
};

struct TokenizeCodeRunner : CodeRunner
{
    RunError
    run_code(lox::Interpreter*, const std::string& source) override
    {
        auto printer = PrintErrors{source};
        auto tokens = lox::ScanTokens(source, &printer);

        if(printer.error_detected)
        {
            return RunError::syntax_error;
        }

        for (const auto& token : tokens)
        {
            std::cout << token.to_string() << "\n";
        }

        return RunError::no_error;
    }
};


struct AstCodeRunner : CodeRunner
{
    bool use_graphviz;
    
    explicit AstCodeRunner(bool gv) : use_graphviz(gv) {}

    RunError
    run_code(lox::Interpreter*, const std::string& source) override
    {
        auto printer = PrintErrors{source};
        auto tokens = lox::ScanTokens(source, &printer);
        auto program = lox::parse_program(tokens, &printer);

        if(printer.error_detected)
        {
            return RunError::syntax_error;
        }

        if(use_graphviz)
        {
            std::cout << lox::ast_to_grapviz(*program) << "\n";
        }
        else
        {
            std::cout << lox::print_ast(*program) << "\n";
        }
        return RunError::no_error;
    }
};


struct InterpreterRunner : CodeRunner
{
    RunError
    run_code(lox::Interpreter* interpreter, const std::string& source) override
    {
        auto printer = PrintErrors{source};
        auto tokens = lox::ScanTokens(source, &printer);
        auto program = lox::parse_program(tokens, &printer);
        
        if(printer.error_detected)
        {
            return RunError::syntax_error;
        }

        const auto interpret_ok = lox::interpret(interpreter, *program, &printer, [](const std::string& s){ std::cout << s << "\n";});
        if(interpret_ok)
        {
            return RunError::no_error;
        }
        else
        {
            return RunError::runtime_error;
        }
    }
};



std::shared_ptr<CodeRunner> make_lexer()
{
    return std::make_shared<TokenizeCodeRunner>();
}

std::shared_ptr<CodeRunner> make_parser()
{
    return std::make_shared<AstCodeRunner>(false);
}

std::shared_ptr<CodeRunner> make_parser_gv()
{
    return std::make_shared<AstCodeRunner>(true);
}

std::shared_ptr<CodeRunner> make_interpreter()
{
    return std::make_shared<InterpreterRunner>();
}


// represents Lox.java
struct Lox
{
    void
    print_usage()
    {
        std::cout << "Usage: lox [flags] [file/script]\n";
        std::cout << "\n";

        std::cout << "FLAGS:\n";
        std::cout << "  -x - assume the file is a piece of code\n";
        std::cout << "  -h - print help\n";
        std::cout << "  -L - run lexer only = tokenize input\n";
        std::cout << "  -P - run lexer/parser only = print ast tree\n";
        std::cout << "  -G - run lexer/parser only = print ast tree in graphviz\n";
        std::cout << "  -I - run interpreter\n";
        std::cout << "\n";

        std::cout << "FILE/SCRIPT:\n";
        std::cout << "  path to file or script(-x), special files are:\n";
        std::cout << "    repl - run a repl instead\n";
        std::cout << "    stdin - read file from stdin\n";
        std::cout << "\n";
    }

    int
    main(int argc, char** argv)
    {
        std::function<std::shared_ptr<CodeRunner>()> run_creator = make_interpreter;
        bool is_code = false;

        for(int arg_index=1; arg_index<argc; arg_index+=1)
        {
            const std::string cmd = argv[arg_index];
            const bool is_flags = cmd[0] == '-' || cmd[0] == '/';

            if(is_flags)
            {
                bool got_flag = false;
                for(unsigned int flag_index=1; flag_index<cmd.length(); flag_index+=1)
                {
                    got_flag = true;
                    const auto flag = cmd[flag_index];
                    switch(flag)
                    {
                    case 'x':
                        is_code = true;
                        break;
                    case 'h':
                        print_usage();
                        return exit_codes::no_error;
                    case 'L':
                        run_creator = make_lexer;
                        break;
                    case 'P':
                        run_creator = make_parser;
                        break;
                    case 'G':
                        run_creator = make_parser_gv;
                        break;
                    case 'I':
                        run_creator = make_interpreter;
                        break;
                    default:
                        std::cerr << "ERROR: unknown flag" << flag << "\n";
                        print_usage();
                        return exit_codes::incorrect_usage;
                    }
                }

                if(got_flag == false)
                {
                    std::cerr << "ERROR: missing flag in argument #" << arg_index << ": " << cmd << "\n";
                    print_usage();
                    return exit_codes::incorrect_usage;
                }
            }
            else
            {
                if(arg_index+1 != argc)
                {
                    // this isn't the last argument
                    std::cerr << "ERROR: too many arguments after #" << arg_index << ": " << cmd << "\n";
                    print_usage();
                    return exit_codes::incorrect_usage;
                }


                // arg is not flags, assume "file"
                if(is_code)
                {
                    auto runner = run_creator();
                    return run_code_get_exitcode(runner.get(), cmd);
                }
                else if(cmd == "repl")
                {
                    run_prompt(run_creator);
                    return exit_codes::no_error;
                }
                else
                {
                    auto runner = run_creator();
                    // neither code nor prompt, assume file
                    return run_file_get_exitcode(runner.get(), cmd);
                }
            }
        }
        
        std::cerr << "No input given...\n";
        print_usage();
        return exit_codes::incorrect_usage;
    }

    void
    run_prompt(const std::function<std::shared_ptr<CodeRunner>()>& run_creator)
    {
        lox::Interpreter interpreter;
        auto run = run_creator();

        std::cout << "REPL started. EOF (ctrl-d) to exit.\n";
        while (true)
        {
            std::cout << "> ";
            std::string line;
            if (std::getline(std::cin, line))
            {
                const auto result = run->run_code(&interpreter, line);
                if(result != RunError::no_error )
                {
                    std::cout << "EOF (ctrl-d) to exit.\n";
                }
            }
            else
            {
                std::cout << "\n\n";
                return;
            }
        }
    }

    [[nodiscard]] int
    run_file_get_exitcode(CodeRunner* runner, const std::string& path)
    {
        if(path == "stdin")
        {
            return run_stream_get_exitcode(runner, std::cin);
        }
        else
        {
            std::ifstream handle(path);

            if(handle.good() == false)
            {
                std::cerr << "Unable to open file '" << path << "'\n";
                return exit_codes::missing_input;
            }

            return run_stream_get_exitcode(runner, handle);
        }
    }

    [[nodiscard]] int
    run_stream_get_exitcode(CodeRunner* runner, std::istream& handle)
    {
        // https://stackoverflow.com/questions/2602013/read-whole-ascii-file-into-c-stdstring/2602258
        std::string str
        (
            (std::istreambuf_iterator<char>(handle)),
            std::istreambuf_iterator<char>()
        );

        return run_code_get_exitcode(runner, str);
    }

    [[nodiscard]] int
    run_code_get_exitcode(CodeRunner* runner, const std::string& str)
    {
        lox::Interpreter interpreter;
        const auto error_detected = runner->run_code(&interpreter, str);

        switch(error_detected)
        {
        case RunError::no_error:
            return exit_codes::no_error;
        case RunError::syntax_error:
            return exit_codes::bad_input;
        case RunError::runtime_error:
            // hrm... jlox returns 70 but internal error feels wrong...
            return exit_codes::internal_error;
        default:
            assert(false && "unhandled RunError in run_code_get_exitcode(...)");
            return 42;
        }
    }

    

    // todo(Gustav): refactor error handling to be part of parsing so we
    // know the source and don't have to recreate the mapping for each error
    
};


int main(int argc, char** argv)
{
    Lox lox;
    return lox.main(argc, argv);
}
