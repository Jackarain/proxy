/*
 * Copyright 1993, 2000 Christopher Seiwald.
 *
 * This file is part of Jam - see jam.c for Copyright information.
 */
/*  This file is ALSO:
 *  Copyright 2001-2004 David Abrahams.
 *  Distributed under the Boost Software License, Version 1.0.
 *  (See accompanying file LICENSE.txt or https://www.bfgroup.xyz/b2/LICENSE.txt)
 */

/*
 * headers.c - handle #includes in source files
 *
 * Using regular expressions provided as the variable $(HDRSCAN), headers()
 * searches a file for #include files and phonies up a rule invocation:
 *    $(HDRRULE) <target> : <include files> ;
 *
 * External routines:
 *    headers() - scan a target for include files and call HDRRULE
 *
 * Internal routines:
 *    headers1() - using regexp, scan a file and build include LIST
 */

#include "jam.h"
#include "headers.h"

#include "constants.h"
#include "compile.h"
#include "frames.h"
#include "lists.h"
#include "modules.h"
#include "object.h"
#include "parse.h"
#include "regexp.h"
#include "rules.h"
#include "variable.h"
#include "output.h"

#ifdef OPT_HEADER_CACHE_EXT
# include "hcache.h"
#endif

#include <errno.h>
#include <stdio.h>

#include <string>


// max numebr of regular expression sourced from HDRSCAN
#define MAX_SCAN_REGEXPS 10


/*
 * headers() - scan a target for include files and call HDRRULE
 */

void headers( TARGET * t )
{
    module_t * rootm = root_module();

    b2::list_cref hdrscan( var_get( rootm, constant_HDRSCAN ) );
    if ( hdrscan.empty() ) return;

    b2::list_cref hdrrule( var_get( rootm, constant_HDRRULE ) );
    if ( hdrrule.empty() ) return;

    if ( is_debug_header() )
        out_printf( "header scan %s\n", object_str( t->name ) );

    b2::list_cref fln_arg( var_get( rootm, constant_FILENAME ) );
    b2::value_ptr file_ptr = fln_arg.empty() ? nullptr : list_front( *fln_arg );
    b2::list_cref mod_arg( var_get( rootm, constant_MODULE ) );
    b2::value_ptr user_mod = mod_arg.empty() ? nullptr : list_front( *mod_arg );

    // this is used for error reporting purposes only
    FRAME frame_re[ 1 ];
    frame_init( frame_re );
    std::string varname = std::string("HDRSCAN on ") + object_str( t->name );
    frame_re->rulename = varname.c_str();
    lol_add( frame_re->args, list_copy( *hdrscan ) );
    // try to retrieve at least the user jamfile filename
    if (file_ptr)
    {
        frame_re->file = file_ptr;
        frame_re->line = 0; // only filename is known
    }

    // compile all regular expressions in HDRSCAN
    b2::regex::program re_prog[MAX_SCAN_REGEXPS];
    int rec = 0;
    {
        // compilation errors print a nice error message and exit
        b2::regex::frame_ctx ctx(frame_re);

        b2::list_cref::iterator iter = hdrscan.begin();
        b2::list_cref::iterator end = hdrscan.end();
        for ( ; ( rec < MAX_SCAN_REGEXPS ) && iter != end; iter++ )
        {
            re_prog[ rec ].reset( (*iter)->str() );
            rec += 1;
        }
    }

    FRAME frame[ 1 ];
    frame_init( frame );
    frame->prev = frame_re;
    varname = std::string("HDRRULE on ") + object_str( t->name );
    frame_re->rulename = varname.c_str();
    if (user_mod)
    {
        // do not thrust user_mod, search for module with find_module
        frame->module = find_module(user_mod);
        if (! frame->module) frame->module = rootm;
    }

    // Get LIST of included files
    lol_add( frame->args, list_new( object_copy( t->name ) ) );
#ifdef OPT_HEADER_CACHE_EXT
    lol_add( frame->args, hcache( t, rec, re_prog, *hdrscan ) );
#else
    LIST * headlist = L0;
    lol_add( frame->args, headers1( headlist, t->boundname, rec, re_prog ) );
#endif

    // Call HDRRULE rule
    if ( lol_get( frame->args, 1 ) )
    {
        b2::value_ptr rulename = list_front( *hdrrule );
        // The third argument to HDRRULE is the bound name of $(<).
        lol_add( frame->args, list_new( object_copy( t->boundname ) ) );
        list_free( evaluate_rule( bindrule( rulename, frame->module ), rulename, frame ) );
    }

    // Clean up.
    frame_free( frame );
    frame_free( frame_re );
}
#undef MAX_SCAN_REGEXPS


/*
 * headers1() - using regexp, scan a file and build include LIST.
 */

LIST * headers1( LIST * l, OBJECT * file, int rec, b2::regex::program re[] )
{
    FILE * f;
    char buf[ 1024 ];
    int i;

#ifdef OPT_IMPROVED_PATIENCE_EXT
    static int count = 0;
    ++count;
    if ( ( ( count == 100 ) || !( count % 1000 ) ) && is_debug_make() )
    {
        out_printf( "...patience...\n" );
        out_flush();
    }
#endif

    if ( file->as_string().size() == 0 )
    {
        /* If the scanning was fed empty file names we just ignore them. */
        return l;
    }

    if ( !( f = fopen( object_str( file ), "r" ) ) )
    {
        /* No source files will be generated when -n flag is passed */
        if ( !globs.noexec || errno != ENOENT )
            err_printf( "[errno %d] failed to scan file '%s': %s",
                errno, object_str( file ), strerror(errno) );
        return l;
    }

    while ( fgets( buf, sizeof( buf ), f ) )
    {
        for ( i = 0; i < rec; ++i )
        {
            auto re_i = re [ i ].search( buf );
            if ( re_i && re_i[ 1 ].begin() )
            {
                std::string header(re_i[ 1 ].begin(), re_i[ 1 ].end());
                if ( is_debug_header() )
                    out_printf( "header found: %s\n", header.c_str() );
                l = list_push_back( l, object_new( header.c_str() ) );
            }
        }
    }

    fclose( f );
    return l;
}
