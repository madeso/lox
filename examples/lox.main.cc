#include <fstream>
#include <iostream>
#include <streambuf>

#include "exit_codes/exit_codes.h"

#include "lox/errorhandler.h"
#include "lox/scanner.h"
#include "lox/stringmap.h"


// represents Lox.java
struct Lox : lox::ErrorHandler
{
    bool error_detected = false;

    void
    print_usage()
    {
        std::cout << "Usage: lox [flags] [file/script]\n";
        std::cout << "\n";

        std::cout << "FLAGS:\n";
        std::cout << "  -x - assume the file is a piece of code\n";
        std::cout << "  -h - print help\n";
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
                    return run_code_get_exitcode(cmd);
                }
                else if(cmd == "repl")
                {
                    run_prompt();
                    return exit_codes::no_error;
                }
                else
                {
                    // neither code nor prompt, assume file
                    return run_file_get_exitcode(cmd);
                }
            }
        }
        
        std::cerr << "No input given...\n";
        print_usage();
        return exit_codes::incorrect_usage;
    }

    void
    run_prompt()
    {
        std::cout << "REPL started. EOF (ctrl-d) to exit.\n";
        while (true)
        {
            std::cout << "> ";
            std::string line;
            if (std::getline(std::cin, line))
            {
                run(line);
                error_detected = false;
            }
            else
            {
                std::cout << "\n\n";
                return;
            }
        }
    }

    [[nodiscard]] int
    run_file_get_exitcode(const std::string& path)
    {
        if(path == "stdin")
        {
            return run_stream_get_exitcode(std::cin);
        }
        else
        {
            std::ifstream handle(path);

            if(handle.good() == false)
            {
                std::cerr << "Unable to open file '" << path << "'\n";
                return exit_codes::missing_input;
            }

            return run_stream_get_exitcode(handle);
        }
    }

    [[nodiscard]] int
    run_stream_get_exitcode(std::istream& handle)
    {
        // https://stackoverflow.com/questions/2602013/read-whole-ascii-file-into-c-stdstring/2602258
        std::string str
        (
            (std::istreambuf_iterator<char>(handle)),
            std::istreambuf_iterator<char>()
        );

        return run_code_get_exitcode(str);
    }

    [[nodiscard]] int
    run_code_get_exitcode(const std::string& str)
    {
        run(str);

        if (error_detected)
        {
            return exit_codes::bad_input;
        }
        else
        {
            return exit_codes::no_error;
        }
    }

    void
    run(const std::string& source)
    {
        current_source = source;
        auto tokens = lox::ScanTokens(source, this);

        if(error_detected)
        {
            // assume we have written error to user
            return;
        }

        for (const auto& token : tokens)
        {
            std::cout << token.to_string() << "\n";
        }
    }

    // todo(Gustav): refactor error handling to be part of parsing so we
    // know the source and don't have to recreate the mapping for each error
    std::string_view current_source;

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


int main(int argc, char** argv)
{
    Lox lox;
    return lox.main(argc, argv);
}
