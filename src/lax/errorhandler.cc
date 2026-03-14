#include "lax/errorhandler.h"

namespace lax
{



void ErrorHandler::on_error(const Offset& where, const std::string& messages)
{
    on_errors(where, {messages});
}


void ErrorHandler::on_note(const Offset& where, const std::string& messages)
{
    on_notes(where, {messages});
}



}
