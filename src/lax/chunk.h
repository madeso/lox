#pragma once

namespace lax
{

enum class OpCode : std::uint8_t
{
    Return
};

OpCode opcode_from_byte(std::uint8_t byte);

struct Chunk
{
    std::vector<std::uint8_t> code;

    void write(std::uint8_t byte);
};

}
