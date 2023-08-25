cmake_minimum_required(VERSION 3.16)
project(Boost)

include(CMakeDetermineASMCompiler)
include(CheckFunctionExists)

if (WIN32)
	enable_language(ASM_MASM)
else()
	enable_language(ASM)
endif()

find_package(Threads)

check_function_exists(swapcontext HAVE_UCONTEXT)

set(BOOST_ATOMIC_SRC libs/atomic/src/lock_pool.cpp)
if (WIN32)
	set(BOOST_ATOMIC_SRC ${BOOST_ATOMIC_SRC} libs/atomic/src/wait_on_address.cpp)
endif()

set(BOOST_SERIALIZATION_SRC
	libs/serialization/src/archive_exception.cpp
	libs/serialization/src/basic_archive.cpp
	libs/serialization/src/basic_iarchive.cpp
	libs/serialization/src/basic_iserializer.cpp
	libs/serialization/src/basic_oarchive.cpp
	libs/serialization/src/basic_oserializer.cpp
	libs/serialization/src/basic_pointer_iserializer.cpp
	libs/serialization/src/basic_pointer_oserializer.cpp
	libs/serialization/src/basic_serializer_map.cpp
	libs/serialization/src/basic_text_iprimitive.cpp
	libs/serialization/src/basic_text_oprimitive.cpp
	libs/serialization/src/basic_xml_archive.cpp
	libs/serialization/src/binary_iarchive.cpp
	libs/serialization/src/binary_oarchive.cpp
	libs/serialization/src/extended_type_info_no_rtti.cpp
	libs/serialization/src/extended_type_info_typeid.cpp
	libs/serialization/src/extended_type_info.cpp
	libs/serialization/src/polymorphic_iarchive.cpp
	libs/serialization/src/polymorphic_oarchive.cpp
	libs/serialization/src/polymorphic_text_iarchive.cpp
	libs/serialization/src/polymorphic_text_oarchive.cpp
	libs/serialization/src/stl_port.cpp
	libs/serialization/src/text_iarchive.cpp
	libs/serialization/src/text_oarchive.cpp
	libs/serialization/src/void_cast.cpp
	libs/serialization/src/xml_archive_exception.cpp
	libs/serialization/src/xml_iarchive.cpp
	libs/serialization/src/xml_oarchive.cpp
	libs/serialization/src/xml_grammar.cpp
	libs/serialization/src/utf8_codecvt_facet.cpp
	libs/serialization/src/basic_xml_grammar.ipp # doesn't show up in "Source Files" in Xcode"'
)

set(BOOST_REGEX_SRC
	libs/regex/src/regex.cpp
	libs/regex/src/wide_posix_api.cpp
	libs/regex/src/regex_debug.cpp
	libs/regex/src/posix_api.cpp
	libs/regex/src/static_mutex.cpp
	libs/regex/src/internals.hpp
)

if (WIN32)
	set(BOOST_THREAD_SRC
		libs/thread/src/future.cpp
		libs/thread/src/win32/tss_dll.cpp
		libs/thread/src/win32/thread.cpp
		libs/thread/src/win32/tss_pe.cpp
	)
else()
	set(BOOST_THREAD_SRC
		libs/thread/src/future.cpp
		libs/thread/src/pthread/once.cpp
		libs/thread/src/pthread/thread.cpp
		libs/thread/src/pthread/once_atomic.cpp
	)
endif()

set(BOOST_CHRONO_SRC
	libs/chrono/src/chrono.cpp
	libs/chrono/src/thread_clock.cpp
	libs/chrono/src/process_cpu_clocks.cpp
)

set(BOOST_CONTEXT_SRC
	libs/context/src/dummy.cpp
#	libs/context/src/fiber.cpp
)

if (WIN32)
	list(APPEND BOOST_CONTEXT_SRC libs/context/src/windows/stack_traits.cpp)
	set(ASSEMBLER masm)
	set(ASM_FILE_EXT "asm")
	set(EXEUCTABLEFORMAT "pe")
	set(SYSABI "ms")

	if (CMAKE_SIZEOF_VOID_P EQUAL 8)
		set(ARCH "x86_64")
	else()
		set(ARCH "i386")
	endif()
else()
	set(ASSEMBLER gas)
	set(ASM_FILE_EXT "S")

	if (APPLE)
		set(EXEUCTABLEFORMAT "macho")
	else()
		set(EXEUCTABLEFORMAT "elf")
	endif()

	list(APPEND BOOST_CONTEXT_SRC libs/context/src/posix/stack_traits.cpp)

	if ("${CMAKE_SYSTEM_PROCESSOR}" MATCHES "x86")
		set(SYSABI "sysv")
		if(CMAKE_SIZEOF_VOID_P EQUAL 8)
			set(ARCH "x86_64")
		else()
			set(ARCH "i386")
		endif()
	elseif ("${CMAKE_SYSTEM_PROCESSOR}" MATCHES "i686")
		set(SYSABI "sysv")
		set(ARCH "i386")
	elseif ("${CMAKE_SYSTEM_PROCESSOR}" MATCHES "^arm")
		set(SYSABI "aapcs")
		if (CMAKE_SIZEOF_VOID_P EQUAL 8)
			set(ARCH "arm64")
		else()
			set(ARCH "arm")
		endif()
	elseif ("${CMAKE_SYSTEM_PROCESSOR}" MATCHES "aarch64")
		set(SYSABI "aapcs")
		set(ARCH "arm64")
	elseif ("${CMAKE_SYSTEM_PROCESSOR}" MATCHES "mips32")
		set(ARCH "mips32")
		set(SYSABI "o32")
	elseif ("${CMAKE_SYSTEM_PROCESSOR}" MATCHES "mips64")
		set(ARCH "mips64")
		set(SYSABI "n64")
	endif()
endif()

list(APPEND BOOST_CONTEXT_SRC libs/context/src/asm/jump_${ARCH}_${SYSABI}_${EXEUCTABLEFORMAT}_${ASSEMBLER}.${ASM_FILE_EXT})
list(APPEND BOOST_CONTEXT_SRC libs/context/src/asm/make_${ARCH}_${SYSABI}_${EXEUCTABLEFORMAT}_${ASSEMBLER}.${ASM_FILE_EXT})
list(APPEND BOOST_CONTEXT_SRC libs/context/src/asm/ontop_${ARCH}_${SYSABI}_${EXEUCTABLEFORMAT}_${ASSEMBLER}.${ASM_FILE_EXT})

if (HAVE_UCONTEXT)
	list(APPEND BOOST_CONTEXT_SRC libs/context/src/continuation.cpp)
endif()

set(BOOST_COROUTINE_SRC
	libs/coroutine/src/detail/coroutine_context.cpp
	libs/coroutine/src/exceptions.cpp
)

if (WIN32)
	list(APPEND BOOST_COROUTINE_SRC libs/coroutine/src/windows/stack_traits.cpp)
else()
	list(APPEND BOOST_COROUTINE_SRC libs/coroutine/src/posix/stack_traits.cpp)
endif()

set(BOOST_DATE_TIME_SRC

	libs/date_time/src/posix_time/posix_time_types.cpp
	libs/date_time/src/gregorian/gregorian_types.cpp
	libs/date_time/src/gregorian/greg_month.cpp
	libs/date_time/src/gregorian/greg_names.hpp
)

set(BOOST_EXCEPTION_SRC libs/exception/src/clone_current_exception_non_intrusive.cpp)

if (USE_BOOST_FILESYSTEM)
	set(BOOST_FILESYSTEM_SRC
		libs/filesystem/src/atomic_ref.hpp
		libs/filesystem/src/atomic_tools.hpp
		libs/filesystem/src/codecvt_error_category.cpp
		libs/filesystem/src/directory.cpp
		libs/filesystem/src/error_handling.hpp
		libs/filesystem/src/exception.cpp
		libs/filesystem/src/operations.cpp
		libs/filesystem/src/path.cpp
		libs/filesystem/src/path_traits.cpp
		libs/filesystem/src/platform_config.hpp
		libs/filesystem/src/portability.cpp
		libs/filesystem/src/posix_tools.hpp
		libs/filesystem/src/private_config.hpp
		libs/filesystem/src/unique_path.cpp
		libs/filesystem/src/utf8_codecvt_facet.cpp
		libs/filesystem/src/windows_file_codecvt.cpp
		libs/filesystem/src/windows_file_codecvt.hpp
		libs/filesystem/src/windows_tools.hpp
	)
endif()

set(BOOST_LOCALE_SRC
	libs/locale/src/encoding/codepage.cpp
	libs/locale/src/shared/date_time.cpp
	libs/locale/src/shared/generator.cpp
	libs/locale/src/shared/message.cpp
	libs/locale/src/shared/format.cpp
	libs/locale/src/shared/ids.cpp
	libs/locale/src/shared/mo_lambda.cpp
	libs/locale/src/shared/formatting.cpp
	libs/locale/src/shared/localization_backend.cpp
	libs/locale/src/std/collate.cpp
	libs/locale/src/std/numeric.cpp
	libs/locale/src/std/all_generator.hpp
	libs/locale/src/std/codecvt.cpp
	libs/locale/src/std/std_backend.cpp
	libs/locale/src/std/std_backend.hpp
	libs/locale/src/std/converter.cpp
	libs/locale/src/util/locale_data.cpp
	libs/locale/src/util/default_locale.cpp
	libs/locale/src/util/locale_data.hpp
	libs/locale/src/util/numeric.hpp
	libs/locale/src/util/info.cpp
	libs/locale/src/util/codecvt_converter.cpp
	libs/locale/src/util/timezone.hpp
	libs/locale/src/util/gregorian.hpp
	libs/locale/src/util/gregorian.cpp
	libs/locale/src/util/iconv.hpp
)

if (WIN32)
	list(APPEND BOOST_LOCALE_SRC
		libs/locale/src/win32/collate.cpp
		libs/locale/src/win32/lcid.cpp
		libs/locale/src/win32/win_backend.cpp
		libs/locale/src/win32/numeric.cpp
		libs/locale/src/win32/win_backend.hpp
		libs/locale/src/win32/api.hpp
		libs/locale/src/win32/all_generator.hpp
		libs/locale/src/win32/converter.cpp
		libs/locale/src/win32/lcid.hpp
	)
else()
	list(APPEND BOOST_LOCALE_SRC
		libs/locale/src/posix/collate.cpp
		libs/locale/src/posix/numeric.cpp
		libs/locale/src/posix/posix_backend.cpp
		libs/locale/src/posix/posix_backend.hpp
		libs/locale/src/posix/all_generator.hpp
		libs/locale/src/posix/codecvt.cpp
		libs/locale/src/posix/converter.cpp
		libs/locale/src/posix/codecvt.hpp
	)
endif()

if (CMAKE_COMPILER_IS_GNUCXX)
	add_definitions(-DBOOST_LOCALE_WITH_ICONV)
endif()

set(BOOST_PROGRAM_OPTIONS_SRC
	libs/program_options/src/winmain.cpp
	libs/program_options/src/value_semantic.cpp
	libs/program_options/src/convert.cpp
	libs/program_options/src/variables_map.cpp
	libs/program_options/src/parsers.cpp
	libs/program_options/src/options_description.cpp
	libs/program_options/src/utf8_codecvt_facet.cpp
	libs/program_options/src/split.cpp
	libs/program_options/src/positional_options.cpp
	libs/program_options/src/cmdline.cpp
	libs/program_options/src/config_file.cpp
)

set(BOOST_JSON_SRC
	libs/json/src/src.cpp
)

file(GLOB_RECURSE BOOST_URL_SRC CONFIGURE_DEPENDS libs/url/src/*.cpp)

set(BOOST_TIMER_SRC
	libs/timer/src/auto_timers_construction.cpp
	libs/timer/src/cpu_timer.cpp
)

set(BOOST_RANDOM_SRC libs/random/src/random_device.cpp)

set(BOOST_FIBER_SRC
	libs/fiber/src/scheduler.cpp
	libs/fiber/src/fiber.cpp
	libs/fiber/src/properties.cpp
	libs/fiber/src/future.cpp
	libs/fiber/src/recursive_mutex.cpp
	libs/fiber/src/context.cpp
	libs/fiber/src/timed_mutex.cpp
	libs/fiber/src/condition_variable.cpp
	libs/fiber/src/algo/shared_work.cpp
	libs/fiber/src/algo/algorithm.cpp
	libs/fiber/src/algo/work_stealing.cpp
	libs/fiber/src/algo/round_robin.cpp
	libs/fiber/src/recursive_timed_mutex.cpp
	libs/fiber/src/mutex.cpp
	libs/fiber/src/barrier.cpp
	libs/fiber/src/numa/algo/work_stealing.cpp
	libs/fiber/src/numa/topology.cpp
)

if (WIN32)
	list(APPEND BOOST_FIBER_SRC
		libs/fiber/src/numa/windows/topology.cpp
		libs/fiber/src/numa/windows/pin_thread.cpp
	)
elseif (FREEBSD)
	list(APPEND BOOST_FIBER_SRC
		libs/fiber/src/numa/freebsd/topology.cpp
		libs/fiber/src/numa/freebsd/pin_thread.cpp
	)
elseif (LINUX)
	list(APPEND BOOST_FIBER_SRC
		libs/fiber/src/numa/linux/topology.cpp
		libs/fiber/src/numa/linux/pin_thread.cpp
	)
endif()

if (USE_BOOST_TEST)
	set(BOOST_TEST_SRC
		libs/test/src/compiler_log_formatter.cpp
		libs/test/src/cpp_main.cpp
		libs/test/src/debug.cpp
		libs/test/src/decorator.cpp
		libs/test/src/execution_monitor.cpp
		libs/test/src/framework.cpp
		libs/test/src/junit_log_formatter.cpp
		libs/test/src/plain_report_formatter.cpp
		libs/test/src/progress_monitor.cpp
		libs/test/src/results_collector.cpp
		libs/test/src/results_reporter.cpp
		libs/test/src/test_framework_init_observer.cpp
		libs/test/src/test_main.cpp
		libs/test/src/test_tools.cpp
		libs/test/src/test_tree.cpp
		libs/test/src/unit_test_log.cpp
		libs/test/src/unit_test_main.cpp
		libs/test/src/unit_test_monitor.cpp
		libs/test/src/unit_test_parameters.cpp
		libs/test/src/xml_log_formatter.cpp
		libs/test/src/xml_report_formatter.cpp
	)
endif()

set(BOOST_SRCS ${BOOST_ATOMIC_SRC})

add_library(boost_header INTERFACE)
add_library(Boost::boost ALIAS boost_header)
add_library(Boost::system ALIAS boost_header)

if (Boost_USE_STATIC_LIBS)
	add_library(boost_atomic_static STATIC ${BOOST_ATOMIC_SRC})
	target_include_directories(boost_atomic_static PRIVATE libs/atomic/src)
	add_library(Boost::atomic ALIAS boost_atomic_static)
else()
	add_library(boost_atomic_dll SHARED ${BOOST_ATOMIC_SRC})
	target_include_directories(boost_atomic_dll PRIVATE libs/atomic/src)
	add_library(Boost::atomic ALIAS boost_atomic_dll)
endif()


if (USE_BOOST_SERIALIZATION)
	if (Boost_USE_STATIC_LIBS)
		add_library(boost_serialization_static STATIC ${BOOST_SERIALIZATION_SRC})
		add_library(Boost::serialization ALIAS boost_serialization_static)
	else()
		add_library(boost_serialization_dll SHARED ${BOOST_SERIALIZATION_SRC})
		add_library(Boost::serialization ALIAS boost_serialization_dll)
	endif()
endif()

if (Boost_USE_STATIC_LIBS)
	add_library(boost_thread_static STATIC ${BOOST_THREAD_SRC})
	add_library(Boost::thread ALIAS boost_thread_static)
else()
	add_library(boost_thread_dll SHARED ${BOOST_THREAD_SRC})
	add_library(Boost::thread ALIAS boost_thread_dll)
endif()

if (Boost_USE_STATIC_LIBS)
	add_library(boost_context_static STATIC ${BOOST_CONTEXT_SRC})
	add_library(Boost::context ALIAS boost_context_static)
else()
	add_library(boost_context_dll SHARED ${BOOST_CONTEXT_SRC})
	add_library(Boost::context ALIAS boost_context_dll)
endif()

if (Boost_USE_STATIC_LIBS)
	add_library(boost_coroutine_static STATIC ${BOOST_COROUTINE_SRC})
	add_library(Boost::coroutine ALIAS boost_coroutine_static)
else()
	add_library(boost_coroutine_dll SHARED ${BOOST_COROUTINE_SRC})
	add_library(Boost::coroutine ALIAS boost_coroutine_dll)
endif()

if (Boost_USE_STATIC_LIBS)
	add_library(boost_chrono_static STATIC ${BOOST_CHRONO_SRC})
	add_library(Boost::chrono ALIAS boost_chrono_static)
else()
	add_library(boost_chrono_dll SHARED ${BOOST_CHRONO_SRC})
	add_library(Boost::chrono ALIAS boost_chrono_dll)
endif()

if (USE_BOOST_FILESYSTEM)
	if (Boost_USE_STATIC_LIBS)
		add_library(boost_filesystem_static STATIC ${BOOST_FILESYSTEM_SRC})
		add_library(Boost::filesystem ALIAS boost_filesystem_static)
	else()
		add_library(boost_filesystem_dll SHARED ${BOOST_FILESYSTEM_SRC})
		add_library(Boost::filesystem ALIAS boost_filesystem_dll)
	endif()
endif()

if (USE_BOOST_LOCALE)
	if (Boost_USE_STATIC_LIBS)
		add_library(boost_locale_static STATIC ${BOOST_LOCALE_SRC})
		add_library(Boost::locale ALIAS boost_locale_static)
		if (WIN32)
			target_compile_definitions(boost_locale_static PRIVATE -DBOOST_LOCALE_WITH_WCONV)
		else()
			target_compile_definitions(boost_locale_static PRIVATE -DBOOST_LOCALE_WITH_ICONV)
		endif()
	else()
		add_library(boost_locale_dll SHARED ${BOOST_LOCALE_SRC})
		add_library(Boost::locale ALIAS boost_locale_dll)

		if (WIN32)
			target_compile_definitions(boost_locale_dll PRIVATE -DBOOST_LOCALE_WITH_WCONV)
		else()
			target_compile_definitions(boost_locale_dll PRIVATE -DBOOST_LOCALE_WITH_ICONV)
		endif()
	endif()
endif()

if (Boost_USE_STATIC_LIBS)
	add_library(boost_date_time_static STATIC ${BOOST_DATE_TIME_SRC})
	add_library(Boost::date_time ALIAS boost_date_time_static)
else()
	add_library(boost_date_time_dll SHARED ${BOOST_DATE_TIME_SRC})
	add_library(Boost::date_time ALIAS boost_date_time_dll)
endif()

if (Boost_USE_STATIC_LIBS)
	add_library(boost_regex_static STATIC ${BOOST_REGEX_SRC})
	add_library(Boost::regex ALIAS boost_regex_static)
else()
	add_library(boost_regex_dll SHARED ${BOOST_REGEX_SRC})
	add_library(Boost::regex ALIAS boost_regex_dll)
endif()

if (Boost_USE_STATIC_LIBS)
	add_library(boost_timer_static STATIC ${BOOST_TIMER_SRC})
	add_library(Boost::timer ALIAS boost_timer_static)
else()
	add_library(boost_timer_dll SHARED ${BOOST_TIMER_SRC})
	add_library(Boost::timer ALIAS boost_timer_dll)
endif()

if (Boost_USE_STATIC_LIBS)
	add_library(boost_exception_static STATIC ${BOOST_EXCEPTION_SRC})
	add_library(Boost::exception ALIAS boost_exception_static)
else()
	add_library(boost_exception_dll SHARED ${BOOST_EXCEPTION_SRC})
	add_library(Boost::exception ALIAS boost_exception_dll)
endif()

if (Boost_USE_STATIC_LIBS)
	add_library(boost_random_static STATIC ${BOOST_RANDOM_SRC})
	add_library(Boost::random ALIAS boost_random_static)
else()
	add_library(boost_random_dll SHARED ${BOOST_RANDOM_SRC})
	add_library(Boost::random ALIAS boost_random_dll)
endif()

if (Boost_USE_STATIC_LIBS)
	add_library(boost_program_options_static STATIC ${BOOST_PROGRAM_OPTIONS_SRC} )
	add_library(Boost::program_options ALIAS boost_program_options_static)
else()
	add_library(boost_program_options_dll SHARED ${BOOST_PROGRAM_OPTIONS_SRC})
	add_library(Boost::program_options ALIAS boost_program_options_dll)
endif()

if (Boost_USE_STATIC_LIBS)
	add_library(boost_json_static STATIC ${BOOST_JSON_SRC} )
	add_library(Boost::json ALIAS boost_json_static)
else()
	add_library(boost_json_dll SHARED ${BOOST_JSON_SRC})
	add_library(Boost::json ALIAS boost_json_dll)
endif()

if (Boost_USE_STATIC_LIBS)
	add_library(boost_url_static STATIC ${BOOST_URL_SRC} )
	add_library(Boost::url ALIAS boost_url_static)
else()
	add_library(boost_url_dll SHARED ${BOOST_URL_SRC})
	add_library(Boost::url ALIAS boost_url_dll)
endif()

if (USE_BOOST_FIBER)
	if (Boost_USE_STATIC_LIBS)
		add_library(boost_fiber_static STATIC ${BOOST_FIBER_SRC})
		add_library(Boost::fiber ALIAS boost_fiber_static)
	else()
		add_library(boost_fiber_dll SHARED ${BOOST_FIBER_SRC})
		add_library(Boost::fiber ALIAS boost_fiber_dll)
	endif()
endif()

if (USE_BOOST_TEST)
	if (Boost_USE_STATIC_LIBS)
		add_library(boost_test_static STATIC ${BOOST_TEST_SRC})
		add_library(Boost::test ALIAS boost_test_static)
	else()
		add_library(boost_test_dll SHARED ${BOOST_TEST_SRC})
		add_library(Boost::test ALIAS boost_test_dll)
	endif()
endif()

if (Boost_USE_STATIC_LIBS)
	add_library(boost_asio_static STATIC asio.cpp)
	add_library(Boost::asio ALIAS boost_asio_static)
	if (OpenSSL_FOUND)
		message(STATUS "**************************** OPENSSL FOUND = ${OpenSSL_FOUND} ****************************")
		target_compile_definitions(boost_asio_static PRIVATE -DHAVE_OPENSSL)
		target_include_directories(boost_asio_static PUBLIC ${OPENSSL_INCLUDE_DIR})
		target_link_libraries(boost_asio_static ${OPENSSL_LIBRARIES})
	endif()
else()
	add_library(boost_asio_dll SHARED asio.cpp)
	add_library(Boost::asio ALIAS boost_asio_dll)
	if (OpenSSL_FOUND)
		message(STATUS "**************************** OPENSSL FOUND = ${OpenSSL_FOUND} ****************************")
		target_compile_definitions(boost_asio_dll PRIVATE -DHAVE_OPENSSL)
		target_include_directories(boost_asio_dll PUBLIC ${OPENSSL_INCLUDE_DIR})
		target_link_libraries(boost_asio_dll ${OPENSSL_LIBRARIES})
	endif()
endif()

if (Boost_USE_STATIC_LIBS)
	add_library(boost_beast_static STATIC beast.cpp)
	add_library(Boost::beast ALIAS boost_beast_static)
else()
	add_library(boost_beast_dll SHARED beast.cpp)
	add_library(Boost::beast ALIAS boost_beast_dll)
endif()

target_include_directories(boost_header INTERFACE ${CMAKE_CURRENT_SOURCE_DIR})

target_compile_definitions(boost_header INTERFACE -DBOOST_ALL_NO_LIB -DBOOST_ALL_STATIC_LINK -DBOOST_THREAD_USE_LIB)
target_compile_definitions(boost_thread_static PRIVATE -DBOOST_THREAD_BUILD_LIB)
target_compile_definitions(boost_context_static PUBLIC -DBOOST_ALL_NO_LIB -DBOOST_ALL_STATIC_LINK)
target_compile_definitions(boost_context_static PRIVATE -DBOOST_CONTEXT_EXPORT)

if (USE_BOOST_FILESYSTEM)
	if (APPLE)
		target_compile_definitions(boost_filesystem_static PUBLIC -DBOOST_FILESYSTEM_STATIC_LINK -DBOOST_FILESYSTEM_NO_CXX20_ATOMIC_REF)
	else()
		target_compile_definitions(boost_filesystem_static PUBLIC -DBOOST_FILESYSTEM_STATIC_LINK -DBOOST_FILESYSTEM_NO_CXX20_ATOMIC_REF -DBOOST_FILESYSTEM_HAS_POSIX_AT_APIS)
	endif()
endif()

target_compile_definitions(boost_json_static PUBLIC -DBOOST_JSON_SOURCE -DBOOST_JSON_STATIC_LINK)
target_compile_definitions(boost_json_static PRIVATE -DBOOST_JSON_SOURCE -DBOOST_JSON_STATIC_LINK)

target_compile_definitions(boost_url_static PUBLIC -DBOOST_URL_SOURCE -DBOOST_URL_STATIC_LINK)
target_compile_definitions(boost_url_static PRIVATE -DBOOST_URL_SOURCE -DBOOST_URL_STATIC_LINK)

if (USE_BOOST_TEST)
	if (Boost_USE_STATIC_LIBS)
		target_compile_definitions(boost_test_static PUBLIC -DBOOST_TEST_STATIC_LINK)
	else()
		target_compile_definitions(boost_test_static PUBLIC -DBOOST_TEST_DYN_LINK)
	endif()
endif()

if (Boost_USE_STATIC_LIBS)
	if (USE_BOOST_TEST)
		target_link_libraries(boost_test_static boost_header)
	endif()
	target_link_libraries(boost_atomic_static boost_header)
	if (USE_BOOST_SERIALIZATION)
		target_link_libraries(boost_serialization_static boost_header)
	endif()
	if (USE_BOOST_FILESYSTEM)
		target_link_libraries(boost_filesystem_static boost_header)
	endif()
	target_link_libraries(boost_thread_static boost_header Threads::Threads)
	target_link_libraries(boost_context_static boost_header)
	if (USE_BOOST_FIBER)
		target_link_libraries(boost_fiber_static boost_context_static boost_header)
	endif()
	target_link_libraries(boost_json_static boost_header)
	target_link_libraries(boost_url_static boost_header)
	target_link_libraries(boost_coroutine_static boost_context_static boost_header)
	target_link_libraries(boost_chrono_static boost_header)
	if (USE_BOOST_LOCALE)
		target_link_libraries(boost_locale_static boost_header)
	endif()
	target_link_libraries(boost_date_time_static boost_chrono_static boost_header)
	target_link_libraries(boost_regex_static boost_header)
	target_link_libraries(boost_timer_static boost_chrono_static boost_header)
	target_link_libraries(boost_exception_static boost_header)
	target_link_libraries(boost_random_static boost_header)
	target_link_libraries(boost_program_options_static boost_header)
	target_link_libraries(boost_asio_static boost_thread_static)
	target_link_libraries(boost_beast_static boost_asio_static)
else()
	if (USE_BOOST_TEST)
		target_link_libraries(boost_test_dll boost_header)
	endif()
	target_link_libraries(boost_atomic_dll boost_header)
	if (USE_BOOST_SERIALIZATION)
		target_link_libraries(boost_serialization_dll boost_header)
	endif()
	if (USE_BOOST_FILESYSTEM)
		target_link_libraries(boost_filesystem_dll boost_header)
	endif()
	target_link_libraries(boost_thread_dll boost_header Threads::Threads)
	target_link_libraries(boost_context_dll boost_header)
	target_link_libraries(boost_coroutine_dll boost_context_static boost_header)
	if (USE_BOOST_FIBER)
		target_link_libraries(boost_fiber_dll boost_context_static boost_header)
	endif()
	target_link_libraries(boost_json_dll boost_header)
	target_link_libraries(boost_url_dll boost_header)
	target_link_libraries(boost_chrono_dll boost_header)
	if (USE_BOOST_LOCALE)
		target_link_libraries(boost_locale_dll boost_header)
	endif()
	target_link_libraries(boost_date_time_dll boost_chrono_static boost_header)
	target_link_libraries(boost_regex_dll boost_header)
	target_link_libraries(boost_timer_dll boost_chrono_static boost_header)
	target_link_libraries(boost_exception_dll boost_header)
	target_link_libraries(boost_random_dll boost_header)
	target_link_libraries(boost_program_options_dll boost_header)
	target_link_libraries(boost_asio_dll boost_thread_dll)
	target_link_libraries(boost_beast_dll boost_asio_dll)
endif()

if (WIN32)
	target_compile_definitions(boost_header INTERFACE -DWIN32_LEAN_AND_MEAN -DNOMINMAX)
	if ("${CMAKE_CXX_COMPILER_ID}" STREQUAL "Clang")
		target_compile_definitions(boost_header INTERFACE -DBOOST_SP_USE_STD_ATOMIC -DBOOST_USE_WINDOWS_H)
		set_property(SOURCE libs/locale/src/shared/message.cpp APPEND PROPERTY CMAKE_CXX_FLAGS "-O0")
	endif()
	target_compile_definitions(boost_context_static PUBLIC -DBOOST_USE_WINFIB)
else()
	if (HAVE_UCONTEXT)
		target_compile_definitions(boost_context_static PRIVATE -DBOOST_USE_UCONTEXT)
	endif()
endif()

set(BOOST_LIBRARIES
	Boost::boost
	Boost::serialization
	Boost::atomic
	Boost::context
	Boost::fiber
	Boost::json
	Boost::url
	Boost::coroutine
	Boost::chrono
	Boost::date_time
	Boost::regex
	Boost::timer
	Boost::exception
	Boost::filesystem
	Boost::thread
	Boost::random
	Boost::program_options
	Boost::locale
	Boost::test
PARENT_SCOPE)

if (Boost_USE_STATIC_LIBS)
	set_target_properties(boost_asio_static PROPERTIES FOLDER "third_party/boost")
	set_target_properties(boost_beast_static PROPERTIES FOLDER "third_party/boost")
	if (USE_BOOST_TEST)
		set_target_properties(boost_test_static PROPERTIES FOLDER "third_party/boost")
	endif()
	set_target_properties(boost_atomic_static PROPERTIES FOLDER "third_party/boost")
	if (USE_BOOST_SERIALIZATION)
		set_target_properties(boost_serialization_static PROPERTIES FOLDER "third_party/boost")
	endif()
	if (USE_BOOST_FILESYSTEM)
		set_target_properties(boost_filesystem_static PROPERTIES FOLDER "third_party/boost")
	endif()
	set_target_properties(boost_thread_static PROPERTIES FOLDER "third_party/boost")
	set_target_properties(boost_context_static PROPERTIES FOLDER "third_party/boost")
	if (USE_BOOST_FIBER)
		set_target_properties(boost_fiber_static PROPERTIES FOLDER "third_party/boost")
	endif()
	set_target_properties(boost_json_static PROPERTIES FOLDER "third_party/boost")
	set_target_properties(boost_url_static PROPERTIES FOLDER "third_party/boost")
	set_target_properties(boost_coroutine_static PROPERTIES FOLDER "third_party/boost")
	set_target_properties(boost_chrono_static PROPERTIES FOLDER "third_party/boost")
	if (USE_BOOST_LOCALE)
		set_target_properties(boost_locale_static PROPERTIES FOLDER "third_party/boost")
	endif()
	set_target_properties(boost_date_time_static PROPERTIES FOLDER "third_party/boost")
	set_target_properties(boost_regex_static PROPERTIES FOLDER "third_party/boost")
	set_target_properties(boost_timer_static PROPERTIES FOLDER "third_party/boost")
	set_target_properties(boost_exception_static PROPERTIES FOLDER "third_party/boost")
	set_target_properties(boost_random_static PROPERTIES FOLDER "third_party/boost")
	set_target_properties(boost_program_options_static PROPERTIES FOLDER "third_party/boost")
else()
	set_target_properties(boost_asio_dll PROPERTIES FOLDER "third_party/boost")
	set_target_properties(boost_beast_dll PROPERTIES FOLDER "third_party/boost")
	if (USE_BOOST_TEST)
		set_target_properties(boost_test_dll PROPERTIES FOLDER "third_party/boost")
	endif()
	set_target_properties(boost_atomic_dll PROPERTIES FOLDER "third_party/boost")
	if (USE_BOOST_SERIALIZATION)
		set_target_properties(boost_serialization_dll PROPERTIES FOLDER "third_party/boost")
	endif()
	if (USE_BOOST_FILESYSTEM)
		set_target_properties(boost_filesystem_dll PROPERTIES FOLDER "third_party/boost")
	endif()
	set_target_properties(boost_thread_dll PROPERTIES FOLDER "third_party/boost")
	set_target_properties(boost_context_dll PROPERTIES FOLDER "third_party/boost")
	set_target_properties(boost_fiber_dll PROPERTIES FOLDER "third_party/boost")
	set_target_properties(boost_json_dll PROPERTIES FOLDER "third_party/boost")
	set_target_properties(boost_url_dll PROPERTIES FOLDER "third_party/boost")
	set_target_properties(boost_coroutine_dll PROPERTIES FOLDER "third_party/boost")
	set_target_properties(boost_chrono_dll PROPERTIES FOLDER "third_party/boost")
	if (USE_BOOST_LOCALE)
		set_target_properties(boost_locale_dll PROPERTIES FOLDER "third_party/boost")
	endif()
	set_target_properties(boost_date_time_dll PROPERTIES FOLDER "third_party/boost")
	set_target_properties(boost_regex_dll PROPERTIES FOLDER "third_party/boost")
	set_target_properties(boost_timer_dll PROPERTIES FOLDER "third_party/boost")
	set_target_properties(boost_exception_dll PROPERTIES FOLDER "third_party/boost")
	set_target_properties(boost_random_dll PROPERTIES FOLDER "third_party/boost")
	set_target_properties(boost_program_options_dll PROPERTIES FOLDER "third_party/boost")
endif()
