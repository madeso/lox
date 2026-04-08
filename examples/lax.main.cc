#include <fstream>
#include <iostream>
#include <streambuf>
#include <memory>
#include <functional>
#include <sstream>
#include <utility>

#include "exit_codes/exit_codes.h"

#include "lax/errorhandler.h"
#include "lax/scanner.h"
#include "lax/ast.h"
#include "lax/parser.h"
#include "lax/interpreter.h"
#include "lax/printhandler.h"
#include "lax/resolver.h"



struct PrintErrors : lax::PrintHandler
{
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

    virtual RunError run_code(std::shared_ptr<lax::Interpreter> interpreter, const std::string& str) = 0;
};

struct TokenizeCodeRunner : CodeRunner
{
    RunError
    run_code(std::shared_ptr<lax::Interpreter> interpreter, const std::string& source) override
    {
        auto tokens = lax::scan_lox_tokens(source, interpreter->get_error_handler());

        if(tokens.errors > 0)
        {
            return RunError::syntax_error;
        }

        for (const auto& token : tokens.tokens)
        {
            std::cout << token.to_debug_string(interpreter.get()) << "\n";
        }

        return RunError::no_error;
    }
};

enum class UseGraphviz
{
    no, yes
};

struct AstCodeRunner : CodeRunner
{
    UseGraphviz use_graphviz;
    
    explicit AstCodeRunner(UseGraphviz gv) : use_graphviz(gv) {}

    RunError
    run_code(std::shared_ptr<lax::Interpreter> interpreter, const std::string& source) override
    {
        auto tokens = lax::scan_lox_tokens(source, interpreter->get_error_handler());
        auto program = lax::parse_lax_program(tokens.tokens, interpreter->get_error_handler());

        if(tokens.errors > 0 || program.errors > 0)
        {
            return RunError::syntax_error;
        }

        if(use_graphviz == UseGraphviz::yes)
        {
            std::cout << lax::ast_to_grapviz(*program.program) << "\n";
        }
        else
        {
            std::cout << lax::print_ast(*program.program) << "\n";
        }
        return RunError::no_error;
    }
};


struct InterpreterRunner : CodeRunner
{
    RunError
    run_code(std::shared_ptr<lax::Interpreter> interpreter, const std::string& source) override
    {
        auto tokens = lax::scan_lox_tokens(source, interpreter->get_error_handler());
        auto program = lax::parse_lax_program(tokens.tokens, interpreter->get_error_handler());
        
        if(tokens.errors > 0 || program.errors > 0)
        {
            return RunError::syntax_error;
        }

        auto resolved = lax::resolve(*program.program, interpreter->get_error_handler());

        if(resolved.has_value() == false)
        {
            return RunError::syntax_error;
        }

        const auto interpret_ok = interpreter->interpret(*program.program, *resolved);
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




struct AssemblerTokensRunner : CodeRunner
{
    RunError
    run_code(std::shared_ptr<lax::Interpreter> interpreter, const std::string& source) override
    {
        auto tokens = lax::scan_asm_tokens(source, interpreter->get_error_handler());

        if (tokens.errors > 0)
        {
            return RunError::syntax_error;
        }

        for (const auto& token : tokens.tokens)
        {
            std::cout << token.to_debug_string() << "\n";
        }

        return RunError::no_error;
    }
};



struct AssemblerStatementsRunner : CodeRunner
{
    RunError
    run_code(std::shared_ptr<lax::Interpreter> interpreter, const std::string& source) override
    {
        auto tokens = lax::scan_asm_tokens(source, interpreter->get_error_handler());
        auto program = lax::parse_asm_program(tokens.tokens, interpreter->get_error_handler());

        if (tokens.errors > 0 || program.errors > 0)
        {
            return RunError::syntax_error;
        }

        for (const auto& instruction : program.program)
        {
            std::cout << instruction.label.value_or("[no label]") << ": " << lax::tokentype_to_string(instruction.instruction);

            for (const auto& va : instruction.arguments)
            {
                std::cout << " " << lax::string_from_asm(va);
            }
            std::cout << "\n";
        }

        return RunError::no_error;
    }
};




std::string read_to_string(std::istream& handle)
{
    // https://stackoverflow.com/questions/2602013/read-whole-ascii-file-into-c-stdstring/2602258
    std::string line;
    std::ostringstream ss;
    while(std::getline(handle, line))
    {
        ss << line << '\n';
    }
    return ss.str();
}

std::optional<std::string> read_file_to_string(const std::string& path)
{
    if (path == "stdin")
    {
        return read_to_string(std::cin);
    }
    
    std::ifstream handle(path);
    if (handle.good() == false)
    {
        std::cerr << "Unable to open file '" << path << "'\n";
        return std::nullopt;
    }

    return read_to_string(handle);
}



/// return int to exit directly, or nullopt to continue parsing
using FlagFunction = std::function<std::optional<int>()>;
struct FlagBinding
{
    std::string documentation;
    FlagFunction function;
};
struct Commandline
{
    std::unordered_map<char, FlagBinding> flags;

    Commandline& bind_flag(char flag, const std::string& documentation, FlagFunction function)
    {
        flags[flag] = {documentation, std::move(function)};
        return *this;
    }

    void print_usage() const
    {
        std::cout << "Usage: lax [flags] [file/script]\n";
        std::cout << "\n";

        std::cout << "FLAGS:\n";
        for (const auto& [flag, binding] : flags)
        {
            std::cout << "  -" << flag << " - " << binding.documentation << "\n";
        }
        std::cout << "\n";

        std::cout << "FILE/SCRIPT:\n";
        std::cout << "  path to file or script(-x), special files are:\n";
        std::cout << "    repl - run a repl instead\n";
        std::cout << "    stdin - read file from stdin\n";
        std::cout << "\n";
    }

    int run(int argc, char** argv, const std::function<int (const std::string&)>& callback) const
    {
        for (int arg_index = 1; arg_index < argc; arg_index += 1)
        {
            const std::string cmd = argv[arg_index];
            const bool is_flags = cmd[0] == '-' || cmd[0] == '/';

            if (is_flags)
            {
                if (cmd.length() <= 1)
                {
                    std::cerr << "ERROR: missing flag in argument #" << arg_index << ": " << cmd << "\n";
                    print_usage();
                    return exit_codes::incorrect_usage;
                }

                for (unsigned int flag_index = 1; flag_index < cmd.length(); flag_index += 1)
                {
                    const auto flag = cmd[flag_index];
                    
                    if (const auto found = flags.find(flag); found == flags.end())
                    {
                        std::cerr << "ERROR: unknown flag" << flag << "\n";
                        print_usage();
                        return exit_codes::incorrect_usage;
                    }
                    else
                    {
                        const auto ret = found->second.function();
                        if (ret.has_value())
                        {
                            return *ret;
                        }
                    }
                }
            }
            else
            {
                if (arg_index + 1 != argc)
                {
                    // this isn't the last argument
                    std::cerr << "ERROR: too many arguments after #" << arg_index << ": " << cmd << "\n";
                    print_usage();
                    return exit_codes::incorrect_usage;
                }

                return callback(cmd);
            }
        }

        std::cerr << "No input given...\n";
        print_usage();
        return exit_codes::incorrect_usage;
    }
};

enum class StreamType
{
    automatic, file, code
};



void
run_repl(CodeRunner* run, const std::shared_ptr<lax::Interpreter>& interpreter)
{
    const std::string_view exit_message = "EOF (ctrl-d/ctrl-z) to exit.\n";
    std::cout << "REPL started. " << exit_message;
    while (true)
    {
        std::cout << "> ";
        std::string line;
        if (std::getline(std::cin, line))
        {
            const auto result = run->run_code(interpreter, line);
            if(result != RunError::no_error )
            {
                std::cout << exit_message;
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
run_code_get_exitcode(CodeRunner* runner, std::shared_ptr<lax::Interpreter> interpreter, const std::string& str)
{
    const auto error_detected = runner->run_code(std::move(interpreter), str);

    switch (error_detected)
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



[[nodiscard]] int
run_file_get_exitcode(CodeRunner* runner, std::shared_ptr<lax::Interpreter> interpreter, const std::string& path)
{
    const auto contents = read_file_to_string(path);

    if(contents.has_value() == false)
    {
        return exit_codes::missing_input;
    }

    return run_code_get_exitcode(runner, std::move(interpreter), *contents);
}



// todo(Gustav): refactor error handling to be part of parsing so we
// know the source and don't have to recreate the mapping for each error



int
main(int argc, char** argv)
{
    std::shared_ptr<CodeRunner> runner = std::make_shared<InterpreterRunner>();
    StreamType stream_type = StreamType::automatic;

    Commandline cli;
    cli.bind_flag('x', "assume the file input is a piece of code", [&]() -> std::optional<int> {
        stream_type = StreamType::code;
        return std::nullopt;
        });
    cli.bind_flag('h', "print help", [&]() -> std::optional<int> {
        cli.print_usage();
        return exit_codes::no_error;
        });
    cli.bind_flag('L', "run lexer only = tokenize input", [&]() -> std::optional<int> {
        runner = std::make_shared<TokenizeCodeRunner>();
        return std::nullopt;
        });
    cli.bind_flag('P', "run lexer/parser only = print ast tree", [&]() -> std::optional<int> {
        runner = std::make_shared<AstCodeRunner>(UseGraphviz::no);
        return std::nullopt;
        });
    cli.bind_flag('G', "run lexer/parser only = print ast tree in graphviz", [&]() -> std::optional<int> {
        runner = std::make_shared<AstCodeRunner>(UseGraphviz::yes);
        return std::nullopt;
        });
    cli.bind_flag('I', "run interpreter", [&]() -> std::optional<int> {
        runner = std::make_shared<InterpreterRunner>();
        return std::nullopt;
        });
    cli.bind_flag('l', "show asm tokens", [&]() -> std::optional<int> {
        runner = std::make_shared<AssemblerTokensRunner>();
        return std::nullopt;
        });
    cli.bind_flag('p', "run lexer/parser on asm", [&]() -> std::optional<int> {
        runner = std::make_shared<AssemblerStatementsRunner>();
        return std::nullopt;
        });

    return cli.run(argc, argv, [&](const std::string& cmd) -> int {
        PrintErrors print_errors;
        auto interpreter = lax::make_interpreter(&print_errors, [](const std::string& s) { std::cout << s << "\n"; });

        if (stream_type == StreamType::automatic)
        {
            if (cmd == "repl")
            {
                run_repl(runner.get(), interpreter);
                return exit_codes::no_error;
            }
            stream_type = StreamType::file;
        }

        switch (stream_type)
        {
        case StreamType::code:
            return run_code_get_exitcode(runner.get(), std::move(interpreter), cmd);
        case StreamType::file:
            return run_file_get_exitcode(runner.get(), std::move(interpreter), cmd);
        default:
            assert(false && "unhandled StreamType in main(...)");
            return exit_codes::internal_error;
        }
    });
}
