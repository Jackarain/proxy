/*=============================================================================
    Boost.Wave: A Standard compliant C++ preprocessor library
    http://www.boost.org/

    Copyright (c) 2026 Rac75116. Distributed under the Boost
    Software License, Version 1.0. (See accompanying file
    LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
=============================================================================*/

// Tests predefined macros with custom hook passthrough

//O --object_like_macro_passthrough

//R #line 15 "t_3_005.cpp"
__LINE__                    //R __LINE__ 
__FILE__                    //R __FILE__ 
__INCLUDE_LEVEL__           //R __INCLUDE_LEVEL__ 
//R #line 50 "test.cpp"
#line 50 "test.cpp"
__LINE__                    //R __LINE__ 
__FILE__                    //R __FILE__ 
__INCLUDE_LEVEL__           //R __INCLUDE_LEVEL__ 
