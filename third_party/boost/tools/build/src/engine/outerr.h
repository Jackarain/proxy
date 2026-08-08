/*
Copyright 2026 Paolo Pastori
Distributed under the Boost Software License, Version 1.0.
(See accompanying file LICENSE.txt or https://www.bfgroup.xyz/b2/LICENSE.txt)
*/

#ifndef B2_OUTERR_H
#define B2_OUTERR_H

#include "config.h"
#include "jam_fwd.h"

#include <string>

/*
 * Legacy error related functions.
 */
void print_source_line  ( FRAME * );
//void backtrace_line     ( FRAME * );
//void backtrace          ( FRAME * );
void unknown_rule_error ( FRAME * , module_t * , OBJECT * );

namespace b2 {

/*
 * Error functions and helpers.
 */

std::string args_to_string(LOL * lol);

std::string rule_and_args_to_string(FRAME * frame);

void out_emit(const char * prefix, LIST * messages,
    FRAME * frame, bool with_backtrace, bool exit);

void out_error(LIST * messages, FRAME * frame);
void out_warning(LIST * messages, FRAME * frame);

} // namespace b2

#endif
