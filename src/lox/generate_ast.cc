#include <iostream>
#include <fstream>
#include <string>
#include <string_view>
#include <vector>

#include <fmt/format.h>

#include "exit_codes/exit_codes.h"



struct Var { std::string type; std::string name; };
struct Sub { std::string name; std::vector<Var> members;};
struct Vis { std::string name; std::string type; };

constexpr std::string_view INDENT = "    ";


void
define_type
(
    std::ofstream& source, std::ofstream& header,
    const std::string& base_name,
    const Sub& sub,
    const std::vector<Vis>& visitors
)
{
    header << "struct " << base_name << sub.name << " : " << base_name << "\n";
    header << "{\n";
    for(const auto& m: sub.members)
    {
        const std::string type = m.type == "Token" ? "Token" : fmt::format("std::shared_ptr<{}>", m.type);
        header << INDENT << type << " " << m.name << ";\n";
    }

    header << "\n";
    for(const auto& v: visitors)
    {
        header << INDENT << v.type << " accept" << "(" << v.name << "* vis) const override;\n";
        
        source << v.type << " " << base_name << sub.name << "::accept(" << v.name << "* vis) const\n";
        source << "{\n";
        source << INDENT << "vis ->visit" << sub.name << "(*this);\n";
        source << "}\n";
        source << "\n\n";
    }

    header << "};\n";
    header << "\n\n";
}


void
define_visitor
(
    std::ofstream& header,
    const std::string& base_name,
    const Vis& vis, const std::vector<Sub>& subs
)
{
    header << "struct " << vis.name << "\n";
    header << "{\n";
    header << INDENT << "virtual ~" << vis.name << "() = default;\n";
    header << "\n";
    for(const auto& s: subs)
    {
        header
            << INDENT << "virtual " << vis.type << " visit" << s.name
            << "(const " << base_name << s.name <<"&) = 0;\n";
    }
    header << "};\n";
    header << "\n\n";
}


void
define_ast
(
    std::ofstream& source, std::ofstream& header,
    const std::string& base_name, const std::string& header_name,
    const std::vector<Sub>& subs,
    const std::vector<Vis>& visitors
)
{
    header << "#pragma once\n";
    header << "\n";
    header << "#include <memory>\n";
    header << "\n";
    header << "#include \"lox/token.h\"\n";
    header << "\n\n";
    header << "namespace lox\n";
    header << "{\n";
    header << "\n\n";
    for(const auto& sub: subs)
    {
        header << "struct " << base_name << sub.name << ";\n";
    }
    header << "\n\n";
    for(const auto& vis: visitors)
    {
        define_visitor(header, base_name, vis, subs);
    }
    header << "// ---------------------------------------------------------\n";
    header << "\n\n";

    header << "struct " << base_name << "\n";
    header << "{\n";
    header << INDENT << "virtual ~" << base_name << "() = default;\n";
    header << "\n";
    for(const auto& vis: visitors)
    {
        header << INDENT << "virtual " << vis.type << " accept(" << vis.name << "* vis) const = 0;\n";
    }
    header << "};\n";
    header << "\n\n";

    source << "#include \"" << header_name << "\"\n";
    source << "\n";
    source << "\n\n";
    source << "namespace lox\n";
    source << "{\n";
    source << "\n\n";
    
    header << "// ---------------------------------------------------------\n";
    header << "\n\n";

    for(const auto& sub: subs)
    {
        define_type(source, header, base_name, sub, visitors);
    }

    header << "}\n";
    header << "\n\n";

    source << "}\n";
    source << "\n\n";
}


int
main(int argc, char** argv)
{
    if(argc != 3)
    {
        std::cerr << "Invalid usage!\n";
        std::cout << "Usage " << argv[0] << " source header\n";
        return exit_codes::bad_input;
    }

    const std::string source_path = argv[1];
    const std::string header_path = argv[2];

    std::ofstream source(source_path.c_str());
    std::ofstream header(header_path.c_str());

    if(source.good() == false || header.good() == false)
    {
        std::cerr << "Failed to either open " << source_path << " or " << header_path << " for writing\n";
        return exit_codes::cant_create_output;
    }

    define_ast
    (
        source, header, "Expr", "expr.h",
        {
            {
                "Binary",
                {
                    {"Expr", "left"},
                    {"Token", "op"},
                    {"Expr", "right"}
                }
            },
            {
                "Grouping",
                {
                    {"Expr", "expression"}
                }
            },
            {
                "Literal",
                {
                    {"Object", "value"}
                }
            },
            {
                "Unary",
                {
                    {"Token", "op"},
                    {"Expr", "right"}
                }
            }
        },
        {
            {"VoidVisitor", "void"}
        }
    );

    return exit_codes::no_error;
}
