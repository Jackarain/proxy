// Copyright 2025 Matt Borland
// Distributed under the Boost Software License, Version 1.0.
// https://www.boost.org/LICENSE_1_0.txt

#ifndef BOOST_DECIMAL_EXAMPLE_WHERE_FILE_HPP
#define BOOST_DECIMAL_EXAMPLE_WHERE_FILE_HPP

#include <string>
#include <vector>
#include <fstream>
#include <sstream>

namespace boost {
namespace decimal {

inline auto where_file(const std::string& test_vectors_filename) -> std::string
{
    // Try to open the file in each of the known relative paths
    // in order to find out where it is located.

    // Boost-root
    std::string test_vectors_filename_relative = "libs/decimal/examples/" + test_vectors_filename;

    std::ifstream in_01(test_vectors_filename_relative.c_str());

    const bool file_01_is_open { in_01.is_open() };


    if(file_01_is_open)
    {
        in_01.close();
    }
    else
    {
        // Local test directory or IDE
        test_vectors_filename_relative = "../examples/" + test_vectors_filename;

        std::ifstream in_02(test_vectors_filename_relative.c_str());

        const bool file_02_is_open { in_02.is_open() };

        if(file_02_is_open)
        {
            in_02.close();
        }
        else
        {
            // test/cover
            test_vectors_filename_relative = "../../examples/" + test_vectors_filename;

            std::ifstream in_03(test_vectors_filename_relative.c_str());

            const bool file_03_is_open { in_03.is_open() };

            if(file_03_is_open)
            {
                in_03.close();
            }
            else
            {
                // CMake builds
                test_vectors_filename_relative = "../../../../libs/decimal/examples/" + test_vectors_filename;

                std::ifstream in_04(test_vectors_filename_relative.c_str());

                const bool file_04_is_open { in_04.is_open() };

                if(file_04_is_open)
                {
                    in_04.close();
                }
                else
                {
                    // Try to open the file from the absolute path.
                    test_vectors_filename_relative = test_vectors_filename;

                    std::ifstream in_05(test_vectors_filename_relative.c_str());

                    const bool file_05_is_open { in_05.is_open() };

                    if(file_05_is_open)
                    {
                        in_05.close();
                    }
                    else
                    {
                        // Clion Cmake builds
                        test_vectors_filename_relative = "../../../libs/decimal/examples/" + test_vectors_filename;

                        std::ifstream in_06(test_vectors_filename_relative.c_str());

                        const bool file_06_is_open { in_06.is_open() };
                        if (file_06_is_open)
                        {
                            in_06.close();
                        }
                        else
                        {
                            test_vectors_filename_relative = "";
                        }
                    }
                }
            }
        }
    }

    return test_vectors_filename_relative;
}

} // namespace decimal
} // namespace boost

#endif //BOOST_DECIMAL_EXAMPLE_WHERE_FILE_HPP
