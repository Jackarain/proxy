/*
 * /+\
 * +\   Copyright 1993-2002 Christopher Seiwald and Perforce Software, Inc.
 * \+/
 *
 * This file is part of jam.
 *
 * License is hereby granted to use this software and distribute it freely, as
 * long as this copyright notice is retained and modifications are clearly
 * marked.
 *
 * ALL WARRANTIES ARE HEREBY DISCLAIMED.
 */

/* This file is ALSO:
 * Copyright 2001-2004 David Abrahams.
 * Copyright 2018-2025 Rene Rivera
 * Distributed under the Boost Software License, Version 1.0.
 * (See accompanying file LICENSE.txt or copy at
 * https://www.bfgroup.xyz/b2/LICENSE.txt)
 */

/*
 * jam.c - make redux
 *
 * See Jam.html for usage information.
 *
 * These comments document the code.
 *
 * The top half of the code is structured such:
 *
 *                       jam
 *                      / | \
 *                 +---+  |  \
 *                /       |   \
 *         jamgram     option  \
 *        /  |   \              \
 *       /   |    \              \
 *      /    |     \             |
 *  scan     |     compile      make
 *   |       |    /  | \       / |  \
 *   |       |   /   |  \     /  |   \
 *   |       |  /    |   \   /   |    \
 * jambase parse     |   rules  search make1
 *                   |           |      |   \
 *                   |           |      |    \
 *                   |           |      |     \
 *               builtins    timestamp command execute
 *                               |
 *                               |
 *                               |
 *                             filesys
 *
 *
 * The support routines are called by all of the above, but themselves are
 * layered thus:
 *
 *                     variable|expand
 *                      /      |   |
 *                     /       |   |
 *                    /        |   |
 *                 lists       |   pathsys
 *                    \        |
 *                     \      hash
 *                      \      |
 *                       \     |
 *                        \    |
 *                         \   |
 *                          \  |
 *                         object
 *
 * Roughly, the modules are:
 *
 *  builtins.c - jam's built-in rules
 *  command.c - maintain lists of commands
 *  compile.c - compile parsed jam statements
 *  exec*.c - execute a shell script on a specific OS
 *  file*.c - scan directories and archives on a specific OS
 *  hash.c - simple in-memory hashing routines
 *  hdrmacro.c - handle header file parsing for filename macro definitions
 *  headers.c - handle #includes in source files
 *  jamgram.y - jam grammar
 *  lists.c - maintain lists of strings
 *  make.c - bring a target up to date, once rules are in place
 *  make1.c - execute command to bring targets up to date
 *  object.c - string manipulation routines
 *  option.c - command line option processing
 *  parse.c - make and destroy parse trees as driven by the parser
 *  path*.c - manipulate file names on a specific OS
 *  hash.c - simple in-memory hashing routines
 *  regexp.c - Henry Spencer's regexp
 *  rules.c - access to RULEs, TARGETs, and ACTIONs
 *  scan.c - the jam yacc scanner
 *  search.c - find a target along $(SEARCH) or $(LOCATE)
 *  timestamp.c - get the timestamp of a file or archive member
 *  variable.c - handle jam multi-element variables
 */

#include "jam.h"

#include "patchlevel.h"

#include "bindjam.h"
#include "builtins.h"
#include "class.h"
#include "compile.h"
#include "constants.h"
#include "cwd.h"
#include "debugger.h"
#include "events.h"
#include "execcmd.h"
#include "filesys.h"
#include "function.h"
#include "hcache.h"
#include "jam_strings.h"
#include "lists.h"
#include "make.h"
#include "mod_args.h"
#include "mod_command_db.h"
#include "mod_sysinfo.h"
#include "modules.h"
#include "object.h"
#include "output.h"
#include "parse.h"
#include "rules.h"
#include "scan.h"
#include "search.h"
#include "startup.h"
#include "timestamp.h"
#include "variable.h"

#include <cerrno>
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

/* Keep JAMVERSYM in sync with VERSION. */
/* It can be accessed as $(JAMVERSION) in the Jamfile. */
#define JAM_STRINGIZE(X) JAM_DO_STRINGIZE(X)
#define JAM_DO_STRINGIZE(X) #X
#define VERSION_MAJOR_SYM JAM_STRINGIZE(VERSION_MAJOR)
#define VERSION_MINOR_SYM JAM_STRINGIZE(VERSION_MINOR)
#define VERSION_PATCH_SYM JAM_STRINGIZE(VERSION_PATCH)
#define VERSION VERSION_MAJOR_SYM "." VERSION_MINOR_SYM "." VERSION_PATCH_SYM
#define JAMVERSYM "JAMVERSION=" VERSION

/* And UNIX for this. */
#ifdef unix
#include <sys/utsname.h>
#endif

#ifdef WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#endif

#include "ext_bfgroup_lyra.h"

/* on Win32-LCC */
#if defined(OS_NT) && defined(__LCC__)
#define use_environ _environ
#endif

#if defined(__MWERKS__)
#define use_environ _environ
extern char ** _environ;
#endif

#ifndef use_environ
#define use_environ environ
#if !defined(__WATCOM__) && !defined(OS_OS2) && !defined(OS_NT)
extern char ** environ;
#endif
#endif

#if YYDEBUG != 0
extern int yydebug;
#endif

#ifndef NDEBUG
static void run_unit_tests()
{
#if defined(USE_EXECNT)
	extern void execnt_unit_test();
	execnt_unit_test();
#endif
	string_unit_test();
}
#endif

char const * saved_argv0 = nullptr;

struct global_config globs;

void global_config::out_print() const
{
	out_printf("global_config:\n");
	auto true_false = [](bool v) { return v ? "true" : "false"; };
	out_printf("  > anyhow: %s\n", true_false(globs.anyhow));
	out_printf("  > noexec: %s\n", true_false(globs.noexec));
	out_printf("  > jobs: %d\n", globs.jobs);
	out_printf("  > quitquick: %s\n", true_false(globs.quitquick));
	out_printf("  > newestfirst: %s\n", true_false(globs.newestfirst));
	out_printf("  > debug:");
	for (auto v : globs.debug) out_printf(" %s", true_false(v));
	out_puts("\n");
	out_printf("  > timeout: %d\n", int(globs.timeout));
	out_printf("  > max_buf: %d\n", globs.max_buf);
	out_printf("  > is_debugger: %s\n", true_false(globs.is_debugger));
	out_printf("  > debug_interface: %d\n", int(globs.debug_interface));
}

struct args_data
{
	std::vector<std::string> targets_to_touch;
	std::vector<std::string> variables_to_set;
	std::vector<std::string> debug_handles;
	std::vector<std::string> files_to_parse;
	std::vector<std::string> extra_args;
	std::string out_filename;
} args_data;

int guarded_main(int argc, char * argv[])
{
	int status = 0;
	int arg_c = argc;
	char ** arg_v = argv;
	module_t * environ_module;
	b2::system_info sys_info;

	auto & cli = b2::args::lyra_cli().relaxed();

	cli |= lyra::arg(args_data.extra_args, "request")
			   .help("Targets, requirements, etc.")
			   .choices([](const std::string & v) {
				   return v.empty() || v[0] != '-';
			   })
			   .cardinality(0, 0);

	cli |= lyra::opt(globs.display_help)
			   .name("-?")
			   .name("-h")
			   .help("Display invocation help.");

	/* Version info. */
	cli |= lyra::opt([](bool) {
		out_printf("B2 %s (%s, jobs=%i)\n\n", VERSION, OSMINOR, globs.jobs);
		b2::clean_exit(b2::exit_result::success);
	})
			   .name("-v")
			   .help("Print the version of jam and exit.");

	cli |= lyra::opt(globs.anyhow)
			   .name("-a")
			   .help("Build all targets, even if they are current.");

	cli |= lyra::opt([](bool) {
		globs.noexec = true;
		if (!globs.debug_flag_used) globs.debug[2] = true;
	})
			   .name("-n")
			   .help("Don't actually execute the updating actions.");

	/* Undocumented -p3 (acts like both -p1 -p2) means separate pipe action
	 * stdout and stderr.
	 */
	cli |= lyra::opt(globs.pipe_action, "x")
			   .name("-p")
			   .help(
				   "x=0, pipes action stdout and stderr"
				   "merged into action output.")
			   .choices([](int val) { return 0 <= val && val <= 3; });

	cli |= lyra::opt(globs.quitquick)
			   .name("-q")
			   .help("Quit quickly as soon as a target fails.");

	cli |= lyra::opt(
		[](const std::string & v) {
			globs.quitquick = (v == "on" || v == "yes" || v == "true");
		},
		"x")
			   .name("--keep-going")
			   .help(
				   "Specify if we continue to build after failures or not "
				   "(--keep-going=no is equivalent to -q)")
			   .choices("on", "yes", "true", "off", "no", "false");

	cli |= lyra::opt(globs.jobs, "x")
			   .name("-j")
			   .name("--jobs")
			   .help("Run up to x shell commands concurrently.")
			   .choices([](int val) {
				   if (val < 1)
				   {
					   err_printf("Invalid value for the '-j' option.\n");
					   b2::clean_exit(EXITBAD);
					   return false;
				   }
				   globs.jobs = val;
				   return true;
			   });

	cli |= lyra::opt(globs.newestfirst)
			   .name("-g")
			   .help("Build from newest sources first.");

	cli |= lyra::opt(globs.timeout, "x")
			   .name("-l")
			   .help(
				   "Limit actions to x number of seconds"
				   "after which they are stopped.");

	cli |= lyra::opt(
		[](const std::string & x) {
			globs.max_buf = std::stoi(x) * 1024; /* convert to kb */
		},
		"x")
			   .name("-m")
			   .help(
				   "Maximum target output saved (kb),"
				   "default is to save all output.");

	cli |= lyra ::opt(
		[](const std::string & val) {
			/* Turn on/off debugging */
			/* First -d, turn off defaults. */
			if (!globs.debug_flag_used)
				for (bool & d : globs.debug) d = false;
			globs.debug_flag_used = true;
			if (val == "mi")
			{
				globs.debug_interface = global_config::debug_interface_mi;
				globs.is_debugger = true;
			}
			else if (val == "console")
			{
				globs.debug_interface = global_config::debug_interface_console;
				globs.is_debugger = true;
			}
			else if (val[0] == '+') /* +n turns on level n. */
				globs.debug[std::stoi(val.substr(1))] = true;
			else
			{
				/* n turns on levels 1-n. And turns everything else off*/
				int d = std::stoi(val);
				globs.debug[0] = false;
				for (int i = 1; i < DEBUG_MAX; ++i) globs.debug[i] = (i <= d);
			}
		},
		"x")
			   .name("-d")
			   .help("Set the debug level to x (0-13,console,mi).")
			   .cardinality(0, 0);

	cli |= lyra::opt(args_data.out_filename, "x")
			   .name("-o")
			   .help("Mirror all output to file x.");

	cli |= lyra::opt(args_data.targets_to_touch, "x")
			   .name("-t")
			   .help("Rebuild x, even if it is up-to-date.")
			   .cardinality(0, 0);

	cli |= lyra::opt(
		[&](const std::string & v) { args_data.variables_to_set.push_back(v); },
		"x=y")
			   .name("-s")
			   .help("Set variable x=y, overriding environment.")
			   .cardinality(0, 0);

	cli |= lyra::opt(args_data.files_to_parse, "x")
			   .name("-f")
			   .help("Read x instead of bootstrap.")
			   .cardinality(0, 0);

	cli |= lyra::opt(globs.debug_configuration)
			   .name("--debug-configuration")
			   .help(
				   "Display additional debugging information related to "
				   "locating and loading Boost Build configuration files.");

	cli |= lyra::opt(args_data.debug_handles, "h")
			   .name(debugger_opt)
			   .help("Internal debugger.")
			   .cardinality(0, 2);

	/* Set default parallel jobs to match cpu threads. This can be overridden
	the usual way with -jX or PARALLELISM env var. */
	globs.jobs = sys_info.cpu_thread_count();

	// Set up options.
	b2::args::set_args(arg_c, arg_v);
	b2::args::process_args(true);

	saved_argv0 = argv[0];
	last_update_now_status = 0;

#ifdef JAM_DEBUGGER

#if NT

	/* Check whether this instance is being run by the debugger. */
	if (args_data.debug_handles.size() >= 2)
	{
		debug_init_handles(args_data.debug_handles[0].c_str(),
			args_data.debug_handles[1].c_str());
		/* Fix up argc/argv to hide the internal options */
		arg_c = argc = (argc - 2);
		argv[2] = argv[0];
		arg_v = argv = (argv + 2);
		globs.debug_interface = global_config::debug_interface_child;
		args_data = {};
		b2::args::set_args(arg_c, arg_v);
		b2::args::process_args(true);
	}

	if (globs.is_debugger)
	{
		return debugger();
	}

#else

	if (globs.is_debugger)
	{
		if (setjmp(debug_child_data.jmp) != 0)
		{
			arg_c = argc = debug_child_data.argc;
			arg_v = argv = (char **)debug_child_data.argv;
			globs.debug_interface = global_config::debug_interface_child;
			args_data = {};
			b2::args::set_args(arg_c, arg_v);
			b2::args::process_args(true);
		}
		else
		{
			return debugger();
		}
	}

#endif

#endif

	if (!args_data.out_filename.empty())
	{
		/* If an output file is specified, set globs.out to that. */
		globs.out = std::fopen(args_data.out_filename.c_str(), "w");
		if (!globs.out)
		{
			err_printf("[errno %d] failed to write output file '%s': %s", errno,
				args_data.out_filename.c_str(), std::strerror(errno));
			b2::clean_exit(EXITBAD);
		}
	}

	{
		PROFILE_ENTER(MAIN);

#ifndef NDEBUG
		run_unit_tests();
#endif
#if YYDEBUG != 0
		if (is_debug_parse()) yydebug = 1;
#endif

		/* Set JAMDATE. */
		{
			timestamp current;
			timestamp_current(&current);
			b2::jam::variable("JAMDATE", timestamp_str(&current));
		}

		/* Set JAM_VERSION. */
		{
			b2::jam::variable jam_version("JAM_VERSION");
			jam_version = VERSION_MAJOR_SYM;
			jam_version += VERSION_MINOR_SYM;
			jam_version += VERSION_PATCH_SYM;
		}

		/* Set JAMUNAME. */
#ifdef unix
		{
			struct utsname u;

			if (uname(&u) >= 0)
			{
				b2::jam::variable jamuname("JAMUNAME");
				jamuname = u.sysname;
				jamuname += u.nodename;
				jamuname += u.release;
				jamuname += u.version;
				jamuname += u.machine;
			}
		}
#endif /* unix */

		/* Set JAM_TIMESTAMP_RESOLUTION. */
		{
			timestamp fmt_resolution[1];
			file_supported_fmt_resolution(fmt_resolution);
			b2::jam::variable(
				"JAM_TIMESTAMP_RESOLUTION", timestamp_timestr(fmt_resolution));
		}

		/* Load up environment variables. */

		/* First into the global module, with splitting, for backward
		 * compatibility.
		 */
		var_defines(root_module(), use_environ, 1);

		environ_module = bindmodule(constant_ENVIRON);
		/* Then into .ENVIRON, without splitting. */
		var_defines(environ_module, use_environ, 0);

		/*
		 * Jam defined variables OS & OSPLAT. We load them after environment, so
		 * that setting OS in environment does not change Jam's notion of the
		 * current platform.
		 */
		{
			const char * othersyms[] = { OSMAJOR, OSMINOR, OSPLAT, JAMVERSYM, 0 };
			var_defines(root_module(), othersyms, 1);
		}

		/* Load up variables set on command line. */
		for (const std::string & s : args_data.variables_to_set)
		{
			const char * symv[2];
			symv[0] = s.c_str();
			symv[1] = nullptr;
			var_defines(root_module(), symv, 1);
			var_defines(environ_module, symv, 0);
		}

		/* Set the ARGV to reflect the complete list of arguments of invocation.
		 */
		{
			b2::jam::variable argv_v("ARGV");
			for (int n = 0; n < arg_c; ++n) argv_v += arg_v[n];
		}

		/* Initialize built-in rules. */
		load_builtins();
		b2::startup::load_builtins();

		/* The build system may set the PARALLELISM variable to override -j
		 * options.
		 */
		{
			b2::jam::variable parallelism("PARALLELISM");
			if (parallelism)
			{
				int const j = std::atoi(parallelism[1]->str());
				if (j < 1)
					out_printf("Invalid value of PARALLELISM: %s.\n",
						parallelism[1]->str());
				else
					globs.jobs = j;
			}
		}

		/* KEEP_GOING overrides -q option. */
		{
			b2::jam::variable keep_going("KEEP_GOING");
			if (keep_going)
				globs.quitquick = std::atoi(keep_going[1]->str()) != 0;
		}

		/* Add the targets in the command line to the update list. */
		if (args_data.extra_args.empty())
			mark_target_for_updating(constant_all);
		else
			for (const std::string & val : args_data.extra_args)
			{
				b2::value_ref target(val);
				mark_target_for_updating(target);
			}

		/* Load build system. */
		if (args_data.files_to_parse.empty())
		{
			FRAME frame;
			frame_init(&frame);
			/* Initialize the native API bindings. */
			b2::jam::bind_jam(&frame);

			/* Launch the bootstrap to load up the build system. */
			status = b2::startup::bootstrap(&frame) ? 0 : 13;
		}

		/* Parse ruleset. */
		if (!args_data.files_to_parse.empty())
		{
			FRAME frame;
			frame_init(&frame);
			for (const std::string & s : args_data.files_to_parse)
			{
				b2::value_ref filename(s);
				parse_file(filename, &frame);
			}
		}

		// Process options.
		args_data = {};
		b2::args::set_args(arg_c, arg_v);
		b2::args::process_args();

		/* FIXME: What shall we do if builtin_update_now,
		 * the sole place setting last_update_now_status,
		 * failed earlier?
		 */

		if (status == 0) status = yyanyerrors();
		if (status == 0)
		{
			/* Manually touch -t targets. */
			for (auto const & x : args_data.targets_to_touch)
			{
				b2::value_ref target(x);
				touch_target(target);
			}

			/* Now make target. */
			{
				PROFILE_ENTER(MAIN_MAKE);
				b2::list_cref targets(targets_to_update());
				if (!targets.empty())
					status |= make(*targets, globs.anyhow);
				else
					status = last_update_now_status;
				PROFILE_EXIT(MAIN_MAKE);
			}
		}

		PROFILE_EXIT(MAIN);
	}
	b2::trigger_event_exit_main(status ? EXITBAD : EXITOK);

	return status ? EXITBAD : EXITOK;
}

#ifdef WIN32
namespace {

struct SetConsoleCodepage
{
	SetConsoleCodepage()
	{
		// Check whether UTF-8 is actually the default encoding for this process
		if (GetACP() != CP_UTF8) return;

		orig_console_cp = GetConsoleCP();
		if (orig_console_cp != 0 && orig_console_cp != CP_UTF8)
			SetConsoleCP(CP_UTF8);
		orig_console_output_cp = GetConsoleOutputCP();
		if (orig_console_output_cp != 0 && orig_console_output_cp != CP_UTF8)
			SetConsoleOutputCP(CP_UTF8);
	}

	~SetConsoleCodepage()
	{
		// Restore original console codepage
		if (orig_console_cp != 0 && orig_console_cp != CP_UTF8)
			SetConsoleCP(orig_console_cp);
		if (orig_console_output_cp != 0 && orig_console_output_cp != CP_UTF8)
			SetConsoleOutputCP(orig_console_output_cp);
	}

	private:
	UINT orig_console_cp = 0;
	UINT orig_console_output_cp = 0;
};

static const SetConsoleCodepage g_console_codepage_setter {};

} // namespace
#endif

int main(int argc, char ** argv)
{
	BJAM_MEM_INIT();

	cwd_init();
	constants_init();

	int result = EXIT_SUCCESS;
	try
	{
		result = guarded_main(argc, argv);
	}
	catch (b2::exit_result exit_code)
	{
		result = (int)exit_code;
		out_flush();
		err_flush();
	}

	if (is_debug_profile()) profile_dump();

#ifdef OPT_HEADER_CACHE_EXT
	hcache_done();
#endif

	clear_targets_to_update();

	/* Widely scattered cleanup. */
	parse_done();
	debugger_done();
	property_set_done();
	exec_done();
	file_done();
	rules_done();
	timestamp_done();
	search_done();
	class_done();
	modules_done();
	cwd_done();
	path_done();
	function_done();
	list_done();
	constants_done();
	object_done();

	/* Close log out. */
	if (globs.out) std::fclose(globs.out);

	BJAM_MEM_CLOSE();

	return result;
}
