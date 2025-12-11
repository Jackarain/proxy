// Copyright 2025 Christian Granzin
// Copyright 2008 Christophe Henry
// henry UNDERSCORE christophe AT hotmail DOT com
// This is an extended version of the state machine available in the boost::mpl library
// Distributed under the same license as the original.
// Copyright for the original version:
// Copyright 2005 David Abrahams and Aleksey Gurtovoy. Distributed
// under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)

#ifndef BOOST_MSM_BACKMP11_DISPATCH_TABLE_H
#define BOOST_MSM_BACKMP11_DISPATCH_TABLE_H

#include <cstdint>
#include <cstddef>

#include <boost/mp11.hpp>
#include <boost/msm/backmp11/common_types.hpp>

namespace boost { namespace msm { namespace backmp11
{

namespace detail
{

// Value used to initialize a cell of the dispatch table
template<typename Cell>
struct init_cell_value
{
    size_t index;
    Cell address;
};
template<size_t v1, typename Cell, Cell v2>
struct init_cell_constant
{
    static constexpr init_cell_value<Cell> value = {v1, v2};
};

// Type-punned init cell value to suppress redundant template instantiations
typedef std::uintptr_t generic_cell; 
struct generic_init_cell_value
{
    size_t index;
    generic_cell address;
};

// Helper to create an array of init cell values for table initialization
template<typename Cell, typename InitCellConstants, std::size_t... I>
static const init_cell_value<Cell>* get_init_cells_impl(mp11::index_sequence<I...>)
{
    static constexpr init_cell_value<Cell> values[] {mp11::mp_at_c<InitCellConstants, I>::value...};
    return values;
}
template<typename Cell, typename InitCellConstants>
static const init_cell_value<Cell>* get_init_cells_impl(mp11::index_sequence<>)
{
    return nullptr;
}
template<typename Cell, typename InitCellConstants>
static const generic_init_cell_value* get_init_cells()
{
    return reinterpret_cast<const generic_init_cell_value*>(
        get_init_cells_impl<Cell, InitCellConstants>(mp11::make_index_sequence<mp11::mp_size<InitCellConstants>::value>{}));
}

} // detail

}}} // boost::msm::backmp11

#endif //BOOST_MSM_BACKMP11_DISPATCH_TABLE_H
