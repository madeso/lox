#include <fstream>
#include <iostream>
#include <streambuf>

#include "exit_codes/exit_codes.h"

#include "lox/errorhandler.h"
#include "lox/scanner.h"


// represents Lox.java
struct Lox : public lox::ErrorHandler
{
    int main(int argc, char** argv)
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
                return runCode(cmd);
            }
            else
            {
                fmt::print("Invalid flag {0}\n", flag);
                return exit_codes::incorrect_usage;
            }
        }
        else if (argc == 2)
        {
            return runFile(argv[1]);
        }
        else
        {
            runPrompt();
            return exit_codes::no_error;
        }
    }

    void runPrompt()
    {
        while (true)
        {
            std::cout << "> ";
            std::string line;
            if (std::getline(std::cin, line))
            {
                run(line);
                hadError = false;
            }
            else
            {
                return;
            }
        }
    }

    [[nodiscard]] int runFile(char* path)
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

        return runCode(str);
    }

    [[nodiscard]] int runCode(const std::string& str)
    {
        run(str);

        if (hadError)
        {
            return exit_codes::bad_input;
        }
        else
        {
            return exit_codes::no_error;
        }
    }

    void run(const std::string& source)
    {
        auto tokens = lox::ScanTokens(source, this);

        for (const auto& token : tokens)
        {
            std::cout << token.toString() << "\n";
        }
    }

    void error(int line, const std::string& message)
    {
        report(line, "", message);
    }

    void report(int line, const std::string& where, const std::string& message) override
    {
        std::cerr << "[line " << line << "] Error" << where << ": " << message << "\n";
        hadError = true;
    }

    bool hadError = false;
};


int main(int argc, char** argv)
{
    Lox lox;
    return lox.main(argc, argv);
}
