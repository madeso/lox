#include "lax/chunk.h"

namespace lax
{

    void Chunk::write(std::uint8_t byte)
    {
        code.push_back(byte);
    }

    OpCode opcode_from_byte(std::uint8_t byte)
    {
        return static_cast<OpCode>(byte);
    }

}
