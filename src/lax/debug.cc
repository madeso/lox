#include "lax/debug.h"

#include "lax/chunk.h"

namespace lax
{

void disassemble_chunk(DebugOutput* output, Chunk* chunk, std::string& name)
{
    output->on_line(fmt::format("== {} ==\n", name));

    for (std::size_t offset = 0; offset < chunk->code.size();) {
        offset = disassemble_instruction(output, chunk, offset);
    }
}

static std::size_t disassemble_simple_instruction(DebugOutput* output, const char* name, std::size_t offset) {
    output->on_line(fmt::format("{:04d} {}", offset, name));
    return offset + 1;
}

std::size_t disassemble_instruction(DebugOutput* output, const Chunk* chunk, std::size_t offset)
{
    const auto instruction_byte = chunk->code[offset];
    switch (opcode_from_byte(instruction_byte)) {
    case OpCode::Return:
        return disassemble_simple_instruction(output, "OP_RETURN", offset);
    default:
        output->on_line(fmt::format("{:04d} Unknown instruction {}", offset, instruction_byte));
        return offset + 1;
    }
}

}
