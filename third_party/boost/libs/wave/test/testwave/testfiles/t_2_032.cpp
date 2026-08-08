/*=============================================================================
    Boost.Wave: A Standard compliant C++ preprocessor library
    http://www.boost.org/

    Copyright (c) 2026 sortA0329. Distributed under the Boost
    Software License, Version 1.0. (See accompanying file
    LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
=============================================================================*/

// Verify include lookup ignores directories named like the header.

//O -I$P(t_2_032_dir)
//O -I$P(t_2_032_inc)

//R #line 10 "t_2_032_target.hpp"
//R t_2_032_target_from_file
#include "t_2_032_target.hpp"
