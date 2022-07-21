#include <fstream>
#include <iostream>
#include <streambuf>

#include "exit_codes/exit_codes.h"

#include "lox/errorhandler.h"
#include "lox/scanner.h"
#include "lox/stringmap.h"


// represents Lox.java
struct Lox : public lox::ErrorHandler
{
    bool error_detected = false;

    int
    main(int argc, char** argv)
    {
        if (argc > 3)
        {
            fmt::print("Usage: lox [script]\n");
            return exit_codes::incorrect_usage;
        }
        else if (argc == 3)
        {
            const std::string flag = argv[1];
            const std::string cmd = argv[2];
            if (flag == "-x")
            {
                return run_code_get_exitcode(cmd);
            }
            else
            {
                fmt::print("Invalid flag {0}\n", flag);
                return exit_codes::incorrect_usage;
            }
        }
        else if (argc == 2)
        {
            return run_file_get_exitcode(argv[1]);
        }
        else
        {
            run_prompt();
            return exit_codes::no_error;
        }
    }

    void
    run_prompt()
    {
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
                return;
            }
        }
    }

    [[nodiscard]] int
    run_file_get_exitcode(char* path)
    {
        // https://stackoverflow.com/questions/2602013/read-whole-ascii-file-into-c-stdstring/2602258
        std::ifstream handle(path);

        if(handle.good() == false)
        {
            fmt::print("Unable to open file '{}'\n", path);
            return exit_codes::missing_input;
        }

        std::string str((std::istreambuf_iterator<char>(handle)),
                        std::istreambuf_iterator<char>());

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
        return fmt::format("{}^ ", dashes);
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
