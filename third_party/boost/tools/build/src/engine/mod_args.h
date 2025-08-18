/*
Copyright 2025 René Ferdinand Rivera Morell
Distributed under the Boost Software License, Version 1.0.
(See accompanying file LICENSE.txt or https://www.bfgroup.xyz/b2/LICENSE.txt)
*/

#ifndef B2_MOD_ARGS_H
#define B2_MOD_ARGS_H

#include "bind.h"
#include "ext_bfgroup_lyra.h"
#include "lists.h"
#include "value.h"

/* tag::reference[]

[[b2.reference.modules.args]]
= `args` module.

end::reference[] */

namespace b2 { namespace args {

/* tag::reference[]

== `b2::args::add_arg`

====
[horizontal]
Jam:: `rule add-arg ( name : opts + : help : flags * )`
{CPP}:: `void add_arg(const value_ref & name, list_cref opts, const value_ref &
help, list_cref flags);`
====

Declares a new command line argument option to accept. When specified the
argument is then available for access with `get-arg`. The `name` is a symbolic
tag, used for both retrieving the option value and for the help (`-h`)
displayed. The `opts` is a set of option names that are accepted for this
argument. Both short and long variations are valid for `opts`. The `help` is
a description of the argument printed out in the help output (`-h`). The `flags`
value can indicate definition characteristics for the argument. Accepted `flags`
are:

`flag`:: Declares this as an option that has no value. For example `--version`
as opposed to `--prefix=/some/path`.

end::reference[] */
void add_arg(const value_ref & name,
	list_cref opts,
	const value_ref & help,
	list_cref flags);

/* tag::reference[]

== `b2::args::get_arg`

====
[horizontal]
Jam:: `rule get-arg ( name )`
{CPP}:: `list_ref get_arg(value_ref name);`
====

Retrieve the value of a previous specified command line argument.

end::reference[] */
list_ref get_arg(const value_ref & name);

/* tag::reference[]

== `b2::args::has_arg`

====
[horizontal]
Jam:: `rule has-arg ( name )`
{CPP}:: `bool has_arg(value_ref name);`
====

Checks if an argument was previously added.

end::reference[] */
bool has_arg(const value_ref & name);

void set_args(int argc, char ** argv);
lyra::cli & lyra_cli();
void process_args(bool silent = false);

struct args_module : b2::bind::module_<args_module>
{
	const char * module_name = "args";
	static const char * init_code;

	template <class Binder>
	void def(Binder & binder)
	{
		binder
			.def(&add_arg, "add-arg",
				"name" * _1 | "opts" * _1n | "help" * _1 | "flags" * _n)
			.def(&get_arg, "get-arg", "name" * _1)
			.def(&has_arg, "has-arg", "name" * _1);
		binder.eval(init_code);
		binder.loaded();
	}
};

template <typename SubType>
struct declaration_
{
	declaration_() { SubType::declare_args(); }
};

}} // namespace b2::args

#endif
