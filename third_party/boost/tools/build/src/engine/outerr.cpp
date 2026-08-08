/*
Copyright 2026 Paolo Pastori
Distributed under the Boost Software License, Version 1.0.
(See accompanying file LICENSE.txt or https://www.bfgroup.xyz/b2/LICENSE.txt)
*/

#include "jam.h"
#include "outerr.h"

#include "frames.h"
#include "lists.h"
#include "output.h"
#include "startup.h"


/* from builtins.cpp */
void get_source_line( FRAME * frame, char const * * file, int * line );

void print_source_line( FRAME * frame )
{
    char const * file;
    int line;
    get_source_line( frame, &file, &line );
    if ( line > 0 )
        out_printf( "%s:%d:", file, line );
    else if ( line == 0 )
        out_printf( "%s:", file );
    else
        out_printf( "(builtin):" );
}

/*
 * backtrace_line() - print a single line of error backtrace for the given
 * frame.
 */
void backtrace_line( FRAME * frame )
{
    if ( frame == 0 )
    {
        out_printf( "(no frame):" );
    }
    else
    {
        print_source_line( frame );
        out_printf( " in %s\n", frame->rulename );
    }
}

/*
 * backtrace() - Print the entire backtrace from the given frame to the Jambase
 * which invoked it.
 */
void backtrace( FRAME * frame )
{
    if ( !frame ) return;
    while ( ( frame = frame->prev ) )
        backtrace_line( frame );
}

/*
 * unknown_rule_error() - reports an unknown rule occurrence to the user and exits
 */
void unknown_rule_error( FRAME * frame, module_t * modul, OBJECT * rule_name )
{
    b2::list_ref msgs;

    msgs.push_back( b2::rule_and_args_to_string( frame ) );

    std::string msg = std::string("rule \"") + rule_name->str()
        + "\" unknown in ";
    msgs.push_back( modul->name
        ? msg + "module \"" + modul->name->str() + "\"."
        : msg + "root module." );

    b2::out_error( *msgs, frame );
}


namespace b2 {

std::string args_to_string(LOL * lol)
{
    std::string res;
    for (int32_t i = 0; i < lol->count; ++i)
    {
        if (i) res += " :";
        for ( auto el : list_cref(lol->list[i]) )
            res += std::string(" ") + el->str();
    }
    return res;
}

std::string rule_and_args_to_string(FRAME * frame)
{
    return std::string(frame->rulename) + args_to_string( frame->args );
}

/*
 * Error function that try to mimic the b2::jam::errors::backtrace.
 */
void out_emit(const char * prefix,
    LIST * messages, FRAME * frame,
    bool with_backtrace, bool exit)
{
    bool has_prev = false;
    if (frame)
    {
        has_prev = frame->prev != nullptr;
        backtrace_line( has_prev ? frame->prev : frame );
    }

    auto print_ln = [prefix](OBJECT * ln) {
        if (prefix) out_printf( "%s ", prefix );
        out_printf( "%s\n", ln->str() );
    };
    for (OBJECT * msg : list_cref(messages)) print_ln(msg);

    if (with_backtrace && has_prev) backtrace( frame->prev );

    if (exit) clean_exit( EXITBAD );
}

void out_error(LIST * messages, FRAME * frame)
{
    out_emit("error:", messages, frame, true, true);
}

void out_warning(LIST * messages, FRAME * frame)
{
    out_emit("warning:", messages, frame, false, false);
}

} // namespace b2
