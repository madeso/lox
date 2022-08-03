#include "lox/printhandler.h"

#include "lox/stringmap.h"


namespace lox { namespace
{



std::string
get_line_gutter(std::size_t line)
{
    return "   {} | "_format(line);
}



std::string
get_marker_at(const LineData& line, std::size_t offset)
{
    assert(offset >= line.offset.start);
    assert(offset <= line.offset.end);

    const std::size_t line_length = offset - line.offset.start;
    const std::string gutter = get_line_gutter(line.line+1);

    const std::string dashes = std::string((gutter.length() + line_length) - 1, '-');
    return "{}^-- "_format(dashes);
}



std::string
get_underline_for(const LineData& line, const Offset& offset, char underline_char)
{
    assert(offset.end <= line.offset.end);
    assert(offset.start >= line.offset.start);

    const std::string gutter = get_line_gutter(line.line+1);

    const std::size_t start_char = offset.start - line.offset.start;
    const std::size_t length = offset.end - offset.start + 1;

    const std::string initial_spaces = std::string(gutter.length() + start_char, ' ');
    const std::string underline_string = std::string(length, underline_char);
    return "{}{} "_format(initial_spaces, underline_string);
}



void
print_line(PrintHandler* print, std::string_view current_source, const LineData& line)
{
    print->on_line("{}{}"_format
    (
        get_line_gutter(line.line+1),
        current_source.substr(line.offset.start, line.offset.end - line.offset.start)
    ));
}



void
print_message(PrintHandler* print, std::string_view current_source, std::string_view type, const Offset& offset, const std::string& message)
{
    const auto map = StringMap{current_source};
    const auto start_line = map.get_line_from_offset(offset.start);
    const auto end_line = map.get_line_from_offset(offset.end);

    if(start_line.line == end_line.line)
    {
        print_line(print, current_source, start_line);
        print->on_line("{}{}: {}"_format(
            get_underline_for(start_line, offset, '^'),
            type, message
        ));
    }
    else
    {
        print_line(print, current_source, end_line);
        print->on_line("{}{}: {}"_format(
            get_marker_at(end_line, offset.end),
            type, message
        ));

        print_line(print, current_source, start_line);
        print->on_line("{} starts here"_format(
            get_marker_at(start_line, offset.start)
        ));
    }
}





}}




namespace lox
{



PrintHandler::PrintHandler(std::string_view source)
    : current_source(source)
{
}

void
PrintHandler::on_error(const Offset& offset, const std::string& message)
{
    error_detected = true;
    print_message(this, current_source, "Error", offset, message);
}

void
PrintHandler::on_note(const Offset& offset, const std::string& message)
{
    print_message(this, current_source, "Note", offset, message);
}



}

