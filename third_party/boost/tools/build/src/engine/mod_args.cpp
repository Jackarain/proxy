/*
Copyright 2024 René Ferdinand Rivera Morell
Distributed under the Boost Software License, Version 1.0.
(See accompanying file LICENSE.txt or https://www.bfgroup.xyz/b2/LICENSE.txt)
*/

#include "mod_args.h"

#include "jam.h"
#include "lists.h"
#include "output.h"
#include "startup.h"
#include "value.h"

#include <memory>
#include <sstream>
#include <string>
#include <unordered_map>
#include <vector>

namespace b2 { namespace args {

namespace {
struct args_reg
{
	static args_reg & ref()
	{
		static args_reg r;
		return r;
	}

	lyra::cli cli;
	std::unique_ptr<lyra::parse_result> result;
	std::unordered_map<std::string, std::shared_ptr<list_ref>> options;
	std::vector<std::string> args;
	bool need_reparse = true;

	args_reg() { cli.style_print_short_first(); }

	void set_args(int argc, char ** argv)
	{
		args.clear();
		for (int i = 0; i < argc; ++i)
		{
			args.emplace_back(argv[i]);
		}
	}

	void reparse()
	{
		need_reparse = false;
		// We can call this multiple times. Hence we go back to a clean state
		// on the collected values to get reproducible parsing.
		result.reset();
		for (auto & o : options)
		{
			o.second->reset();
		}

		result.reset(new lyra::parse_result(
			cli.parse(lyra::args(args.begin(), args.end()))));
	}

	void add_opt(const value_ref & name,
		list_cref opts,
		const value_ref & help,
		list_cref flags)
	{
		if (options.count(name) > 0) return;
		need_reparse = true;
		bool is_flag = false;
		for (auto f : flags)
		{
			if (f->equal_to(*value_ref("flag"))) is_flag = true;
		}
		std::shared_ptr<list_ref> values = std::make_shared<list_ref>();
		std::unique_ptr<lyra::opt> arg;
		if (is_flag)
			arg.reset(new lyra::opt([values](bool v) {
				values->push_back("true");
			}));
		else
			arg.reset(new lyra::opt(
				[values](const std::string & v) { values->push_back(v); },
				name->str()));
		arg->help(help);
		for (auto opt : opts)
		{
			arg->name(opt->str());
		}
		cli.add_argument(*arg);
		options[name->str()] = values;
	}

	list_ref get_opt(const value_ref & name)
	{
		if (need_reparse) reparse();
		if (options.count(name->str()) > 0) return *options[name->str()];
		if (name == "help")
			return globs.display_help ? list_ref("true") : list_ref();
		if (name == "debug-configuration")
			return globs.debug_configuration ? list_ref("true") : list_ref();
		return {};
	}

	bool has_opt(const value_ref & name) { return (options.count(name) > 0); }
};
} // namespace

void add_arg(
	const value_ref & name, list_cref opts, const value_ref & help, list_cref flags)
{
	args_reg::ref().add_opt(name, opts, help, flags);
}

list_ref get_arg(const value_ref & name)
{
	return args_reg::ref().get_opt(name);
}

bool has_arg(const value_ref & name) { return args_reg::ref().has_opt(name); }

void set_args(int argc, char ** argv) { args_reg::ref().set_args(argc, argv); }

lyra::cli & lyra_cli() { return args_reg::ref().cli; }

void process_args(bool silent)
{
	args_reg::ref().reparse();
	if (!silent && globs.display_help)
	{
		std::ostringstream out;
		out << args_reg::ref().cli;
		err_puts(out.str().c_str());
		b2::clean_exit(EXITOK);
	}
}

const char * args_module::init_code = R"jam(
rule __test__ ( )
{
    import assert ;
}
)jam";

}} // namespace b2::args
