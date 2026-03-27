#include "lax/chunk.h"

namespace lax
{
    void Chunk::write(std::uint8_t byte)
    {
        code.push_back(byte);
    }
}
