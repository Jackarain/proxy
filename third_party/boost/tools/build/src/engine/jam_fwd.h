/*
Copyright 2026 Paolo Pastori
Distributed under the Boost Software License, Version 1.0.
(See accompanying file LICENSE.txt or https://www.bfgroup.xyz/b2/LICENSE.txt)
*/

#ifndef B2_JAM_FWD_H
#define B2_JAM_FWD_H

// forward declarations, to avoid header inclusion.

// frames.h
using FRAME = struct frame;

// function.h
using FUNCTION = struct _function;
using function_ptr = FUNCTION *;

// jam_strings.h
using string = struct string;

// lists.h
struct LIST;
using list_ptr = LIST *;
struct LOL;

// modules.h
using module_t = struct module_t;
using module_ptr = module_t *;

// object.h, see below

// parse.h
using PARSE = struct _PARSE;

// pathsys.h
using PATHNAME = struct _pathname;

// regexp.h
namespace b2 { namespace regex {
struct program;
}} // namespace b2::regex

// rules.h
using RULE = struct _rule;
using TARGET = struct _target;

// timestamp.h
using timestamp = struct timestamp;

// value.h
namespace b2 {
struct value;
using value_ptr = value *;
} // namespace b2

// object.h
using OBJECT = b2::value;

#endif
