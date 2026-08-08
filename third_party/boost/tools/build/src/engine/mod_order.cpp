/* Copyright 2004. Vladimir Prus
 * Copyright 2026 Paolo Pastori
 * Distributed under the Boost Software License, Version 1.0.
 * (See accompanying file LICENSE.txt or copy at
 * https://www.bfgroup.xyz/b2/LICENSE.txt)
 */

#include "frames.h"
#include "lists.h"
#include "native.h"
#include "object.h"
#include "variable.h"

#include <utility>
#include <vector>


// vertex type, NOTE: limit the max number of vertices in the graph
using node_typ = uint16_t;
using node_vec = std::vector<node_typ>;
using vec_graph = std::vector<node_vec>;

enum node_state { TO_VISIT, VISITING, VISITED };
using state_vec = std::vector<char>;


/* Use quite klugy approach: when we add order dependency from 'a' to 'b',
 * just append 'b' to of value of variable 'a'.  NOTE: This is still here
 * only for backward compatibility reasons since latest order.jam use a
 * normal class method rule instead of this.
 */
LIST * add_pair( FRAME * frame, int32_t flags )
{
    LIST * arg = lol_get( frame->args, 0 );
    LISTITER iter = list_begin( arg );
    LISTITER const end = list_end( arg );
    var_set( frame->module, list_item( iter ), list_copy_range( arg, list_next(
        iter ), end ), VAR_APPEND );
    return L0;
}


/* Given a list and a value, returns position of that value in the list, or -1
 * if not found.
 */
static int32_t list_index( LIST * list, OBJECT * value )
{
    int32_t result = 0;
    LISTITER iter = list_begin( list );
    LISTITER const end = list_end( list );
    for ( ; iter != end; iter = list_next( iter ), ++result )
        if ( object_equal( list_item( iter ), value ) )
            return result;
    return -1;
}

/* Routine for depth first traversal. Calls itself recursively on all adjacent
 * vertices which were not yet visited. After that, 'current_vertex' is added
 * to 'result'.
 */
static void do_df( FRAME * frame, const vec_graph & graph,
        int32_t current_vertex, state_vec & state, node_vec & result )
{
    state[ current_vertex ] = VISITING;
    for ( auto adjacent_vertex : graph[ current_vertex ] )
    {
        if ( state[ adjacent_vertex ] == TO_VISIT )
            do_df( frame, graph, adjacent_vertex, state, result );
        // TODO
        //if ( state[ adjacent_vertex ] == VISITING )
        //    emit warning "Cyclic order dependency on 'x' and 'y'."
    }
    state[ current_vertex ] = VISITED;
    result.push_back( static_cast<node_typ>( current_vertex ) );
}

static void topological_sort( FRAME * frame, const vec_graph & graph,
        int32_t size, node_vec & result )
{
    state_vec state(size, TO_VISIT);
    for ( int32_t i = size - 1; i >= 0; --i )
        if ( state[ i ] == TO_VISIT )
            do_df( frame, graph, i, state, result );
}


LIST * order( FRAME * frame, int32_t flags )
{
    b2::list_cref arg( lol_get( frame->args, 0 ) );
    int32_t length = arg.length();
    if (length == 0) return L0;

    // Build dependency graph
    vec_graph graph;
    graph.reserve( length );
    for ( auto & obj : arg )
    {
        b2::list_cref deps( var_get( frame->module, obj ) );
        node_vec depl;
        depl.reserve( deps.length() );
        for ( auto & dep : deps )
        {
            int32_t dst = list_index( *arg, dep );
            if ( dst != -1 )
                depl.push_back( static_cast<node_typ>( dst ) ) ;
        }
        graph.push_back( std::move( depl ) );
    }

    node_vec order;
    order.reserve( length );
    topological_sort( frame, graph, length, order );

    b2::list_ref result;
    for ( int32_t i = length - 1; i >= 0; --i )
        result.push_back( object_copy( arg[ order[ i ] ] ) );

    return result.release();
}


void init_order()
{
    {   // for backward compatibility, see #593
        char const * args[] = { "first", "second", 0 };
        declare_native_rule( "class@order", "add-pair", args, add_pair, 1 );
    }

    {
        char const * args[] = { "objects", "*", 0 };
        declare_native_rule( "class@order", "order", args, order, 1 );
    }
}
