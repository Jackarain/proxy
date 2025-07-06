//
// logging.hpp
// ~~~~~~~~~~~
//
// Copyright (c) 2023 Jack (jack dot wgm at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#ifndef INCLUDE__2016_10_14__LOGGING_HPP
#define INCLUDE__2016_10_14__LOGGING_HPP


#ifndef LOGGING_DISABLE_BOOST_ASIO_ENDPOINT
# if defined(__has_include)
#  if __has_include(<boost/asio.hpp>)
#   include <boost/asio/ip/tcp.hpp>
#   include <boost/asio/ip/udp.hpp>
#   include <boost/asio/ip/address.hpp>
#   include <boost/asio/ip/basic_endpoint.hpp>
#  else
#   define LOGGING_DISABLE_BOOST_ASIO_ENDPOINT
#  endif
# else
#  include <boost/asio/ip/tcp.hpp>
#  include <boost/asio/ip/udp.hpp>
#  include <boost/asio/ip/address.hpp>
#  include <boost/asio/ip/basic_endpoint.hpp>
# endif
#endif // !LOGGING_DISABLE_ASIO_ENDPOINT


#ifndef LOGGING_DISABLE_BOOST_POSIX_TIME
# if defined(__has_include)
#  if __has_include(<boost/date_time/posix_time/posix_time.hpp>)
#   include <boost/date_time/posix_time/posix_time.hpp>
#  else
#   define LOGGING_DISABLE_BOOST_POSIX_TIME
#  endif
# else
#  include <boost/date_time/posix_time/posix_time.hpp>
# endif
#endif

#ifndef LOGGING_DISABLE_BOOST_FILESYSTEM
# if defined(__has_include)
#  if __has_include(<boost/filesystem.hpp>)
#   include <boost/filesystem.hpp>
#  else
#   define LOGGING_DISABLE_BOOST_FILESYSTEM
#  endif
# else
#  include <boost/filesystem.hpp>
# endif
#endif

#ifndef LOGGING_DISABLE_BOOST_STRING_VIEW
# if defined(__has_include)
#  if __has_include(<boost/utility/string_view.hpp>)
#   include <boost/utility/string_view.hpp>
#  else
#   define LOGGING_DISABLE_BOOST_STRING_VIEW
#  endif
# else
#  include <boost/utility/string_view.hpp>
# endif
#endif

#ifdef WIN32
# ifndef LOGGING_DISABLE_AUTO_UTF8
#  define LOGGING_ENABLE_AUTO_UTF8
# endif // !LOGGING_DISABLE_WINDOWS_AUTO_UTF8
#endif // WIN32


//////////////////////////////////////////////////////////////////////////

#if defined(_WIN32) || defined(WIN32)
# ifndef WIN32_LEAN_AND_MEAN
#  define WIN32_LEAN_AND_MEAN
# endif // !WIN32_LEAN_AND_MEAN
# include <windows.h>
#endif // _WIN32

#ifdef USE_SYSTEMD_LOGGING
# if __has_include(<systemd/sd-journal.h>)
#  include <systemd/sd-journal.h>
# else
#  error "systemd/sd-journal.h not found"
# endif
#endif

#if defined(__ANDROID__)
# if __has_include(<android/log.h>)
#   include <android/log.h>
#   if !defined(DISABLE_WRITE_LOGGING) && !defined(ENABLE_ANDROID_LOG)
#	 define DISABLE_WRITE_LOGGING
#   endif
# else
#  error "android/log.h not found"
# endif
#endif

//////////////////////////////////////////////////////////////////////////
#ifndef LOGGING_DISABLE_COMPRESS_LOGS
# if defined(__has_include)
#  if __has_include(<zlib.h>)
#   include <zlib.h>
#   ifndef LOGGING_ENABLE_COMPRESS_LOGS
#    define LOGGING_ENABLE_COMPRESS_LOGS
#   endif
#  endif
# else
#  ifdef LOGGING_ENABLE_COMPRESS_LOGS
#   include <zlib.h>
#  endif
# endif
#endif

#if defined(__cpp_lib_format)
# include <format>
#endif

#if !defined(__cpp_lib_format)
# ifdef _MSC_VER
#  pragma warning(push)
#  pragma warning(disable: 4244 4127)
# endif // _MSC_VER

#if (_LIBCPP_VERSION < 170000) || defined(__ANDROID__)

# include <fmt/ostream.h>
# include <fmt/printf.h>
# include <fmt/format.h>

namespace std {
	using ::fmt::format;
	using ::fmt::format_to;
	using ::fmt::vformat;
	using ::fmt::make_format_args;
}

#endif // _LIBCPP_VERSION

# ifdef _MSC_VER
#  pragma warning(pop)
# endif
#endif

#include <version>
#include <codecvt>
#include <clocale>
#include <fstream>
#include <chrono>
#include <mutex>
#include <memory>
#include <string>
#include <tuple>
#include <thread>
#include <functional>
#include <filesystem>
#include <system_error>
#include <atomic>
#include <deque>
#include <csignal>
#include <condition_variable>
#include <optional>


//////////////////////////////////////////////////////////////////////////
//
// User customization function for hook log, function signature:
//
// bool tag_invoke(logger_tag,
//   int64_t time, const int& level, const std::string& message);
//
// For example:
//
// namespace xlogger {
//   bool tag_invoke(logger_tag,
//     int64_t time, const int& level, const std::string& message)
//   {
//     // do something.
//     return true;
//   }
// }
//
// namespace xlogger {
//  bool tag_invoke(logger_abort_tag)
//  {
//    // return true to abort and stopped logging.
//    return abort_flag;
//  }
// }
//


namespace xlogger {

	namespace fs = std::filesystem;

#ifndef LOGGING_DISABLE_BOOST_ASIO_ENDPOINT
	namespace net = boost::asio;
#endif

#ifndef LOG_APPNAME
#	define LOG_APPNAME "application"
#endif

#ifndef DEFAULT_LOG_MAXFILE_SIZE
#	define DEFAULT_LOG_MAXFILE_SIZE (-1)
#endif // DEFAULT_LOG_MAXFILE_SIZE


inline bool global_logging___ = true;
inline bool global_console_logging___ = true;
inline bool global_write_logging___ = true;
inline int64_t global_logfile_size___ = DEFAULT_LOG_MAXFILE_SIZE;


#ifdef LOGGING_ENABLE_COMPRESS_LOGS

namespace xlogging_compress__ {

	struct closefile_deleter {
		inline void operator()(FILE* fp) const {
			fclose(fp);
		}
	};

	struct closegz_deleter {
		inline void operator()(gzFile gz) const {
			gzclose(gz);
		}
	};

	const inline std::string LOGGING_GZ_SUFFIX = ".gz";
	const inline size_t LOGGING_GZ_BUFLEN = 65536;

	inline std::mutex& compress_lock()
	{
		static std::mutex lock;
		return lock;
	}

	inline bool do_compress_gz(const std::string& infile)
	{
		std::string outfile = infile + LOGGING_GZ_SUFFIX;

		gzFile out = gzopen(outfile.c_str(), "wb6f");
		if (!out)
			return false;

		using gzFileType = typename std::remove_pointer<gzFile>::type;
		std::unique_ptr<gzFileType, closegz_deleter> gz_closer(out);

		FILE* in = fopen(infile.c_str(), "rb");
		if (!in)
			return false;

		std::unique_ptr<FILE, closefile_deleter> FILE_closer(in);
		std::unique_ptr<char[]> bufs(new char[LOGGING_GZ_BUFLEN]);
		char* buf = bufs.get();
		int len;

		for (;;) {
			len = (int)fread(buf, 1, sizeof(buf), in);
			if (ferror(in))
				return false;

			if (len == 0)
				break;

			int total = 0;
			int ret;
			while (total < len) {
				ret = gzwrite(out, buf + total, (unsigned)len - total);
				if (ret <= 0) {
					// detail error information see gzerror(out, &ret);
					return false;
				}
				total += ret;
			}
		}

		return true;
	}

}

#endif


namespace logger_aux__ {

	inline constexpr long long epoch___ = 0x19DB1DED53E8000LL;

	inline int64_t gettime()
	{
#ifdef WIN32
		static std::tuple<LONGLONG, LONGLONG, LONGLONG>
			static_start = []() ->
			std::tuple<LONGLONG, LONGLONG, LONGLONG>
		{
			LARGE_INTEGER f;
			QueryPerformanceFrequency(&f);

			FILETIME ft;
#if (_WIN32_WINNT >= _WIN32_WINNT_WIN8)
			GetSystemTimePreciseAsFileTime(&ft);
#else
			GetSystemTimeAsFileTime(&ft);
#endif
			auto now = (((static_cast<long long>(ft.dwHighDateTime)) << 32)
				+ static_cast<long long>(ft.dwLowDateTime) - epoch___)
				/ 10000;

			LARGE_INTEGER start;
			QueryPerformanceCounter(&start);

			return { f.QuadPart / 1000, start.QuadPart, now };
		}();

		auto [freq, start, now] = static_start;

		LARGE_INTEGER current;
		QueryPerformanceCounter(&current);

		auto elapsed = current.QuadPart - start;
		elapsed /= freq;

		return static_cast<int64_t>(now + elapsed);
#else
		using std::chrono::system_clock;
		auto now = system_clock::now() -
			system_clock::time_point(std::chrono::milliseconds(0));

		return std::chrono::duration_cast<
			std::chrono::milliseconds>(now).count();
#endif
	}

	namespace internal {
		template <typename T = void>
		struct Null {};
		inline Null<> localtime_r(...) { return Null<>(); }
		inline Null<> localtime_s(...) { return Null<>(); }
		inline Null<> gmtime_r(...) { return Null<>(); }
		inline Null<> gmtime_s(...) { return Null<>(); }
	}

	// Thread-safe replacement for std::localtime
	inline bool localtime(std::time_t time, std::tm& tm)
	{
		struct LocalTime {
			std::time_t time_;
			std::tm tm_{0};

			explicit LocalTime(std::time_t t) : time_(t) {}

			inline bool run() {
				using namespace internal;
				return handle(localtime_r(&time_, &tm_));
			}

			inline bool handle(std::tm* tm) { return tm != nullptr; }

			inline bool handle(internal::Null<>) {
				using namespace internal;
				return fallback(localtime_s(&tm_, &time_));
			}

			inline bool fallback(int res) { return res == 0; }

			inline bool fallback(internal::Null<>) {
				using namespace internal;
				std::tm* tm = std::localtime(&time_);
				if (tm) tm_ = *tm;
				return tm != nullptr;
			}
		};

		LocalTime lt(time);
		if (lt.run()) {
			tm = lt.tm_;
			return true;
		}

		return false;
	}

	inline namespace utf {

	inline uint32_t decode(uint32_t* state, uint32_t* codep, uint32_t byte)
	{
		static constexpr uint8_t utf8d[] =
		{
			0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0, // 00..1f
			0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0, // 20..3f
			0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0, // 40..5f
			0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0, // 60..7f
			1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,9,9,9,9,9,9,9,9,9,9,9,9,9,9,9,9, // 80..9f
			7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7, // a0..bf
			8,8,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2, // c0..df
			0xa,0x3,0x3,0x3,0x3,0x3,0x3,0x3,0x3,0x3,0x3,0x3,0x3,0x4,0x3,0x3, // e0..ef
			0xb,0x6,0x6,0x6,0x5,0x8,0x8,0x8,0x8,0x8,0x8,0x8,0x8,0x8,0x8,0x8, // f0..ff
			0x0,0x1,0x2,0x3,0x5,0x8,0x7,0x1,0x1,0x1,0x4,0x6,0x1,0x1,0x1,0x1, // s0..s0
			1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,0,1,1,1,1,1,0,1,0,1,1,1,1,1,1, // s1..s2
			1,2,1,1,1,1,1,2,1,2,1,1,1,1,1,1,1,1,1,1,1,1,1,2,1,1,1,1,1,1,1,1, // s3..s4
			1,2,1,1,1,1,1,1,1,2,1,1,1,1,1,1,1,1,1,1,1,1,1,3,1,3,1,1,1,1,1,1, // s5..s6
			1,3,1,1,1,1,1,3,1,3,1,1,1,1,1,1,1,3,1,1,1,1,1,1,1,1,1,1,1,1,1,1, // s7..s8
		};

		uint32_t type = utf8d[byte];

		*codep = (*state != 0) ?
			(byte & 0x3fu) | (*codep << 6) :
			(0xff >> type) & (byte);

		*state = utf8d[256 + *state * 16 + type];
		return *state;
	}

	inline bool utf8_check_is_valid(std::string_view str)
	{
		uint32_t codepoint;
		uint32_t state = 0;
		uint8_t* s = (uint8_t*)str.data();
		uint8_t* end = s + str.size();

		for (; s != end; ++s)
			if (decode(&state, &codepoint, *s) == 1)
				return false;

		return state == 0;
	}

	inline std::optional<std::wstring> utf8_convert(std::string_view str)
	{
		uint8_t* start = (uint8_t*)str.data();
		uint8_t* end = start + str.size();

		std::wstring wstr;
		uint32_t codepoint;
		uint32_t state = 0;

		for (; start != end; ++start)
		{
			switch (decode(&state, &codepoint, *start))
			{
			case 0:
				if (codepoint <= 0xFFFF) [[likely]]
				{
					wstr.push_back(static_cast<wchar_t>(codepoint));
					continue;
				}
				wstr.push_back(static_cast<wchar_t>(0xD7C0 + (codepoint >> 10)));
				wstr.push_back(static_cast<wchar_t>(0xDC00 + (codepoint & 0x3FF)));
				continue;
			case 1:
				return {};
			default:
				;
			}
		}

		if (state != 0)
			return {};

		return wstr;
	}

	inline bool append(uint32_t cp, std::string& result)
	{
		if (!(cp <= 0x0010ffffu && !(cp >= 0xd800u && cp <= 0xdfffu)))
			return false;

		if (cp < 0x80)
		{
			result.push_back(static_cast<uint8_t>(cp));
		}
		else if (cp < 0x800)
		{
			result.push_back(static_cast<uint8_t>((cp >> 6) | 0xc0));
			result.push_back(static_cast<uint8_t>((cp & 0x3f) | 0x80));
		}
		else if (cp < 0x10000)
		{
			result.push_back(static_cast<uint8_t>((cp >> 12) | 0xe0));
			result.push_back(static_cast<uint8_t>(((cp >> 6) & 0x3f) | 0x80));
			result.push_back(static_cast<uint8_t>((cp & 0x3f) | 0x80));
		}
		else {
			result.push_back(static_cast<uint8_t>((cp >> 18) | 0xf0));
			result.push_back(static_cast<uint8_t>(((cp >> 12) & 0x3f) | 0x80));
			result.push_back(static_cast<uint8_t>(((cp >> 6) & 0x3f) | 0x80));
			result.push_back(static_cast<uint8_t>((cp & 0x3f) | 0x80));
		}

		return true;
	}

	inline std::optional<std::string> utf16_convert(std::wstring_view wstr)
	{
		std::string result;

		auto end = wstr.cend();
		for (auto start = wstr.cbegin(); start != end;)
		{
			uint32_t cp = static_cast<uint16_t>(0xffff & *start++);

			if (cp >= 0xdc00u && cp <= 0xdfffu) [[unlikely]]
				return {};

			if (cp >= 0xd800u && cp <= 0xdbffu)
			{
				if (start == end) [[unlikely]]
					return {};

				uint32_t trail = static_cast<uint16_t>(0xffff & *start++);
				if (!(trail >= 0xdc00u && trail <= 0xdfffu)) [[unlikely]]
					return {};

				cp = (cp << 10) + trail + 0xFCA02400;
			}

			if (!append(cp, result))
				return {};
		}

		if (result.empty())
			return {};

		return result;
	}

#ifdef WIN32
	inline std::optional<std::wstring> string_wide(const std::string_view& src)
	{
		auto len = MultiByteToWideChar(CP_ACP, 0,
			src.data(), static_cast<int>(src.size()), NULL, 0);
		if (len <= 0)
			return {};

		std::wstring ret(len, 0);
		MultiByteToWideChar(CP_ACP, 0,
			src.data(), static_cast<int>(src.size()),
			(LPWSTR)ret.data(), len);

		return ret;
	}

	inline std::optional<std::wstring> utf8_utf16(const std::string_view& src)
	{
		auto len = MultiByteToWideChar(CP_UTF8, 0,
			src.data(), static_cast<int>(src.size()), NULL, 0);
		if (len <= 0)
			return {};

		std::wstring ret(len, 0);
		MultiByteToWideChar(CP_UTF8, 0,
			src.data(), static_cast<int>(src.size()),
			(LPWSTR)ret.data(), len);

		return ret;
	}

	inline std::optional<std::string> utf16_utf8(std::wstring_view utf16)
	{
		auto len = WideCharToMultiByte(CP_UTF8, 0,
			(LPCWCH)utf16.data(), static_cast<int>(utf16.size()),
			NULL, 0, NULL, NULL);
		if (len <= 0)
			return {};

		std::string ret(len, 0);
		WideCharToMultiByte(CP_UTF8, 0,
			(LPCWCH)utf16.data(), static_cast<int>(utf16.size()),
			(LPSTR)ret.data(), len, NULL, NULL);

		return ret;
	}

#else
	inline std::optional<std::wstring> string_wide(const std::string_view& src)
	{
		const char* first = src.data();
		const char* last = src.data() + src.size();
		const char* snext = nullptr;

		std::wstring result(src.size() + 1, wchar_t{ 0 });

		wchar_t* dest = result.data();
		wchar_t* dnext = nullptr;

		using codecvt_type = std::codecvt<wchar_t, char, mbstate_t>;
		std::locale sys_locale("");
		mbstate_t in_state;

		auto ret = std::use_facet<codecvt_type>(sys_locale).in(
			in_state, first, last, snext, dest, dest + result.size(), dnext);
		if (ret != codecvt_type::ok)
			return {};

		result.resize(static_cast<size_t>(dnext - dest));
		return result;
	}

	inline std::optional<std::wstring> utf8_utf16(std::string_view utf8)
	{
		const char* first = &utf8[0];
		const char* last = first + utf8.size();
		const char8_t* snext = nullptr;

		std::wstring result(utf8.size(), char16_t{ 0 });
		wchar_t* dest = &result[0];
		char16_t* next = nullptr;

		using codecvt_type = std::codecvt<char16_t, char8_t, std::mbstate_t>;

		codecvt_type* cvt = new codecvt_type;

		// manages reference to codecvt facet to free memory.
		std::locale loc;
		loc = std::locale(loc, cvt);

		codecvt_type::state_type state{};

		auto ret = cvt->in(
			state, (char8_t*)first, (char8_t*)last, snext,
			(char16_t*)dest, (char16_t*)dest + result.size(), next);
		if (ret != codecvt_type::ok)
			return {};

		result.resize(static_cast<size_t>((wchar_t*)next - dest));
		return result;
	}

	inline std::optional<std::string> utf16_utf8(std::wstring_view utf16)
	{
		auto* first = &utf16[0];
		auto* last = first + utf16.size();

		std::string result((utf16.size() + 1) * 6, char{ 0 });
		char* dest = &result[0];
		char8_t* next = nullptr;

		using codecvt_type = std::codecvt<char16_t, char8_t, std::mbstate_t>;

		codecvt_type* cvt = new codecvt_type;
		// manages reference to codecvt facet to free memory.
		std::locale loc;
		loc = std::locale(loc, cvt);

		codecvt_type::state_type state{};
		const char16_t* snext = nullptr;
		auto ret = cvt->out(
			state, (char16_t*)first, (char16_t*)last, snext,
			(char8_t*)dest, (char8_t*)dest + result.size(), next);
		if (ret != codecvt_type::ok)
			return {};

		result.resize(static_cast<size_t>((char*)next - dest));
		return result;
	}
#endif

	} // namespace utf

	inline std::string from_u8string(const std::string& s)
	{
		return s;
	}

	inline std::string from_u8string(std::string&& s)
	{
		return s;
	}

#if defined(__cpp_lib_char8_t)
	inline std::string from_u8string(const std::u8string& s)
	{
		return std::string(s.begin(), s.end());
	}
#endif

	//////////////////////////////////////////////////////////////////////////

	template <class Lock>
	Lock& lock_single()
	{
		static Lock lock_instance;
		return lock_instance;
	}

	template <class Writer>
	Writer& writer_single(std::string log_path = "")
	{
		static Writer writer_instance(log_path);
		return writer_instance;
	}

	inline struct tm* time_to_string(char* buffer, int64_t time)
	{
		std::time_t rawtime = time / 1000;
		thread_local struct tm ptm;

		if (!localtime(rawtime, ptm))
			return nullptr;

		if (!buffer)
			return &ptm;

		std::format_to(buffer,
			"{:04}-{:02}-{:02} {:02}:{:02}:{:02}.{:03}",
			ptm.tm_year + 1900, ptm.tm_mon + 1, ptm.tm_mday,
			ptm.tm_hour, ptm.tm_min, ptm.tm_sec, (int)(time % 1000)
		);

		return &ptm;
	}
}


class auto_logger_file__
{
	// c++11 noncopyable.
	auto_logger_file__(const auto_logger_file__&) = delete;
	auto_logger_file__& operator=(const auto_logger_file__&) = delete;

public:
	explicit auto_logger_file__(std::string log_path = "")
	{
		if (!log_path.empty())
			m_log_path = log_path;

		if (global_logfile_size___ > 0 && global_logfile_size___ < 10 * 1024 * 1024) {
			// log file size must be greater than 10 MiB.
			global_logfile_size___ = DEFAULT_LOG_MAXFILE_SIZE;
		}

		m_log_path = m_log_path / (LOG_APPNAME + std::string(".log"));

		if (!global_logging___)
			return;

		std::error_code ignore_ec;
		if (!fs::exists(m_log_path, ignore_ec) && global_write_logging___)
			fs::create_directories(
				m_log_path.parent_path(), ignore_ec);
	}
	~auto_logger_file__()
	{
		m_last_time = 0;
	}

	typedef std::shared_ptr<std::ofstream> ofstream_ptr;

	inline void open(const char* path)
	{
		m_log_path = path;

		if (!global_logging___)
			return;

		std::error_code ignore_ec;
		if (!fs::exists(m_log_path, ignore_ec) && global_write_logging___)
			fs::create_directories(
				m_log_path.parent_path(), ignore_ec);
	}

	inline std::string log_path() const
	{
		return m_log_path.string();
	}

	inline void write([[maybe_unused]] int64_t time,
		const char* str, std::streamsize size)
	{
		bool condition = false;
		auto hours = time / 1000 / 3600;
		auto last_hours = m_last_time / 1000 / 3600;

		if (m_log_size > global_logfile_size___ &&
			global_logfile_size___ > 0)
			condition = true;

		if (last_hours != hours && global_logfile_size___ < 0)
			condition = true;

		while (condition) {
			if (m_last_time == -1) {
				m_last_time = time;
				break;
			}

			auto ptm = logger_aux__::time_to_string(nullptr, m_last_time);

			m_ofstream->close();
			m_ofstream.reset();

			auto logpath = fs::path(m_log_path.parent_path());
			fs::path filename;

			if (global_logfile_size___ <= 0) {
				auto logfile = std::format("{:04d}{:02d}{:02d}-{:02d}.log",
					ptm->tm_year + 1900,
					ptm->tm_mon + 1,
					ptm->tm_mday,
					ptm->tm_hour);
				filename = logpath / logfile;
			} else {
				auto utc_time = std::mktime(ptm);
				auto logfile = std::format("{:04d}{:02d}{:02d}-{}.log",
					ptm->tm_year + 1900,
					ptm->tm_mon + 1,
					ptm->tm_mday,
					utc_time);
				filename = logpath / logfile;
			}

			m_last_time = time;

			std::error_code ec;
			if (!fs::copy_file(m_log_path, filename, ec))
				break;

			fs::resize_file(m_log_path, 0, ec);
			m_log_size = 0;

#ifdef LOGGING_ENABLE_COMPRESS_LOGS
			auto fn = filename.string();
			std::thread th([fn]()
				{
					std::error_code ignore_ec;
					std::mutex& m = xlogging_compress__::compress_lock();
					std::lock_guard lock(m);
					if (!xlogging_compress__::do_compress_gz(fn))
					{
						auto file = fn + xlogging_compress__::LOGGING_GZ_SUFFIX;
						fs::remove(file, ignore_ec);
						if (ignore_ec)
							std::cerr
								<< "delete log failed: " << file
								<< ", error code: " << ignore_ec.message()
								<< std::endl;
						return;
					}

					fs::remove(fn, ignore_ec);
				});
			th.detach();
#endif
			break;
		}

		if (!m_ofstream) {
			m_ofstream.reset(new std::ofstream);
			auto& ofstream = *m_ofstream;
			ofstream.open(m_log_path.string().c_str(),
				std::ios_base::out | std::ios_base::app);
			ofstream.sync_with_stdio(false);
		}

		if (m_ofstream->is_open()) {
			m_log_size += static_cast<int64_t>(size);
			m_ofstream->write(str, size);
			m_ofstream->flush();
		}
	}

private:
	fs::path m_log_path{"./logs"};
	ofstream_ptr m_ofstream;
	int64_t m_last_time{ -1 };
	int64_t m_log_size{ 0 };
};

#ifndef DISABLE_LOGGER_THREAD_SAFE
#define LOGGER_LOCKS_() std::lock_guard \
	lock(logger_aux__::lock_single<std::mutex>())
#else
#define LOGGER_LOCKS_() ((void)0)
#endif // LOGGER_THREAD_SAFE

#ifndef LOGGER_DBG_VIEW_
#if defined(WIN32) && \
	(defined(LOGGER_DBG_VIEW) || \
	defined(DEBUG) || \
	defined(_DEBUG))
#define LOGGER_DBG_VIEW_(x)                \
	do {                                   \
		::OutputDebugStringW((x).c_str()); \
	} while (0)
#else
#define LOGGER_DBG_VIEW_(x) ((void)0)
#endif // WIN32 && LOGGER_DBG_VIEW
#endif // LOGGER_DBG_VIEW_

enum logger_level__ {
	_logger_debug_id__,
	_logger_info_id__,
	_logger_warn_id__,
	_logger_error_id__,
	_logger_file_id__
};

const inline std::string _LOGGER_STR__[] = {
	" DEBUG ",
	" INFO  ",
	" WARN  ",
	" ERROR ",
	" FILE  "
};

const inline std::string _LOGGER_DEBUG_STR__ = " DEBUG ";
const inline std::string _LOGGER_INFO_STR__  = " INFO  ";
const inline std::string _LOGGER_WARN_STR__  = " WARN  ";
const inline std::string _LOGGER_ERR_STR__   = " ERROR ";
const inline std::string _LOGGER_FILE_STR__  = " FILE  ";

inline void logger_output_console__([[maybe_unused]] const logger_level__& level,
	[[maybe_unused]] const std::string& prefix,
	[[maybe_unused]] const std::string& message) noexcept
{
#if defined(WIN32)

#if !defined(DISABLE_LOGGER_TO_CONSOLE) || !defined(DISABLE_LOGGER_TO_DBGVIEW)
	std::wstring title = *logger_aux__::utf8_utf16(prefix);
	std::wstring msg = *logger_aux__::utf8_utf16(message);
#endif

#if !defined(DISABLE_LOGGER_TO_CONSOLE)
	HANDLE handle_stdout = GetStdHandle(STD_OUTPUT_HANDLE);
	CONSOLE_SCREEN_BUFFER_INFO csbi;
	GetConsoleScreenBufferInfo(handle_stdout, &csbi);

	switch (level)
	{
	case _logger_info_id__:
		SetConsoleTextAttribute(handle_stdout,
			FOREGROUND_GREEN);
		break;
	case _logger_debug_id__:
		SetConsoleTextAttribute(handle_stdout,
			FOREGROUND_GREEN | FOREGROUND_INTENSITY);
		break;
	case _logger_warn_id__:
		SetConsoleTextAttribute(handle_stdout,
			FOREGROUND_GREEN | FOREGROUND_RED | FOREGROUND_INTENSITY);
		break;
	case _logger_error_id__:
		SetConsoleTextAttribute(handle_stdout,
			FOREGROUND_RED | FOREGROUND_INTENSITY);
		break;
	}

	WriteConsoleW(handle_stdout,
		title.data(), (DWORD)title.size(), nullptr, nullptr);
	SetConsoleTextAttribute(handle_stdout,
		FOREGROUND_GREEN | FOREGROUND_RED | FOREGROUND_BLUE);

	WriteConsoleW(handle_stdout,
		msg.data(), (DWORD)msg.size(), nullptr, nullptr);
	SetConsoleTextAttribute(handle_stdout, csbi.wAttributes);
#endif

#if !defined(DISABLE_LOGGER_TO_DBGVIEW)
	LOGGER_DBG_VIEW_(title + msg);
#endif

#elif !defined(DISABLE_LOGGER_TO_CONSOLE)
	std::string out;

	switch (level)
	{
	case _logger_info_id__:
		std::format_to(std::back_inserter(out),
			"\033[32m{}\033[0m{}", prefix, message);
		break;
	case _logger_debug_id__:
		std::format_to(std::back_inserter(out),
			"\033[1;32m{}\033[0m{}", prefix, message);
		break;
	case _logger_warn_id__:
		std::format_to(std::back_inserter(out),
			"\033[1;33m{}\033[0m{}", prefix, message);
		break;
	case _logger_error_id__:
		std::format_to(std::back_inserter(out),
			"\033[1;31m{}\033[0m{}", prefix, message);
		break;
	case _logger_file_id__:
		// std::format_to(std::back_inserter(out),
		//	"\033[1;34m{}\033[0m{}", prefix, message);
		break;
	}

	std::cout << out;
	std::cout.flush();
#endif
}

#ifdef USE_SYSTEMD_LOGGING
inline void logger_output_systemd__(
	const int& level, const std::string& message) noexcept
{
	if (level == _logger_info_id__)
		sd_journal_print(LOG_INFO, "%s", message.c_str());
	else if (level == _logger_debug_id__)
		sd_journal_print(LOG_DEBUG, "%s", message.c_str());
	else if (level == _logger_warn_id__)
		sd_journal_print(LOG_WARNING, "%s", message.c_str());
	else if (level == _logger_error_id__)
		sd_journal_print(LOG_ERR, "%s", message.c_str());
}
#endif // USE_SYSTEMD_LOGGING

#ifdef __ANDROID__
inline void logger_output_android__(
	const int& level, const std::string& message) noexcept
{
	if (level == _logger_info_id__)
		__android_log_print(ANDROID_LOG_INFO, LOG_APPNAME, "%s", message.c_str());
	else if (level == _logger_debug_id__)
		__android_log_print(ANDROID_LOG_DEBUG, LOG_APPNAME, "%s", message.c_str());
	else if (level == _logger_warn_id__)
		__android_log_print(ANDROID_LOG_WARN, LOG_APPNAME, "%s", message.c_str());
	else if (level == _logger_error_id__)
		__android_log_print(ANDROID_LOG_ERROR, LOG_APPNAME, "%s", message.c_str());
}
#endif // __ANDROID__

inline const std::string& logger_level_string__(const logger_level__& level) noexcept
{
	return _LOGGER_STR__[level];
}

struct logger_tag
{};

struct logger_abort_tag
{};

namespace access
{
	namespace detail
	{
		template <typename... T>
		bool tag_invoke(T...) noexcept
		{
			return false;
		}

		struct tag_invoke_t
		{
			template <typename Tag>
			bool operator()(Tag tag,
				int64_t time,
				const int& level,
				const std::string& message) noexcept
			{
				return tag_invoke(
					std::forward<Tag>(tag),
					time,
					level,
					message);
			}

			template <typename Tag>
			bool operator()(Tag tag) noexcept
			{
				return tag_invoke(
					std::forward<Tag>(tag));
			}
		};
	}

	inline detail::tag_invoke_t tag_invoke{};
}

inline void logger_writer__(int64_t time, const logger_level__& level,
	const std::string& message,
	[[maybe_unused]] bool disable_cout = false) noexcept
{
	LOGGER_LOCKS_();
	static auto& logger = xlogger::logger_aux__::writer_single<
		xlogger::auto_logger_file__>();
	char ts[64] = { 0 };
	[[maybe_unused]] auto ptm = logger_aux__::time_to_string(ts, time);
	std::string prefix = ts + logger_level_string__(level);
	std::string tmp = message + "\n";
	std::string whole = prefix + tmp;

	// User log hook.
	if (access::tag_invoke(logger_tag(), time, level, message))
		return;
	if (access::tag_invoke(logger_abort_tag()))
		return;

#ifndef DISABLE_WRITE_LOGGING
	if (global_write_logging___)
		logger.write(time, whole.c_str(), whole.size());
#endif // !DISABLE_WRITE_LOGGING

	// Output to systemd.
#ifdef USE_SYSTEMD_LOGGING
	logger_output_systemd__(level, message);
#endif // USE_SYSTEMD_LOGGING

	// Output to android.
#ifdef __ANDROID__
	logger_output_android__(level, message);
#endif // __ANDROID__

	// Output to console.
#if !defined(USE_SYSTEMD_LOGGING) && !defined(__ANDROID__)
	if (global_console_logging___ && !disable_cout)
		logger_output_console__(level, prefix, tmp);
#endif
}

#if defined(_WIN32) || defined(WIN32)
static LONG WINAPI unexpectedExceptionHandling(EXCEPTION_POINTERS* info);
#endif
void signal_handler(int);

namespace logger_aux__ {
	using namespace std::chrono_literals;

	class async_logger___
	{
		struct internal_message
		{
			logger_level__ level_;
			int64_t time_;
			std::string message_;
			bool disable_cout_;
		};

		// c++11 noncopyable.
		async_logger___(const async_logger___&) = delete;
		async_logger___& operator=(const async_logger___&) = delete;

	public:
		async_logger___()
		{
			// 实现Crash handler以接管在crash时
			// 不会漏写日志.

#if defined(_WIN32) || defined(WIN32)
			m_unexpected_exception_handler =
				SetUnhandledExceptionFilter(unexpectedExceptionHandling);
#endif
			signal(SIGTERM, signal_handler);
			signal(SIGABRT, signal_handler);
			signal(SIGFPE, signal_handler);
			signal(SIGSEGV, signal_handler);
			signal(SIGILL, signal_handler);
		}
		~async_logger___()
		{
			m_abort = true;
			if (m_internal_thread.joinable())
				m_internal_thread.join();
		}

	public:
#if defined(_WIN32) || defined(WIN32)
		inline LPTOP_LEVEL_EXCEPTION_FILTER oldUnhandledExceptionFilter()
		{
			return m_unexpected_exception_handler;
		}
#endif

		inline void stop()
		{
			m_abort = true;
		}

		inline void internal_work()
		{
			while (!m_abort || !m_messages.empty())
			{
				std::unique_lock lock(m_internal_mutex);

				if (m_messages.empty())
					m_internal_cv.wait_for(lock, 128ms);

				while (!m_messages.empty())
				{
					auto message = std::move(m_messages.front());
					m_messages.pop_front();

					logger_writer__(message.time_,
						message.level_,
						message.message_,
						message.disable_cout_);
				}
			}
		}

		inline void post_log(const logger_level__& level,
			std::string&& message, bool disable_cout = false)
		{
			[[maybe_unused]] static auto runthread =
				&(m_internal_thread = std::thread([this]()
					{
						internal_work();
					}));

			auto time = logger_aux__::gettime();
			std::unique_lock lock(m_internal_mutex);

			m_messages.emplace_back(
				internal_message
				{
				.level_ = level,
				.time_ = time,
				.message_ = std::move(message),
				.disable_cout_ = disable_cout
				}
			);
			lock.unlock();

			m_internal_cv.notify_one();
		}

	private:
		std::thread m_internal_thread;
		std::mutex m_internal_mutex;
		std::condition_variable m_internal_cv;
		std::deque<internal_message> m_messages;
		std::atomic_bool m_abort{ false };
#if defined(_WIN32) || defined(WIN32)
		LPTOP_LEVEL_EXCEPTION_FILTER m_unexpected_exception_handler{ nullptr };
#endif
	};
}

inline std::shared_ptr<logger_aux__::async_logger___> global_logger_obj___ =
	std::make_shared<logger_aux__::async_logger___>();

#if defined(_WIN32) || defined(WIN32)
static LONG WINAPI unexpectedExceptionHandling(EXCEPTION_POINTERS* e)
{
	if (!global_logger_obj___)
		return EXCEPTION_CONTINUE_SEARCH;

	auto old = global_logger_obj___->oldUnhandledExceptionFilter();
	SetUnhandledExceptionFilter(old);

	global_logger_obj___.reset();

	return old(e);
}
#endif

inline void signal_handler(int)
{
	global_logger_obj___.reset();
}

inline void init_logging(const std::string& path = "")
{
	logger_aux__::writer_single<xlogger::auto_logger_file__>(path);
}

inline std::string log_path()
{
	auto_logger_file__& file =
		logger_aux__::writer_single<xlogger::auto_logger_file__>();
	return file.log_path();
}

inline void shutdown_logging()
{
	auto& log_obj = global_logger_obj___;
	if (log_obj) {
		log_obj->stop();
		log_obj.reset();
	}
}

inline void turnoff_logging() noexcept
{
	global_logging___ = false;
}

inline void turnon_logging() noexcept
{
	global_logging___ = true;
}

inline void toggle_write_logging(bool enable)
{
	global_write_logging___ = enable;
}

inline void toggle_console_logging(bool enable)
{
	global_console_logging___ = enable;
}

inline void set_logfile_maxsize(int64_t size) noexcept
{
	// log file size must be greater than 10 MiB.
	if (size > 0 && size < 10 * 1024 * 1024)
		return;

	// set log file size.
	global_logfile_size___ = size;
}

struct auto_init_async_logger
{
	auto_init_async_logger() {
		init_logging();
	}
	~auto_init_async_logger() {
		shutdown_logging();
	}
};

class logger___
{
	// c++11 noncopyable.
	logger___(const logger___&) = delete;
	logger___& operator=(const logger___&) = delete;
public:
	logger___(const logger_level__& level,
		bool async = false, bool disable_cout = false)
		: level_(level)
		, async_(async)
		, disable_cout_(disable_cout)
	{
		if (!global_logging___)
			return;
	}
	~logger___()
	{
		if (!global_logging___)
			return;

		// if global_logger_obj___ is nullptr, fallback to
		// synchronous operation.
		if (async_ && global_logger_obj___)
			global_logger_obj___->post_log(
				level_, std::move(out_), disable_cout_);
		else
			logger_writer__(logger_aux__::gettime(),
				level_, out_, disable_cout_);
	}

	template <class... Args>
	inline logger___& format_to(std::string_view fmt, Args&&... args)
	{
		if (!global_logging___)
			return *this;
		out_ += std::vformat(fmt,
			std::make_format_args(args...));
		return *this;
	}

	template <class T>
	inline logger___& strcat_impl(T const& v) noexcept
	{
		if (!global_logging___)
			return *this;
		std::format_to(std::back_inserter(out_), "{}", v);
		return *this;
	}

	inline logger___& operator<<(bool v)
	{
		return strcat_impl(v);
	}
	inline logger___& operator<<(char v)
	{
		return strcat_impl(v);
	}
	inline logger___& operator<<(short v)
	{
		return strcat_impl(v);
	}
	inline logger___& operator<<(unsigned short v)
	{
		return strcat_impl(v);
	}
	inline logger___& operator<<(int v)
	{
		return strcat_impl(v);
	}
	inline logger___& operator<<(unsigned int v)
	{
		return strcat_impl(v);
	}
	inline logger___& operator<<(unsigned long long v)
	{
		return strcat_impl(v);
	}
	inline logger___& operator<<(long v)
	{
		return strcat_impl(v);
	}
	inline logger___& operator<<(long long v)
	{
		return strcat_impl(v);
	}
	inline logger___& operator<<(float v)
	{
		return strcat_impl(v);
	}
	inline logger___& operator<<(double v)
	{
		return strcat_impl(v);
	}
	inline logger___& operator<<(long double v)
	{
		return strcat_impl(v);
	}
	inline logger___& operator<<(unsigned long int v)
	{
		return strcat_impl(v);
	}
	inline logger___& operator<<(const std::string& v)
	{
#ifdef LOGGING_ENABLE_AUTO_UTF8
		if (!global_logging___)
			return *this;
		if (!logger_aux__::utf8_check_is_valid(v))
		{
			auto wres = logger_aux__::string_wide(v);
			if (wres)
			{
				auto ret = logger_aux__::utf16_utf8(*wres);
				if (ret)
					return strcat_impl(*ret);
			}
		}
#endif
		return strcat_impl(v);
	}
	inline logger___& operator<<(const std::wstring& v)
	{
		if (!global_logging___)
			return *this;
		return strcat_impl(*logger_aux__::utf16_utf8(v));
	}
	inline logger___& operator<<(const std::u16string& v)
	{
		if (!global_logging___)
			return *this;
		return strcat_impl(*logger_aux__::utf16_utf8(
			{(const wchar_t*)v.data(), v.size()}));
	}
#if (__cplusplus >= 202002L)
	inline logger___& operator<<(const std::u8string& v)
	{
		return strcat_impl(reinterpret_cast<const char*>(v.c_str()));
	}
#endif
	inline logger___& operator<<(const std::string_view& v)
	{
#ifdef LOGGING_ENABLE_AUTO_UTF8
		if (!global_logging___)
			return *this;
		if (!logger_aux__::utf8_check_is_valid(v))
		{
			auto wres = logger_aux__::string_wide(v);
			if (wres)
			{
				auto ret = logger_aux__::utf16_utf8(*wres);
				if (ret)
					return strcat_impl(*ret);
			}
		}
#endif
		return strcat_impl(v);
	}
	inline logger___& operator<<(const boost::string_view& v)
	{
		std::string_view sv{v.data(), v.length()};
#ifdef LOGGING_ENABLE_AUTO_UTF8
		if (!global_logging___)
			return *this;
		if (!logger_aux__::utf8_check_is_valid(sv))
		{
			auto wres = logger_aux__::string_wide(sv);
			if (wres)
			{
				auto ret = logger_aux__::utf16_utf8(*wres);
				if (ret)
					return strcat_impl(*ret);
			}
		}
#endif
		return strcat_impl(sv);
	}
	inline logger___& operator<<(const char* v)
	{
		std::string_view sv(v);
#ifdef LOGGING_ENABLE_AUTO_UTF8
		if (!global_logging___)
			return *this;
		if (!logger_aux__::utf8_check_is_valid(sv))
		{
			auto wres = logger_aux__::string_wide(sv);
			if (wres)
			{
				auto ret = logger_aux__::utf16_utf8(*wres);
				if (ret)
					return strcat_impl(*ret);
			}
		}
#endif
		return strcat_impl(sv);
	}
	inline logger___& operator<<(const wchar_t* v)
	{
		if (!global_logging___)
			return *this;
		return strcat_impl(*logger_aux__::utf16_utf8(v));
	}
	inline logger___& operator<<(const void *v)
	{
		if (!global_logging___)
			return *this;
		std::format_to(std::back_inserter(out_), "{:#010x}", (std::size_t)v);
		return *this;
	}
	inline logger___& operator<<(const std::chrono::nanoseconds& v)
	{
		if (!global_logging___)
			return *this;
		std::format_to(std::back_inserter(out_), "{}ns", v.count());
		return *this;
	}
	inline logger___& operator<<(const std::chrono::microseconds& v)
	{
		if (!global_logging___)
			return *this;
		std::format_to(std::back_inserter(out_), "{}us", v.count());
		return *this;
	}
	inline logger___& operator<<(const std::chrono::milliseconds& v)
	{
		if (!global_logging___)
			return *this;
		std::format_to(std::back_inserter(out_), "{}ms", v.count());
		return *this;
	}
	inline logger___& operator<<(const std::chrono::seconds& v)
	{
		if (!global_logging___)
			return *this;
		std::format_to(std::back_inserter(out_), "{}s", v.count());
		return *this;
	}
	inline logger___& operator<<(const std::chrono::minutes& v)
	{
		if (!global_logging___)
			return *this;
		std::format_to(std::back_inserter(out_), "{}min", v.count());
		return *this;
	}
	inline logger___& operator<<(const std::chrono::hours& v)
	{
		if (!global_logging___)
			return *this;
		std::format_to(std::back_inserter(out_), "{}h", v.count());
		return *this;
	}

#ifndef LOGGING_DISABLE_BOOST_ASIO_ENDPOINT
	inline logger___& operator<<(const net::ip::tcp::endpoint& v)
	{
		if (!global_logging___)
			return *this;
		if (v.address().is_v6())
			std::format_to(std::back_inserter(out_),
				"[{}]:{}", v.address().to_string(), v.port());
		else
			std::format_to(std::back_inserter(out_),
				"{}:{}", v.address().to_string(), v.port());
		return *this;
	}
	inline logger___& operator<<(const net::ip::udp::endpoint& v)
	{
		if (!global_logging___)
			return *this;
		if (v.address().is_v6())
			std::format_to(std::back_inserter(out_),
				"[{}]:{}", v.address().to_string(), v.port());
		else
			std::format_to(std::back_inserter(out_),
				"{}:{}", v.address().to_string(), v.port());
		return *this;
	}
#endif

#if (__cplusplus >= 202002L)
	inline logger___& operator<<(const std::chrono::days& v)
	{
		if (!global_logging___)
			return *this;
		std::format_to(std::back_inserter(out_), "{}d", v.count());
		return *this;
	}
	inline logger___& operator<<(const std::chrono::weeks& v)
	{
		if (!global_logging___)
			return *this;
		std::format_to(std::back_inserter(out_), "{}weeks", v.count());
		return *this;
	}
	inline logger___& operator<<(const std::chrono::years& v)
	{
		if (!global_logging___)
			return *this;
		std::format_to(std::back_inserter(out_), "{}years", v.count());
		return *this;
	}
	inline logger___& operator<<(const std::chrono::months& v)
	{
		if (!global_logging___)
			return *this;
		std::format_to(std::back_inserter(out_), "{}months", v.count());
		return *this;
	}
	inline logger___& operator<<(const std::chrono::weekday& v)
	{
		if (!global_logging___)
			return *this;
		switch (v.c_encoding())
		{
#ifndef __cpp_lib_char8_t
		case 0:	out_ = "Sunday"; break;
		case 1:	out_ = "Monday"; break;
		case 2:	out_ = "Tuesday"; break;
		case 3:	out_ = "Wednesday"; break;
		case 4:	out_ = "Thursday"; break;
		case 5:	out_ = "Friday"; break;
		case 6:	out_ = "Saturday"; break;
#else
		case 0:	out_ = logger_aux__::from_u8string(u8"周日"); break;
		case 1:	out_ = logger_aux__::from_u8string(u8"周一"); break;
		case 2:	out_ = logger_aux__::from_u8string(u8"周二"); break;
		case 3:	out_ = logger_aux__::from_u8string(u8"周三"); break;
		case 4:	out_ = logger_aux__::from_u8string(u8"周四"); break;
		case 5:	out_ = logger_aux__::from_u8string(u8"周五"); break;
		case 6:	out_ = logger_aux__::from_u8string(u8"周六"); break;
#endif
		}
		return *this;
	}
	inline logger___& operator<<(const std::chrono::year& v)
	{
		if (!global_logging___)
			return *this;
#if 0
		std::format_to(std::back_inserter(out_),
			"{:04}", static_cast<int>(v));
#else
		std::format_to(std::back_inserter(out_),
			"{:04}{}", static_cast<int>(v),
				logger_aux__::from_u8string(u8"年"));
#endif
		return *this;
	}
	inline logger___& operator<<(const std::chrono::month& v)
	{
		if (!global_logging___)
			return *this;
		switch (static_cast<unsigned int>(v))
		{
#ifndef __cpp_lib_char8_t
		case  1: out_ = "January"; break;
		case  2: out_ = "February"; break;
		case  3: out_ = "March"; break;
		case  4: out_ = "April"; break;
		case  5: out_ = "May"; break;
		case  6: out_ = "June"; break;
		case  7: out_ = "July"; break;
		case  8: out_ = "August"; break;
		case  9: out_ = "September"; break;
		case 10: out_ = "October"; break;
		case 11: out_ = "November"; break;
		case 12: out_ = "December"; break;
#else
		case  1: out_ = logger_aux__::from_u8string(u8"01月"); break;
		case  2: out_ = logger_aux__::from_u8string(u8"02月"); break;
		case  3: out_ = logger_aux__::from_u8string(u8"03月"); break;
		case  4: out_ = logger_aux__::from_u8string(u8"04月"); break;
		case  5: out_ = logger_aux__::from_u8string(u8"05月"); break;
		case  6: out_ = logger_aux__::from_u8string(u8"06月"); break;
		case  7: out_ = logger_aux__::from_u8string(u8"07月"); break;
		case  8: out_ = logger_aux__::from_u8string(u8"08月"); break;
		case  9: out_ = logger_aux__::from_u8string(u8"09月"); break;
		case 10: out_ = logger_aux__::from_u8string(u8"10月"); break;
		case 11: out_ = logger_aux__::from_u8string(u8"11月"); break;
		case 12: out_ = logger_aux__::from_u8string(u8"12月"); break;
#endif
		}
		return *this;
	}
	inline logger___& operator<<(const std::chrono::day& v)
	{
		if (!global_logging___)
			return *this;
#ifndef __cpp_lib_char8_t
		std::format_to(std::back_inserter(out_),
			"{:02}", static_cast<int>(v));
#else
		std::format_to(std::back_inserter(out_),
			"{:02}{}", static_cast<unsigned int>(v),
				logger_aux__::from_u8string(u8"日"));
#endif
		return *this;
	}
#endif
	inline logger___& operator<<(const fs::path& p) noexcept
	{
		if (!global_logging___)
			return *this;
		auto ret = logger_aux__::utf16_utf8(p.wstring());
		if (ret)
			return strcat_impl(*ret);
		return strcat_impl(p.string());
	}
#ifndef LOGGING_DISABLE_BOOST_FILESYSTEM
	inline logger___& operator<<(const boost::filesystem::path& p) noexcept
	{
		if (!global_logging___)
			return *this;
		auto ret = logger_aux__::utf16_utf8(p.wstring());
		if (ret)
			return strcat_impl(*ret);
		return strcat_impl(p.string());
	}
#endif
#ifndef LOGGING_DISABLE_BOOST_POSIX_TIME
	inline logger___& operator<<(const boost::posix_time::ptime& p) noexcept
	{
		if (!global_logging___)
			return *this;

		if (!p.is_not_a_date_time())
		{
			auto date = p.date().year_month_day();
			auto time = p.time_of_day();

			std::format_to(std::back_inserter(out_),
				"{:04}", static_cast<unsigned int>(date.year));
			std::format_to(std::back_inserter(out_),
				"-{:02}", date.month.as_number());
			std::format_to(std::back_inserter(out_),
				"-{:02}", date.day.as_number());

			std::format_to(std::back_inserter(out_),
				" {:02}", time.hours());
			std::format_to(std::back_inserter(out_),
				":{:02}", time.minutes());
			std::format_to(std::back_inserter(out_),
				":{:02}", time.seconds());

			auto ms = time.total_milliseconds() % 1000;		// milliseconds.
			if (ms != 0)
				std::format_to(std::back_inserter(out_),
					".{:03}", ms);
		}
		else
		{
			BOOST_ASSERT("Not a date time" && false);
			out_ += "NOT A DATE TIME";
		}

		return *this;
	}
#endif
	inline logger___& operator<<(const std::thread::id& id) noexcept
	{
		if (!global_logging___)
			return *this;
		std::ostringstream oss;
		oss << id;
		out_ += oss.str();
		return *this;
	}

	std::string out_;
	const logger_level__& level_;
	bool async_;
	bool disable_cout_;
};

class empty_logger___
{
public:
	template <class T>
	empty_logger___& operator<<(T const&/*v*/)
	{
		return *this;
	}
};
} // namespace xlogger

#if (defined(DEBUG) || defined(_DEBUG) || \
	defined(ENABLE_LOGGER)) && !defined(DISABLE_LOGGER)

// API for logging.
namespace xlogger {
	inline void init_logging(const std::string& path/* = ""*/);
	inline std::string log_path();
	inline std::string log_path();
	inline void shutdown_logging();
	inline void turnoff_logging() noexcept;
	inline void turnon_logging() noexcept;
	inline void toggle_write_logging(bool enable);
	inline void toggle_console_logging(bool enable);
	inline void set_logfile_maxsize(int64_t size) noexcept;
}

// API for logging.
#define XLOG_DBG xlogger::logger___(xlogger::_logger_debug_id__)
#define XLOG_INFO xlogger::logger___(xlogger::_logger_info_id__)
#define XLOG_WARN xlogger::logger___(xlogger::_logger_warn_id__)
#define XLOG_ERR xlogger::logger___(xlogger::_logger_error_id__)
#define XLOG_FILE xlogger::logger___(xlogger::_logger_file_id__, false, true)

// API for logging.
#define XLOG_FDBG(...) xlogger::logger___( \
		xlogger::_logger_debug_id__).format_to(__VA_ARGS__)
#define XLOG_FINFO(...) xlogger::logger___( \
		xlogger::_logger_info_id__).format_to(__VA_ARGS__)
#define XLOG_FWARN(...) xlogger::logger___( \
		xlogger::_logger_warn_id__).format_to(__VA_ARGS__)
#define XLOG_FERR(...) xlogger::logger___( \
		xlogger::_logger_error_id__).format_to(__VA_ARGS__)
#define XLOG_FFILE(...) xlogger::logger___( \
		xlogger::_logger_file_id__, false, true).format_to(__VA_ARGS__)

#define AXLOG_DBG xlogger::logger___(xlogger::_logger_debug_id__, true)
#define AXLOG_INFO xlogger::logger___(xlogger::_logger_info_id__, true)
#define AXLOG_WARN xlogger::logger___(xlogger::_logger_warn_id__, true)
#define AXLOG_ERR xlogger::logger___(xlogger::_logger_error_id__, true)
#define AXLOG_FILE xlogger::logger___(xlogger::_logger_file_id__, true, true)

#define AXLOG_FDBG(...) xlogger::logger___( \
		xlogger::_logger_debug_id__, true).format_to(__VA_ARGS__)
#define AXLOG_FINFO(...) xlogger::logger___( \
		xlogger::_logger_info_id__, true).format_to(__VA_ARGS__)
#define AXLOG_FWARN(...) xlogger::logger___( \
		xlogger::_logger_warn_id__, true).format_to(__VA_ARGS__)
#define AXLOG_FERR(...) xlogger::logger___( \
		xlogger::_logger_error_id__, true).format_to(__VA_ARGS__)
#define AXLOG_FFILE(...) xlogger::logger___( \
		xlogger::_logger_file_id__, true, true).format_to(__VA_ARGS__)

#define AXVLOG_DBG AXLOG_DBG \
	<< "(" << __FILE__ << ":" << __LINE__ << "): "
#define AXVLOG_INFO AXLOG_INFO \
	<< "(" << __FILE__ << ":" << __LINE__ << "): "
#define AXVLOG_WARN AXLOG_WARN \
	<< "(" << __FILE__ << ":" << __LINE__ << "): "
#define AXVLOG_ERR AXLOG_ERR \
	<< "(" << __FILE__ << ":" << __LINE__ << "): "
#define AXVLOG_FILE AXLOG_FILE \
	<< "(" << __FILE__ << ":" << __LINE__ << "): "

#define AXVLOG_FMT(...) (AXLOG_DBG << "(" \
		<< __FILE__ << ":" << __LINE__ << "): ").format_to(__VA_ARGS__)
#define AXVLOG_IFMT(...) (AXLOG_INFO << "(" \
		<< __FILE__ << ":" << __LINE__ << "): ").format_to(__VA_ARGS__)
#define AXVLOG_WFMT(...) (AXLOG_WARN << "(" \
		<< __FILE__ << ":" << __LINE__ << "): ").format_to(__VA_ARGS__)
#define AXVLOG_EFMT(...) (AXLOG_ERR << "(" \
		<< __FILE__ << ":" << __LINE__ << "): ").format_to(__VA_ARGS__)
#define AXVLOG_FFMT(...) (AXLOG_FILE << "(" \
		<< __FILE__ << ":" << __LINE__ << "): ").format_to(__VA_ARGS__)

#define VXLOG_DBG XLOG_DBG << "(" << __FILE__ << ":" << __LINE__ << "): "
#define VXLOG_INFO XLOG_INFO << "(" << __FILE__ << ":" << __LINE__ << "): "
#define VXLOG_WARN XLOG_WARN << "(" << __FILE__ << ":" << __LINE__ << "): "
#define VXLOG_ERR XLOG_ERR << "(" << __FILE__ << ":" << __LINE__ << "): "
#define VXLOG_FILE XLOG_FILE << "(" << __FILE__ << ":" << __LINE__ << "): "

#define VXLOG_FDBG(...) (XLOG_DBG << "(" \
		<< __FILE__ << ":" << __LINE__ << "): ").format_to(__VA_ARGS__)
#define VXLOG_FINFO(...) (XLOG_INFO << "(" \
		<< __FILE__ << ":" << __LINE__ << "): ").format_to(__VA_ARGS__)
#define VXLOG_FWARN(...) (XLOG_WARN << "(" \
		<< __FILE__ << ":" << __LINE__ << "): ").format_to(__VA_ARGS__)
#define VXLOG_FERR(...) (XLOG_ERR << "(" \
		<< __FILE__ << ":" << __LINE__ << "): ").format_to(__VA_ARGS__)
#define VXLOG_FFILE(...) (XLOG_FILE << "(" \
		<< __FILE__ << ":" << __LINE__ << "): ").format_to(__VA_ARGS__)


#define INIT_ASYNC_LOGGING() [[maybe_unused]] \
		xlogger::auto_init_async_logger ____init_logger____

#else

#define XLOG_DBG xlogger::empty_logger___()
#define XLOG_INFO xlogger::empty_logger___()
#define XLOG_WARN xlogger::empty_logger___()
#define XLOG_ERR xlogger::empty_logger___()
#define XLOG_FILE xlogger::empty_logger___()

#define XLOG_FDBG(...) xlogger::empty_logger___()
#define XLOG_FINFO(...) xlogger::empty_logger___()
#define XLOG_FWARN(...) xlogger::empty_logger___()
#define XLOG_FERR(...) xlogger::empty_logger___()
#define XLOG_FFILE(...) xlogger::empty_logger___()

#define VXLOG_DBG(...) xlogger::empty_logger___()
#define VXLOG_INFO(...) xlogger::empty_logger___()
#define VXLOG_WARN(...) xlogger::empty_logger___()
#define VXLOG_ERR(...) xlogger::empty_logger___()
#define VXLOG_FILE(...) xlogger::empty_logger___()

#define VXLOG_FDBG(...) xlogger::empty_logger___()
#define VXLOG_FINFO(...) xlogger::empty_logger___()
#define VXLOG_FWARN(...) xlogger::empty_logger___()
#define VXLOG_FERR(...) xlogger::empty_logger___()
#define VXLOG_FFILE(...) xlogger::empty_logger___()

#define AXLOG_DBG xlogger::empty_logger___()
#define AXLOG_INFO xlogger::empty_logger___()
#define AXLOG_WARN xlogger::empty_logger___()
#define AXLOG_ERR xlogger::empty_logger___()
#define AXLOG_FILE xlogger::empty_logger___()

#define AXLOG_FDBG(...) xlogger::empty_logger___()
#define AXLOG_FINFO(...) xlogger::empty_logger___()
#define AXLOG_FWARN(...) xlogger::empty_logger___()
#define AXLOG_FERR(...) xlogger::empty_logger___()
#define AXLOG_FFILE(...) xlogger::empty_logger___()

#define AXVLOG_DBG XLOG_DBG
#define AXVLOG_INFO XLOG_INFO
#define AXVLOG_WARN XLOG_WARN
#define AXVLOG_ERR XLOG_ERR
#define AXVLOG_FILE XLOG_FILE

#define AXVLOG_FMT XLOG_FDBG
#define AXVLOG_IFMT XLOG_FINFO
#define AXVLOG_WFMT XLOG_FWARN
#define AXVLOG_EFMT XLOG_FERR
#define AXVLOG_FFMT XLOG_FFILE

#define INIT_ASYNC_LOGGING() void

#endif

#endif // INCLUDE__2016_10_14__LOGGING_HPP
