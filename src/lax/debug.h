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
std::size_t disassemble_instruction(DebugOutput* output, const Chunk* chunk, std::size_t offset);

}
