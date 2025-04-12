// Copyright Antony Polukhin, 2015-2025
//
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE_1_0.txt
// or copy at http://www.boost.org/LICENSE_1_0.txt)

// For more information, see http://www.boost.org

#include <boost/dll/library_info.hpp>

#include <fstream>
#include <boost/filesystem.hpp>

#include <boost/core/lightweight_test.hpp>

int main(int argc, char* argv[]) {
    BOOST_TEST(argc >= 1);
    const auto self_binary = boost::filesystem::path(argv[0]);
    const auto corrupted_binary = self_binary.parent_path() / "corrupted";

    boost::filesystem::copy_file(self_binary, corrupted_binary, boost::filesystem::copy_options::overwrite_existing);
    {
        std::ofstream ofs{corrupted_binary.string(), std::ios::binary};
        ofs.seekp(0);
        ofs << "bad";
    }
    try {
        boost::dll::library_info lib_info(corrupted_binary.string());
        BOOST_TEST(false);
    } catch (const std::exception& ) {}

#if BOOST_OS_WINDOWS
    boost::filesystem::copy_file(self_binary, corrupted_binary, boost::filesystem::copy_options::overwrite_existing);
    {
        std::ofstream ofs{corrupted_binary.string(), std::ios::binary};
        ofs.seekp(
            sizeof(boost::dll::detail::DWORD_)
            + sizeof(boost::dll::detail::IMAGE_FILE_HEADER_)
            + offsetof(boost::dll::detail::IMAGE_OPTIONAL_HEADER_template<boost::dll::detail::ULONGLONG_>, NumberOfRvaAndSizes)
        );
        const char data[] = "\0\0\0\0\0\0\0\0\0\0\0\0";
        ofs.write(data, sizeof(data));
    }

    try {
        boost::dll::library_info lib_info(corrupted_binary.string());
        lib_info.sections();
        BOOST_TEST(false);
    } catch (const std::exception& ) {}
#endif

#if !BOOST_OS_WINDOWS && !BOOST_OS_MACOS && !BOOST_OS_IOS
    // Elf
    boost::filesystem::copy_file(self_binary, corrupted_binary, boost::filesystem::copy_options::overwrite_existing);
    {
        std::ofstream ofs{corrupted_binary.string(), std::ios::binary};
        ofs.seekp(40);  // header->e_shoff
        ofs << "\xff\xff\xff\xff";
    }

    try {
        boost::dll::library_info lib_info(corrupted_binary.string());
        lib_info.sections();
        BOOST_TEST(false);
    } catch (const std::exception& ) {}
#endif

    return boost::report_errors();
}
