#include <iostream>
#include <fstream>
#include <string>
#include <string_view>
#include <vector>

#include <fmt/format.h>

#include "exit_codes/exit_codes.h"


using namespace fmt::literals;



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

char ascii_to_lower(char in)
{
    if (in <= 'Z' && in >= 'A')
    {
        return in - ('Z' - 'z');
    }
    return in;
}

std::string
to_lower(std::string data)
{
    std::transform(data.begin(), data.end(), data.begin(), ascii_to_lower);
    return data;
}

std::string
get_fun_visit_name(const std::string& name, const std::string& base)
{
    return "on_{}_{}"_format(to_lower(name), to_lower(base));
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
    header << "struct " << sub.name << base_name << " : " << base_name << "\n";
    header << "{\n";

    source << "////////////////////////////////////////////////////////////\n";
    source << "// " << sub.name << base_name << "\n";
    source << "\n";
    

    header << INDENT << sub.name << base_name << "\n";
    header << INDENT << "(\n";
    source << sub.name << base_name << "::" << sub.name << base_name << "\n";
    source << "(\n";
    header << INDENT << INDENT << "const Offset&" << " " << "offset" << ",\n";
    source << INDENT << "const Offset&" << " " << "the_offset" << ",\n";
    header << INDENT << INDENT << "const " << base_name << "Id&" << " " << "id";
    source << INDENT << "const " << base_name << "Id&" << " " << "the_id";
    for(const auto& m: sub.members)
    {
        
        header << ",\n";
        source << ",\n";
        const std::string type = is_value_type(m.type) ? m.type : "std::shared_ptr<{}>&&"_format(m.type);
        header << INDENT << INDENT << type << " " << m.name;
        source << INDENT << type << " " << "a" << m.name;
    } header << "\n"; source << "\n";
    header << INDENT << ");\n";
    header << "\n";

    source << ")\n";
    source << INDENT << ':' << " " << base_name << "(the_offset, the_id)\n";
    for(const auto& m: sub.members)
    {
        source << INDENT << ',' << " " << m.name << "(a" << m.name << ")\n";
    }
    source << "{\n";
    source << "}\n";
    source << "\n\n";

    for(const auto& m: sub.members)
    {
        const std::string type = is_value_type(m.type) ? m.type : "std::shared_ptr<{}>"_format(m.type);
        header << INDENT << type << " " << m.name << ";\n";
    }

    header << "\n";
    header << INDENT << base_name << "Type get_type() const override;\n";
    source << base_name << "Type " << sub.name << base_name << "::get_type() const\n";
    source << "{\n";
    source << INDENT << "return " << base_name << "Type::" << to_lower(sub.name) << "_" << to_lower(base_name) << ";\n";
    source << "}\n";
    source << "\n\n";

    // header << "\n";
    for(const auto& v: visitors)
    {
        header << INDENT << v.type << " accept" << "(" << base_name << v.name << "* vis) const override;\n";
        
        source << v.type << " " << sub.name << base_name << "::accept(" << base_name << v.name << "* vis) const\n";
        source << "{\n";
        const std::string ret = v.type == "void" ? "" : "return ";
        source << INDENT << ret << "vis->" << get_fun_visit_name(sub.name, base_name) << "(*this);\n";
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
            << INDENT << "virtual " << vis.type << " " << get_fun_visit_name(s.name, base_name)
            << "(const "<< s.name << base_name <<"&) = 0;\n";
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
        header << "struct " << sub.name << base_name << ";\n";
    }
    header << "\n\n";
    header << "enum class " << base_name << "Type\n";
    header << "{\n";
    {bool first = true;
    for(const auto& sub: subs)
    {
        if(first) first = false;
        else header << ",\n";
        header << INDENT << to_lower(sub.name) << "_" << to_lower(base_name);
    }}
    header << "\n};\n";
    header << "\n\n";
    for(const auto& vis: visitors)
    {
        define_visitor(header, base_name, vis, subs);
    }
    header << "// ---------------------------------------------------------\n";
    header << "\n\n";

    source << "//////////////////////////////////////////////////////////////\n";
    source << "// " << base_name << " \n\n";
    source << base_name << "::" << base_name << "(const Offset& o, const " << base_name << "Id& i) : offset(o), uid(i) {}\n";
    source << "\n\n\n";

    header << "struct " << base_name << "Id { u64 value; };\n";
    header << "\n\n";
    header << "struct " << base_name << "\n";
    header << "{\n";
    header << INDENT << "" << base_name << "(const Offset& o, const " << base_name << "Id& i);\n";
    header << INDENT << "virtual ~" << base_name << "() = default;\n";
    header << "\n";
    header << INDENT << "virtual " << base_name << "Type get_type() const = 0;\n";
    header << "\n";
    header << INDENT << "Offset offset;\n";
    header << INDENT << base_name << "Id uid;\n";
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
    header << "#include \"lox/source.h\"\n";
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
        source, header, "Expression",
        {
            {
                "Assign",
                {
                    {"std::string", "name"},
                    {"Offset", "name_offset"},
                    {"Expression", "value"}
                }
            },
            {
                "Binary",
                {
                    {"Expression", "left"},
                    {"TokenType", "op"},
                    {"Offset", "op_offset"},
                    {"Expression", "right"}
                }
            },
            {
                "Call",
                {
                    {"Expression", "callee"},
                    {"std::vector<std::shared_ptr<Expression>>", "arguments"}
                }
            },
            {
                "Grouping",
                {
                    {"Expression", "expression"}
                }
            },
            {
                "Literal",
                {
                    {"Object", "value"}
                }
            },
            {
                "Logical",
                {
                    {"Expression", "left"},
                    {"TokenType", "op"},
                    {"Expression", "right"}
                }
            },
            {
                "Unary",
                {
                    {"TokenType", "op"},
                    {"Offset", "op_offset"},
                    {"Expression", "right"}
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
            {"VoidVisitor", "void"},
            {"StringVisitor", "std::string"},
            {"ObjectVisitor", "std::shared_ptr<Object>"}
        }
    );
    
    define_ast
    (
        source, header, "Statement",
        {
            {
                "Block",
                {
                    {"std::vector<std::shared_ptr<Statement>>", "statements"}
                }
            },
            {
                "Function",
                {
                    {"std::string", "name"},
                    {"std::vector<std::string>", "params"},
                    {"std::vector<std::shared_ptr<Statement>>", "body"}
                }
            },
            {
                "Expression",
                {
                    {"Expression", "expression"}
                }
            },
            {
                "If",
                {
                    {"Expression", "condition"},
                    {"Statement", "then_branch"},
                    {"Statement", "else_branch"}
                }
            },
            {
                "Print",
                {
                    {"Expression", "expression"}
                }
            },
            {
                "Return",
                {
                    {"Expression", "value"}
                }
            },
            {
                "Var",
                {
                    {"std::string", "name"},
                    {"Expression", "initializer"}
                }
            },
            {
                "While",
                {
                    {"Expression", "condition"},
                    {"Statement", "body"}
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
