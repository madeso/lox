#pragma once

namespace lax
{

enum class OpCode : uint8_t
{
    Constant,
    Return
};

OpCode opcode_from_byte(uint8_t byte);
uint8_t byte_from_opcode(OpCode op);

std::optional<OpCode> find_asm_keyword_or_null(std::string_view str);
std::string string_from_opcode(OpCode op);

}
