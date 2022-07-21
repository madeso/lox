#pragma once

// based on
// https://www.freebsd.org/cgi/man.cgi?query=sysexits&apropos=0&sektion=0&manpath=FreeBSD+4.3-RELEASE&format=html

namespace exit_codes
{
    constexpr int no_error = 0;

    /**
    The command was used incorrectly, e.g., with the wrong number of arguments,
    a bad flag, a bad syntax in a parameter, or whatever.
    */
    constexpr int incorrect_usage = 64;

    /**
    The input data was incorrect in some way.  This should only be used
    for user's data and not system files.
    */
    constexpr int bad_input = 65;

    /**
    An input file (not a system file) did not exist or was not readable.
    This could also include errors like ``No message'' to a mailer
    (if it cared to catch it).
    */
    constexpr int missing_input = 66;

    /**
    The user specified did not exist.
    This might be used for mail addresses or remote logins.
    */
    constexpr int user_didnt_exist = 67;

    /**
    The host specified did not exist.  This is used in mail addresses or network requests.
    */
    constexpr int host_didnt_exist = 68;

    /**
    A service is unavailable.  This can occur if a sup­port program or file does not exist.
    This can also be used as a catchall message when something you wanted
    to do doesn't work, but you don't know why.
    */
    constexpr int service_unavailable = 69;

    /**
    An internal software error has been detected.  This should be limited to
    non-operating system related errors as possible.
    */
    constexpr int internal_error = 70;

    /**
    An operating system error has been detected.  This is intended to be used
    for such things as ``cannot fork'', ``cannot create pipe'', or the like.
    It includes things like getuid returning a user that does not exist
    in the passwd file.
    */
    constexpr int os_error = 71;

    /**
    Some system file (e.g., /etc/passwd, /var/run/utmp, etc.) does not exist,
    cannot be opened, or has some sort of error (e.g., syntax error).
    */
    constexpr int os_file_missing_or_bad = 72;

    /**
    A (user specified) output file cannot be created.
    */
    constexpr int cant_create_output = 73;

    /**
    An error occurred while doing I/O on some file.
    */
    constexpr int io_error = 74;

    /**
    Temporary failure, indicating something that is not really an error.
    In sendmail, this means that a mailer (e.g.) could not create a connection,
    and the request should be reattempted later.
    */
    constexpr int temporary_failure = 75;

    /**
    The remote system returned something that was ``not possible'' during a protocol exchange.
    */
    constexpr int not_possible_in_protocol = 76;

    /**
    You did not have sufficient permission to perform the operation.
    This is not intended for file sys­tem problems, which should use EX_NOINPUT or
    EX_CANTCREAT, but rather for higher level permis­sions.
    */
    constexpr int insufficient_permission = 77;

    /**
    Something was found in an unconfigured or miscon­figured state.
    */
    constexpr int bad_config = 78;
}
