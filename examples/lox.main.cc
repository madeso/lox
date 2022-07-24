#include <fstream>
#include <iostream>
#include <streambuf>
#include <memory>
#include <functional>

#include "exit_codes/exit_codes.h"

#include "lox/errorhandler.h"
#include "lox/scanner.h"
#include "lox/stringmap.h"
#include "lox/ast.h"


struct PrintErrors : lox::ErrorHandler
{
    bool error_detected = false;
    std::string_view current_source;

    explicit PrintErrors(std::string_view source)
        : current_source(source)
    {
    }

    std::string
    get_line_gutter(std::size_t line)
    {
        return fmt::format("   {} | ", line);
    }

    std::string
    get_marker_at(const lox::LineData& line, std::size_t offset)
    {
        assert(offset >= line.offset.start);
        assert(offset <= line.offset.end);

        const std::size_t line_length = offset - line.offset.start;
        const std::string gutter = get_line_gutter(line.line+1);

        const std::string dashes = std::string((gutter.length() + line_length) - 1, '-');
        return fmt::format("{}^-- ", dashes);
    }

    std::string
    get_underline_for(const lox::LineData& line, const Offset& offset, char underline_char)
    {
        assert(offset.end <= line.offset.end);
        assert(offset.start >= line.offset.start);

        const std::string gutter = get_line_gutter(line.line+1);

        const std::size_t start_char = offset.start - line.offset.start;
        const std::size_t length = offset.end - offset.start + 1;

        const std::string initial_spaces = std::string(gutter.length() + start_char, ' ');
        const std::string underline_string = std::string(length, underline_char);
        return fmt::format("{}{} ", initial_spaces, underline_string);
    }

    void
    print_line(const lox::LineData& line)
    {
        std::cerr
            << get_line_gutter(line.line+1)
            << current_source.substr(line.offset.start, line.offset.end - line.offset.start) << "\n"
            ;
    }

    void
    on_error(const Offset& offset, const std::string& message) override
    {
        const auto map = lox::StringMap{current_source};
        const auto start_line = map.get_line_from_offset(offset.start);
        const auto end_line = map.get_line_from_offset(offset.end);

        if(start_line.line == end_line.line)
        {
            print_line(start_line);
            std::cerr
                << get_underline_for(start_line, offset, '^')
                << "Error: " << message << "\n";
        }
        else
        {
            print_line(end_line);
            std::cerr
                << get_marker_at(end_line, offset.end)
                << "Error: " << message << "\n";

            print_line(start_line);
            std::cerr << get_marker_at(start_line, offset.start) << "note: starts here" << "\n";
        }
        error_detected = true;
    }
};

struct CodeRunner
{
    virtual ~CodeRunner() = default;

    virtual bool run_code(const std::string& str) = 0;
};

struct TokenizeCodeRunner : CodeRunner
{
    bool run_code(const std::string& source) override
    {
        auto printer = PrintErrors{source};
        auto tokens = lox::ScanTokens(source, &printer);

        if(printer.error_detected)
        {
            return false;
        }

        for (const auto& token : tokens)
        {
            std::cout << token.to_string() << "\n";
        }

        return true;
    }
};


struct AstCodeRunner : CodeRunner
{
    bool use_graphviz;
    
    explicit AstCodeRunner(bool gv) : use_graphviz(gv) {}

    bool run_code(const std::string&) override
    {
        // todo(Gustav): remove sample and parse code...

        std::shared_ptr<lox::Expr> expression =
        std::make_unique<lox::ExprBinary>
        (
            std::make_unique<lox::ExprUnary>
            (
                lox::TokenType::MINUS,
                std::make_unique<lox::ExprLiteral>(std::make_unique<lox::Number>(123.0f))
            ),
            lox::TokenType::STAR,
            std::make_unique<lox::ExprGrouping>
            (
                std::make_unique<lox::ExprLiteral>(std::make_unique<lox::Number>(45.67f))
            )
        );

        if(use_graphviz)
        {
            std::cout << lox::ast_to_grapviz(*expression) << "\n";
        }
        else
        {
            std::cout << lox::print_ast(*expression) << "\n";
        }
        return true;
    }
};


std::unique_ptr<CodeRunner> make_lexer()
{
    return std::make_unique<TokenizeCodeRunner>();
}

std::unique_ptr<CodeRunner> make_parser()
{
    return std::make_unique<AstCodeRunner>(false);
}

std::unique_ptr<CodeRunner> make_parser_gv()
{
    return std::make_unique<AstCodeRunner>(true);
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
        std::function<std::unique_ptr<CodeRunner>()> run_creator = make_lexer;
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
    run_prompt(const std::function<std::unique_ptr<CodeRunner>()>& run_creator)
    {
        std::cout << "REPL started. EOF (ctrl-d) to exit.\n";
        while (true)
        {
            std::cout << "> ";
            std::string line;
            if (std::getline(std::cin, line))
            {
                auto run = run_creator();
                run->run_code(line);
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
        const auto error_detected = runner->run_code(str);

        if (error_detected)
        {
            return exit_codes::bad_input;
        }
        else
        {
            return exit_codes::no_error;
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
