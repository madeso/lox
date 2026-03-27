#include "lax/debug.h"

#include "lax/chunk.h"

namespace lax
{

void disassemble_chunk(DebugOutput* output, Chunk* chunk, std::string& name)
{
    output->on_line(fmt::format("== {} ==\n", name));

    for (int offset = 0; offset < chunk->code.size();) {
        offset = disassemble_instruction(output, chunk, offset);
    }
}

static int disassemble_simple_instruction(DebugOutput* output, const char* name, int offset) {
    output->on_line(fmt::format("{:04d} {}", offset, name));
    return offset + 1;
}

int disassemble_instruction(DebugOutput* output, const Chunk* chunk, int offset)
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
