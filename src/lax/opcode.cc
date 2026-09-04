#include "lax/opcode.h"

#include <algorithm>
#include <unordered_map>

namespace lax
{

uint8_t byte_from_opcode(OpCode op)
{
    return static_cast<uint8_t>(op);
}

OpCode opcode_from_byte(uint8_t byte)
{
    return static_cast<OpCode>(byte);
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
to_lower_string(std::string data)
{
    std::transform(data.begin(), data.end(), data.begin(), ascii_to_lower);
    return data;
}

template<typename T>
struct StringMap
{
    std::unordered_map<std::string, T> from_string;
    std::unordered_map<T, std::string> to_string;

    StringMap<T>& add(T t, const std::string& str)
    {
        from_string.insert({to_lower_string(str), t});
        to_string.insert({t, str});
        return *this;
    }

    std::optional<std::string> to_string_or_null(T t) const
    {
        const auto it = to_string.find(t);
        if (it == to_string.end())
        {
            return std::nullopt;
        }
        else
        {
            return it->second;
        }
    }

    std::optional<T> from_string_or_null(const std::string& str) const
    {
        const auto it = from_string.find(to_lower_string(str));
        if (it == from_string.end())
        {
            return std::nullopt;
        }
        else
        {
            return it->second;
        }
    }
};

const StringMap<OpCode>& get_opcode_string_map()
{
    static const auto map = StringMap<OpCode>()
        .add(OpCode::Constant, "cst")
        .add(OpCode::Return, "ret")
    ;
    return map;
}

std::optional<OpCode> find_asm_keyword_or_null(std::string_view str)
{
    return get_opcode_string_map().from_string_or_null(std::string(str));
}

std::string string_from_opcode(OpCode op)
{
    return get_opcode_string_map().to_string_or_null(op).value_or("???");
}


}

