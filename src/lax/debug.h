#pragma once

namespace lax
{
struct Chunk;

struct DebugOutput
{
    virtual ~DebugOutput() = default;

    virtual void on_line(const std::string& line) = 0;
};

void disassemble_chunk(DebugOutput* output, Chunk* chunk, std::string& name);
int disassemble_instruction(DebugOutput* output, const Chunk* chunk, int offset);

}
