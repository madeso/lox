#include "lox/errorhandler.h"


namespace lox
{


void ErrorHandler::error(int line, const std::string& message)
{
    report(line, "", message);
}

}
