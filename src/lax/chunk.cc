#include "lax/chunk.h"

#include <iomanip>
#include <sstream>

namespace lax
{

    void Chunk::write(uint8_t byte, SourceRange range)
    {
        code.push_back(byte);
        source.ranges.push_back(range);
    }

    void Chunk::write(OpCode op, SourceRange range)
    {
        write(byte_from_opcode(op), range);
    }

    // printing

    std::size_t simple_instruction(std::ostream& s, const char* name, std::size_t offset) {
        s << name << "\n";
        return offset + 1;
    }

    std::size_t constant_instruction(std::ostream& s, const char* name, const Chunk& chunk, std::size_t offset) {
        const auto constant = chunk.code[offset + 1];
        const auto str = constant < chunk.constants.values.size() ? string_from_value(chunk.constants.values[constant]) : "<invalid index>";
        s << std::left << std::setw(16) << std::setfill(' ') << name << ' '
            << std::setw(4) << std::setfill(' ') << static_cast<int>(constant) << " '" << str << "'\n";
        return offset + 2;
    }

    std::size_t disassemble_instruction(std::ostream& s, const Chunk& chunk, std::size_t offset) {
        s << std::setw(4) << std::setfill('0') << offset << ' ';

        if (offset > 0 &&
            chunk.source.ranges[offset] == chunk.source.ranges[offset - 1]) {
            s << "    |     ";
        }
        else {
            s
                << std::right << std::setw(4) << std::setfill(' ') << chunk.source.ranges[offset].start
                << '-'
                << std::left << std::setw(4) << std::setfill(' ') << chunk.source.ranges[offset].end << " ";
        }

        const auto instruction = chunk.code[offset];
        switch (opcode_from_byte(instruction)) {
        case OpCode::Constant:
            return constant_instruction(s, "Constant", chunk, offset);
        case OpCode::Return:
            return simple_instruction(s, "Return", offset);
        default:
            s << "Unknown opcode " << instruction << "\n";
            return offset + 1;
        }
    }

    std::string disassemble_chunk(const Chunk& chunk, const char* name)
    {
        std::ostringstream ss;

        ss << "== " << name << " ==\n";

        for (std::size_t offset = 0; offset < chunk.code.size();) {
            offset = disassemble_instruction(ss, chunk, offset);
        }

        return ss.str();
    }

    std::size_t Chunk::add_constant(Value v)
    {
        constants.write(v);
        return constants.values.size() - 1;
    }
}
