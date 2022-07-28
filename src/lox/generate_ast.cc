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


bool
starts_with(const std::string& lhs, const std::string& in)
{
    return lhs.substr(0, in.length()) == in;
}

bool
is_value_type(const std::string& type)
{
    return type == "Token"
        || type == "TokenType"
        || type == "Offset"
        || type == "std::string"
        || starts_with(type, "std::vector<")
        ;
}

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

    source << "////////////////////////////////////////////////////////////\n";
    source << "// " << base_name << sub.name << "\n";
    source << "\n";
    

    const std::string_view expl = sub.members.size() == 0 ? "explicit " : "";

    header << INDENT << expl << base_name << sub.name << "\n";
    header << INDENT << "(\n";
    source << base_name << sub.name << "::" << base_name << sub.name<< "\n";
    source << "(\n";
    header << INDENT << INDENT << "const Offset&" << " " << "offset";
    source << INDENT << "const Offset&" << " " << "the_offset";
    for(const auto& m: sub.members)
    {
        
        header << ",\n";
        source << ",\n";
        const std::string type = is_value_type(m.type) ? m.type : fmt::format("std::unique_ptr<{}>&&", m.type);
        header << INDENT << INDENT << type << " " << m.name;
        source << INDENT << type << " " << "a" << m.name;
    } header << "\n"; source << "\n";
    header << INDENT << ");\n";
    header << "\n";

    source << ")\n";
    source << INDENT << ':' << " " << base_name << "(the_offset)\n";
    for(const auto& m: sub.members)
    {
        source << INDENT << ',' << " " << m.name << "(std::move(a" << m.name << "))\n";
    }
    source << "{\n";
    source << "}\n";
    source << "\n\n";

    for(const auto& m: sub.members)
    {
        const std::string type = is_value_type(m.type) ? m.type : fmt::format("std::unique_ptr<{}>", m.type);
        header << INDENT << type << " " << m.name << ";\n";
    }

    header << "\n";
    header << INDENT << base_name << "Type get_type() const override;\n";
    source << base_name << "Type " << base_name << sub.name << "::get_type() const\n";
    source << "{\n";
    source << INDENT << "return " << base_name << "Type::" << sub.name << ";\n";
    source << "}\n";
    source << "\n\n";

    // header << "\n";
    for(const auto& v: visitors)
    {
        header << INDENT << v.type << " accept" << "(" << base_name << v.name << "* vis) const override;\n";
        
        source << v.type << " " << base_name << sub.name << "::accept(" << base_name << v.name << "* vis) const\n";
        source << "{\n";
        const std::string ret = v.type == "void" ? "" : "return ";
        source << INDENT << ret << "vis ->visit" << sub.name << "(*this);\n";
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
    header << "struct " << base_name << vis.name << "\n";
    header << "{\n";
    header << INDENT << "virtual ~" << base_name << vis.name << "() = default;\n";
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
    const std::string& base_name,
    const std::vector<Sub>& subs,
    const std::vector<Vis>& visitors
)
{
    for(const auto& sub: subs)
    {
        header << "struct " << base_name << sub.name << ";\n";
    }
    header << "\n\n";
    header << "enum class " << base_name << "Type\n";
    header << "{\n";
    {bool first = true;
    for(const auto& sub: subs)
    {
        if(first) first = false;
        else header << ",\n";
        header << INDENT << sub.name;
    }}
    header << "\n};\n";
    header << "\n\n";
    for(const auto& vis: visitors)
    {
        define_visitor(header, base_name, vis, subs);
    }
    header << "// ---------------------------------------------------------\n";
    header << "\n\n";

    header << "struct " << base_name << "\n";
    header << "{\n";
    header << INDENT << "constexpr explicit " << base_name << "(const Offset& o) : offset(o) {}\n";
    header << INDENT << "virtual ~" << base_name << "() = default;\n";
    header << "\n";
    header << INDENT << "virtual " << base_name << "Type get_type() const = 0;\n";
    header << "\n";
    header << INDENT << "Offset offset;\n";
    header << "\n";
    for(const auto& vis: visitors)
    {
        header << INDENT << "virtual " << vis.type << " accept(" << base_name << vis.name << "* vis) const = 0;\n";
    }
    header << "};\n";
    header << "\n\n";

    
    
    header << "// ---------------------------------------------------------\n";
    header << "\n\n";

    for(const auto& sub: subs)
    {
        define_type(source, header, base_name, sub, visitors);
    }
}



void
write_code
(
    std::ofstream& source, std::ofstream& header,
    const std::string& header_name
)
{
    header << "#pragma once\n";
    header << "\n";
    header << "#include <memory>\n";
    header << "#include <string>\n";
    header << "\n";
    header << "#include \"lox/token.h\"\n";
    header << "#include \"lox/object.h\"\n";
    header << "#include \"lox/offset.h\"\n";
    header << "\n\n";
    header << "namespace lox\n";
    header << "{\n";
    header << "\n\n";

    source << "#include \"" << header_name << "\"\n";
    source << "\n";
    source << "\n\n";
    source << "namespace lox\n";
    source << "{\n";
    source << "\n\n";

    define_ast
    (
        source, header, "Expr",
        {
            {
                "Assign",
                {
                    {"std::string", "name"},
                    {"Offset", "name_offset"},
                    {"Expr", "value"}
                }
            },
            {
                "Binary",
                {
                    {"Expr", "left"},
                    {"TokenType", "op"},
                    {"Offset", "op_offset"},
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
                    {"TokenType", "op"},
                    {"Offset", "op_offset"},
                    {"Expr", "right"}
                }
            },
            {
                "Variable",
                {
                    {"std::string", "name"}
                }
            }
        },
        {
            {"StringVisitor", "std::string"},
            {"ObjectVisitor", "std::shared_ptr<Object>"}
        }
    );
    
    define_ast
    (
        source, header, "Stmt",
        {
            {
                "Block",
                {
                    {"std::vector<std::shared_ptr<Stmt>>", "statements"}
                }
            },
            {
                "Expression",
                {
                    {"Expr", "expression"}
                }
            },
            {
                "Print",
                {
                    {"Expr", "expression"}
                }
            },
            {
                "Var",
                {
                    {"std::string", "name"},
                    {"Expr", "initializer"}
                }
            }
        },
        {
            {"VoidVisitor", "void"},
            {"StringVisitor", "std::string"}
        }
    );

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
    
    write_code(source, header,  "lox_expression.h");

    return exit_codes::no_error;
}
