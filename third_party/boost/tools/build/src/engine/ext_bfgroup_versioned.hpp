// Copyright René Ferdinand Rivera Morell
// Distributed under the Boost Software License, Version 1.0. (See
// versioned_narrowc_accumulateompanying file LICENSE.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)

#ifndef VERSIONED_VERSIONED_HPP
#define VERSIONED_VERSIONED_HPP

#include <algorithm>
#include <limits>
#include <system_error>
#include <tuple>
#include <type_traits>
#include <vector>

#if defined(__cpp_lib_to_chars)
#	include <charconv>
#endif

#define VERSIONED_VERSION_MAJOR 0
#define VERSIONED_VERSION_MINOR 1
#define VERSIONED_VERSION_PATCH 0

#define VERSIONED_VERSION \
	(((VERSIONED_VERSION_MAJOR) * 10000000) \
		+ ((VERSIONED_VERSION_MINOR) * 100000) + (VERSIONED_VERSION_PATCH))

// Some platform define inconvenient macros.
#ifdef major
#	undef major
#endif
#ifdef minor
#	undef minor
#endif

namespace bfg { namespace versioned { namespace detail {

// The decimal digits.
inline constexpr bool is_digit(const char c) { return c >= '0' && c <= '9'; }

// Alphanumeric, for identifiers.
inline constexpr bool is_alphanum(const char c)
{
	return is_digit(c) || (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z');
}

// Identifier allowed character.
inline constexpr bool is_ident(const char c)
{
	return is_alphanum(c) || (c == '-');
}

// Base 10 only positive integer parsing. Believe it or not, this rather
// simple code provides the fastest throughput for parsing sequential
// integers.
template <typename I>
const char * from_chars_10(const char * first, const char * last, I & value)
{
	I e = 0;
	while (first < last && is_digit(*first)) e = e * 10 + (*(first++) - '0');
	value = e;
	return first;
}

// Compare, as ASCII, two string ranges.
inline int chars_cmp(
	const char * a0, const char * an, const char * b0, const char * bn)
{
	while ((a0 < an) && (b0 < bn) && (*a0 == *b0))
	{
		a0 += 1;
		b0 += 1;
	}
	return *a0 - *b0;
}

template <typename N, typename U>
constexpr N masked_max(U value)
{
	return value & static_cast<U>(::std::numeric_limits<N>::max());
}

// Simple hash values combine.
template <class T, class... N>
T hash_combine(T seed, N... an)
{
	T args[] = { an... };
	// Init.
	T result = seed;
	// Combine.
	for (auto a : args)
	{
		result ^= a + masked_max<T>(3772387269305686495)
			+ (result << (30 * 2 / sizeof(T)))
			+ (result >> (13 * 2 / sizeof(T)));
	}
	// Mix.
	result ^= (result >> (16 * 2 / sizeof(T)));
	result *= masked_max<T>(448100074733706);
	result ^= (result << (8 * 2 / sizeof(T)));
	result *= masked_max<T>(190056597654806);
	result ^= (result >> (11 * 2 / sizeof(T)));
	return result;
}

// Backfill to_string as it's missing on some runtimes.
inline ::std::string to_string_10(::std::size_t value)
{
	::std::string result;
	result.reserve(::std::numeric_limits<::std::size_t>::max_digits10);
	do
	{
		result += '0' + (value % 10);
		value /= 10;
	}
	while (value > 0);
	::std::reverse(result.begin(), result.end());
	return result;
}

}}} // namespace bfg::versioned::detail

namespace bfg { namespace versioned {
struct from_chars_result
{
	const char * ptr;
	::std::errc ec;

	constexpr explicit operator bool() const noexcept
	{
		return ec == ::std::errc { };
	}

	friend constexpr bool operator==(
		const from_chars_result & a, const from_chars_result & b)
	{
		return (a.ptr == b.ptr) && (a.ec == b.ec);
	}

#if defined(__cpp_lib_to_chars) && (__cpp_lib_to_chars >= 201611L)
	constexpr operator std::from_chars_result() { return { ptr, ec }; }
	constexpr from_chars_result & operator=(
		const std::from_chars_result & other) noexcept
	{
		return *this = from_chars_result { other.ptr, other.ec };
	}
#endif
};
}} // namespace bfg::versioned

// tag::version_core-sym[]
namespace bfg { namespace versioned {
template <class Number, ::std::size_t Count = 3>
class version_core
{
	public:
	// types
	using element_t = Number;
	static constexpr ::std::size_t element_c = Count;

	// construction, destruction, and assignment
	version_core() = default;
	version_core(version_core &&) = default;
	version_core(const version_core &) = default;
	version_core & operator=(const version_core &) = default;
	template <class... I>
	version_core(element_t a0, I... an);

	// observers
	element_t & at(::std::size_t i);
	const element_t & at(::std::size_t i) const;

	private:
	static_assert(::std::is_integral<Number>::value,
		"Version element type is not an integral type."); // exposition only
	static_assert(Count > 0,
		"Version must contain at least one part."); // exposition only

	element_t number[element_c] = { }; // exposition only
	// end::version_core-sym[]

	template <class N, ::std::size_t C>
	friend from_chars_result from_chars(
		const char * first, const char * last, version_core<N, C> & value);

	template <class N, ::std::size_t C>
	friend ::std::string to_string(const version_core<N, C> & value);

	template <class N0, ::std::size_t C0, class N1, ::std::size_t C1>
	friend int compare(
		const version_core<N0, C0> & a, const version_core<N1, C1> & b);

	template <class N0, ::std::size_t C0, class N1, ::std::size_t C1>
	friend bool operator==(
		const version_core<N0, C0> & a, const version_core<N1, C1> & b);

	template <class N0, ::std::size_t C0, class N1, ::std::size_t C1>
	friend bool operator<(
		const version_core<N0, C0> & a, const version_core<N1, C1> & b);

	template <class N, ::std::size_t C>
	friend ::std::size_t hash(const version_core<N, C> & value);
	// tag::version_core-sym[]
};
}} // namespace bfg::versioned
// end::version_core-sym[]

namespace bfg { namespace versioned {
template <class N, ::std::size_t C>
template <class... I>
version_core<N, C>::version_core(element_t a0, I... an)
{
	element_t args[] = { a0, an... };
	::std::size_t i = 0;
	for (auto a : args) number[i++] = a;
}

template <class N, ::std::size_t C>
typename version_core<N, C>::element_t & version_core<N, C>::at(::std::size_t i)
{
	if (i >= element_c)
		throw ::std::out_of_range(
			"No such component: " + detail::to_string_10(i));
	return number[i];
}

template <class N, ::std::size_t C>
const typename version_core<N, C>::element_t & version_core<N, C>::at(
	::std::size_t i) const
{
	if (i >= element_c)
		throw ::std::out_of_range(
			"No such component: " + detail::to_string_10(i));
	return number[i];
}

template <class N, ::std::size_t C>
from_chars_result from_chars(
	const char * first, const char * last, version_core<N, C> & value)
{
	if (first == last || !first || !last)
		return from_chars_result { first, ::std::errc::invalid_argument };

	from_chars_result result { first, ::std::errc(0) };
	typename version_core<N, C>::element_t
		number[version_core<N, C>::element_c] { };

	for (::std::size_t i = 0; i < version_core<N, C>::element_c; ++i)
	{
		result.ptr = detail::from_chars_10(result.ptr, last, number[i]);
		if (result.ptr == last || *result.ptr != '.') break;
		result.ptr += 1;
	}

	for (::std::size_t i = 0; i < version_core<N, C>::element_c; ++i)
		value.number[i] = number[i];

	return result;
}

template <class N, ::std::size_t C>
::std::string to_string(const version_core<N, C> & value)
{
	::std::string result;
	for (::std::size_t i = version_core<N, C>::element_c; i > 0; --i)
	{
		::std::size_t n = i - 1;
		if (n == 0)
			result = detail::to_string_10(value.number[n]) + result;
		else if (value.number[n] > 0)
			result = "." + detail::to_string_10(value.number[n]) + result;
	}
	return result;
}

template <class N0, ::std::size_t C0, class N1, ::std::size_t C1>
int compare(const version_core<N0, C0> & a, const version_core<N1, C1> & b)
{
	for (::std::size_t i = 0; i < version_core<N0, C0>::element_c
		&& i < version_core<N1, C1>::element_c;
		++i)
	{
		auto c = a.number[i]
			- static_cast<typename version_core<N0, C0>::element_t>(
				b.number[i]);
		if (c != 0) return int(c);
	}
	return int(version_core<N0, C0>::element_c)
		- int(version_core<N1, C1>::element_c);
}

template <class N0, ::std::size_t C0, class N1, ::std::size_t C1>
bool operator==(const version_core<N0, C0> & a, const version_core<N1, C1> & b)
{
	return compare(a, b) == 0;
}

template <class N0, ::std::size_t C0, class N1, ::std::size_t C1>
bool operator<(const version_core<N0, C0> & a, const version_core<N1, C1> & b)
{
	return compare(a, b) < 0;
}

template <class N, ::std::size_t C>
::std::size_t hash(const version_core<N, C> & value)
{
	::std::hash<typename version_core<N, C>::element_t> h { };
	::std::size_t result = 0;
	for (auto n : value.number) result = detail::hash_combine(result, h(n));
	return result;
}

template <size_t I, class N, ::std::size_t C>
const typename version_core<N, C>::element_t & get(
	const version_core<N, C> & value)
{
	return value.at(I);
}
}} // namespace bfg::versioned

//
namespace std {
template <class N, ::std::size_t C>
struct hash<::bfg::versioned::version_core<N, C>>
{
	::std::size_t operator()(
		const ::bfg::versioned::version_core<N, C> & value) const noexcept
	{
		return ::bfg::versioned::hash(value);
	}
};

#if defined(__clang__)
#	pragma clang diagnostic push
#	pragma clang diagnostic ignored "-Wmismatched-tags"
#endif

template <class N, ::std::size_t C>
struct tuple_size<::bfg::versioned::version_core<N, C>>
	: integral_constant<::std::size_t,
		  ::bfg::versioned::version_core<N, C>::element_c>
{ };

template <size_t I, class N, ::std::size_t C>
struct tuple_element<I, ::bfg::versioned::version_core<N, C>>
{
	using type = const typename ::bfg::versioned::version_core<N, C>::element_t;
};

#if defined(__clang__)
#	pragma clang diagnostic pop
#endif
} // namespace std

// tag::version_tag-syn[]
namespace bfg { namespace versioned {
class version_tag
{
	public:
	// types
	using element_t = ::std::string;
	using range_element_t = ::std::tuple<const char *, const char *>;

	// construction, destruction, and assignment
	version_tag() = default;
	version_tag(version_tag &&) = default;
	version_tag(const version_tag &) = default;
	version_tag & operator=(const version_tag &) = default;
	template <class... S>
	version_tag(const char * a0, S... an);
	template <class... S>
	version_tag(const ::std::string & a0, S... an);

	// observers
	element_t at(::std::size_t i) const;
	range_element_t range_at(::std::size_t i) const;
	bool empty() const;
	::std::size_t size() const;
	// end::version_tag-syn[]

	private:
	// The info contains the part that is parsed from any original data.
	::std::string info_;
	// The parts hold the index to the end of each parsed segment.
	::std::vector<::std::size_t> parts_;

	friend from_chars_result from_chars(
		const char * first, const char * last, version_tag & value);

	friend ::std::string to_string(const version_tag & value);

	friend int compare(const version_tag & a, const version_tag & b);

	friend bool operator==(const version_tag & a, const version_tag & b);
	friend bool operator<(const version_tag & a, const version_tag & b);

	friend ::std::size_t hash(const version_tag & value);
	// tag::version_tag-syn[]
};
}} // namespace bfg::versioned
// end::version_tag-syn[]

namespace bfg { namespace versioned {
template <class... S>
version_tag::version_tag(const char * a0, S... an)
{
	info_ += a0;
	parts_.emplace_back(info_.size());
	const char * args[] = { an... };
	for (auto a : args)
	{
		info_ += ".";
		info_ += a;
		parts_.emplace_back(info_.size());
	}
}

template <class... S>
version_tag::version_tag(const ::std::string & a0, S... an)
{
	info_ += a0;
	parts_.emplace_back(info_.size());
	const char * args[] = { an... };
	for (auto a : args)
	{
		info_ += ".";
		info_ += a;
		parts_.emplace_back(info_.size());
	}
}

inline version_tag::element_t version_tag::at(::std::size_t i) const
{
	auto r = this->range_at(i);
	return ::std::string(::std::get<0>(r), ::std::get<1>(r));
}

inline version_tag::range_element_t version_tag::range_at(::std::size_t i) const
{
	if (i >= parts_.size())
		throw ::std::out_of_range(
			"No such component: " + detail::to_string_10(i));
	return ::std::make_tuple(info_.c_str() + (i == 0 ? 0 : parts_[i - 1] + 1),
		info_.c_str() + parts_[i]);
}

inline bool version_tag::empty() const { return info_.empty(); }

inline ::std::size_t version_tag::size() const { return parts_.size(); }

inline from_chars_result from_chars(
	const char * first, const char * last, version_tag & value)
{
	if (first == last)
		return from_chars_result { first, ::std::errc::invalid_argument };

	from_chars_result result { };
	::std::vector<::std::size_t> parts;

	for (auto begin = first; begin < last;)
	{
		auto end = ::std::find_if(begin, last,
			[](char c) { return c == '.' || !detail::is_ident(c); });
		if (end == first)
		{
			// No valid info parts found. Indicate an error, as nothing is
			// not an allowed input.
			result.ptr = end;
			result.ec = ::std::errc::invalid_argument;
			break;
		}
		else if (end == last || *end == '.')
		{
			// Found the EOS or a part separator. We have a part. And may
			// have more.
			parts.emplace_back(end - first);
			begin = end + 1;
		}
		else if (!detail::is_ident(*end))
		{
			// We parsed a valid part, but reached the end of the valid
			// characters.
			parts.emplace_back(end - first);
			break;
		}
		else
		{
			// Should never happen. But check, and report an error if it
			// does.
			result.ptr = nullptr;
			result.ec = ::std::errc::invalid_argument;
			break;
		}
	}

	if (result.ec == ::std::errc { })
	{
		result.ptr = first + parts.back();
		value.info_ = ::std::string(first, result.ptr);
		value.parts_ = parts;
	}
	return result;
}

inline ::std::string to_string(const version_tag & value)
{
	return value.info_;
}

inline int compare(const version_tag & a, const version_tag & b)
{
	if (!a.empty() && b.empty()) return -1;
	if (a.empty() && !b.empty()) return 1;
	for (::std::size_t i = 0; i < a.size() && i < b.size(); ++i)
	{
		auto a_chars = a.range_at(i);
		auto b_chars = b.range_at(i);
		auto c
			= detail::chars_cmp(::std::get<0>(a_chars), ::std::get<1>(a_chars),
				::std::get<0>(b_chars), ::std::get<1>(b_chars));
		if (c != 0) return c;
	}
	return int(a.size()) - int(b.size());
}

inline bool operator==(const version_tag & a, const version_tag & b)
{
	return compare(a, b) == 0;
}

inline bool operator<(const version_tag & a, const version_tag & b)
{
	return compare(a, b) < 0;
}

inline ::std::size_t hash(const version_tag & value)
{
	return ::std::hash<decltype(value.info_)> { }(value.info_);
}
}} // namespace bfg::versioned

namespace std {
template <>
struct hash<::bfg::versioned::version_tag>
{
	::std::size_t operator()(
		const ::bfg::versioned::version_tag & value) const noexcept
	{
		return ::bfg::versioned::hash(value);
	}
};
} // namespace std

// tag::prerelease_version-syn[]
namespace bfg { namespace versioned {
template <class Number>
class prerelease_version : public version_tag
{
	public:
	// types
	using number_element_t = Number;

	// construction, destruction, and assignment
	using version_tag::version_tag;

	// observers
	number_element_t number_at(::std::size_t i) const;
	bool is_number_at(::std::size_t i) const;
	// end::prerelease_version-syn[]

	private:
	bool is_number_at(::std::size_t i, number_element_t & n) const
	{
		auto r = this->range_at(i);
		const char * e
			= detail::from_chars_10(::std::get<0>(r), ::std::get<1>(r), n);
		return (e == ::std::get<1>(r));
	}

	template <class N0, class N1>
	friend int compare(
		const prerelease_version<N0> & a, const prerelease_version<N1> & b);

	template <class N0, class N1>
	friend bool operator==(
		const prerelease_version<N0> & a, const prerelease_version<N1> & b);

	template <class N0, class N1>
	friend bool operator<(
		const prerelease_version<N0> & a, const prerelease_version<N1> & b);

	template <class N>
	::std::size_t hash(const prerelease_version<N> & value);
	// tag::prerelease_version-syn[]
};
}} // namespace bfg::versioned
// end::prerelease_version-syn[]

namespace bfg { namespace versioned {
template <class N>
typename prerelease_version<N>::number_element_t prerelease_version<
	N>::number_at(::std::size_t i) const
{
	number_element_t n { };
	if (!is_number_at(i, n))
		throw ::std::invalid_argument("Element is not a number.");
	return n;
}

template <class N>
bool prerelease_version<N>::is_number_at(::std::size_t i) const
{
	number_element_t n { };
	return is_number_at(i, n);
}

template <class N0, class N1>
int compare(const prerelease_version<N0> & a, const prerelease_version<N1> & b)
{
	if (!a.empty() && b.empty()) return -1;
	if (a.empty() && !b.empty()) return 1;
	for (::std::size_t i = 0; i < a.size() && i < b.size(); ++i)
	{
		typename prerelease_version<N0>::number_element_t a_n { };
		bool a_is_num = a.is_number_at(i, a_n);
		typename prerelease_version<N1>::number_element_t b_n { };
		bool b_is_num = b.is_number_at(i, b_n);
		if (a_is_num && b_is_num)
		{
			auto b_e = static_cast<
				typename prerelease_version<N0>::number_element_t>(b_n);
			if (a_n < b_e) return -1;
			if (b_e < a_n) return 1;
		}
		else if (a_is_num)
			return -1;
		else if (b_is_num)
			return 1;
		else
		{
			auto a_chars = a.range_at(i);
			auto b_chars = b.range_at(i);
			auto c = detail::chars_cmp(::std::get<0>(a_chars),
				::std::get<1>(a_chars), ::std::get<0>(b_chars),
				::std::get<1>(b_chars));
			if (c != 0) return c;
		}
	}
	return int(a.size()) - int(b.size());
}

template <class N0, class N1>
bool operator==(
	const prerelease_version<N0> & a, const prerelease_version<N1> & b)
{
	return compare(a, b) == 0;
}

template <class N0, class N1>
bool operator<(
	const prerelease_version<N0> & a, const prerelease_version<N1> & b)
{
	return compare(a, b) < 0;
}

template <class N>
::std::size_t hash(const prerelease_version<N> & value)
{
	return hash(static_cast<const version_tag &>(value));
}
}} // namespace bfg::versioned

namespace std {
template <class N>
struct hash<::bfg::versioned::prerelease_version<N>>
{
	::std::size_t operator()(
		const ::bfg::versioned::prerelease_version<N> & value) const noexcept
	{
		return ::bfg::versioned::hash(value);
	}
};
} // namespace std

// tag::build_metadata-syn[]
namespace bfg { namespace versioned {
class build_metadata : public version_tag
{
	public:
	// construction, destruction, and assignment
	using version_tag::version_tag;
};
}} // namespace bfg::versioned
// end::build_metadata-syn[]

// tag::semver-syn[]
namespace bfg { namespace versioned {
template <class Number = int,
	class Prerelease = prerelease_version<Number>,
	class Build = build_metadata>
class semver
{
	public:
	// types
	using version_t = version_core<Number, 3>;
	using prerelease_t = Prerelease;
	using build_t = Build;

	// construction, destruction, and assignment
	semver() = default;
	semver(semver &&) = default;
	semver(const semver &) = default;
	semver(version_t, prerelease_t = { }, build_t = { });
	semver & operator=(semver &&) = default;

	// observers
	const version_t & version() const;
	version_t & version();
	const prerelease_t & prerelease() const;
	prerelease_t & prerelease();
	const build_t & build() const;
	build_t & build();
	typename version_t::element_t major() const;
	typename version_t::element_t minor() const;
	typename version_t::element_t patch() const;

	private:
	version_t core_; // exposition only
	prerelease_t pre_; // exposition only
	build_t build_; // exposition only
	// end::semver-syn[]

	template <class N, class P, class B>
	friend from_chars_result from_chars(
		const char * first, const char * last, semver<N, P, B> & value);

	template <class N, class P, class B>
	friend ::std::string to_string(const semver<N, P, B> & value);

	template <class N0, class P0, class B0, class N1, class P1, class B1>
	friend int compare(
		const semver<N0, P0, B0> & a, const semver<N1, P1, B1> & b);

	template <class N0, class P0, class B0, class N1, class P1, class B1>
	friend bool operator==(
		const semver<N0, P0, B0> & a, const semver<N1, P1, B1> & b);

	template <class N0, class P0, class B0, class N1, class P1, class B1>
	friend bool operator<(
		const semver<N0, P0, B0> & a, const semver<N1, P1, B1> & b);

	template <class N, class P, class B>
	friend ::std::size_t hash(const semver<N, P, B> & value);
	// tag::semver-syn[]
};
}} // namespace bfg::versioned
// end::semver-syn[]

namespace bfg { namespace versioned {

template <class N, class P, class B>
semver<N, P, B>::semver(version_t v, prerelease_t p, build_t b)
	: core_(v)
	, pre_(p)
	, build_(b)
{ }

template <class N, class P, class B>
const typename semver<N, P, B>::version_t & semver<N, P, B>::version() const
{
	return core_;
}

template <class N, class P, class B>
typename semver<N, P, B>::version_t & semver<N, P, B>::version()
{
	return core_;
}

template <class N, class P, class B>
const typename semver<N, P, B>::prerelease_t & semver<N, P, B>::prerelease()
	const
{
	return pre_;
}

template <class N, class P, class B>
typename semver<N, P, B>::prerelease_t & semver<N, P, B>::prerelease()
{
	return pre_;
}

template <class N, class P, class B>
const typename semver<N, P, B>::build_t & semver<N, P, B>::build() const
{
	return build_;
}

template <class N, class P, class B>
typename semver<N, P, B>::build_t & semver<N, P, B>::build()
{
	return build_;
}

template <class N, class P, class B>
typename semver<N, P, B>::version_t::element_t semver<N, P, B>::major() const
{
	return core_.at(0);
}

template <class N, class P, class B>
typename semver<N, P, B>::version_t::element_t semver<N, P, B>::minor() const
{
	return core_.at(1);
}

template <class N, class P, class B>
typename semver<N, P, B>::version_t::element_t semver<N, P, B>::patch() const
{
	return core_.at(2);
}

template <class N, class P, class B>
from_chars_result from_chars(
	const char * first, const char * last, semver<N, P, B> & value)
{
	if (first == last)
		return from_chars_result { first, ::std::errc::invalid_argument };

	from_chars_result result { };

	typename semver<N, P, B>::version_t v;
	{
		result = from_chars(first, last, v);
		if (result.ec != ::std::errc { }) return result;
		first = result.ptr;
	}

	typename semver<N, P, B>::prerelease_t p;
	if (first != last && *first == '-')
	{
		result = from_chars(first + 1, last, p);
		if (result.ec != ::std::errc { }) return result;
		first = result.ptr;
	}

	typename semver<N, P, B>::build_t b;
	if (first != last && *first == '+')
	{
		result = from_chars(first + 1, last, b);
		if (result.ec != ::std::errc { }) return result;
		first = result.ptr;
	}

	value.core_ = v;
	value.pre_ = p;
	value.build_ = b;
	return result;
}

template <class N, class P, class B>
::std::string to_string(const semver<N, P, B> & value)
{
	::std::string result = to_string(value.core_);
	if (!value.pre_.empty()) result += "-" + to_string(value.pre_);
	if (!value.build_.empty()) result += "+" + to_string(value.build_);
	return result;
}

template <class N0, class P0, class B0, class N1, class P1, class B1>
int compare(const semver<N0, P0, B0> & a, const semver<N1, P1, B1> & b)
{
	auto x = compare(a.core_, b.core_);
	if (x == 0) return compare(a.pre_, b.pre_);
	return x;
}

template <class N0, class P0, class B0, class N1, class P1, class B1>
bool operator==(const semver<N0, P0, B0> & a, const semver<N1, P1, B1> & b)
{
	return compare(a, b) == 0;
}

template <class N0, class P0, class B0, class N1, class P1, class B1>
bool operator<(const semver<N0, P0, B0> & a, const semver<N1, P1, B1> & b)
{
	return compare(a, b) < 0;
}

template <class N, class P, class B>
::std::size_t hash(const semver<N, P, B> & value)
{
	return detail::hash_combine(::bfg::versioned::hash(value.core_),
		::bfg::versioned::hash(value.pre_),
		::bfg::versioned::hash(value.build_));
}

template <::std::size_t I, class N, class P, class B>
typename ::std::enable_if<I == 0,
	const typename ::std::tuple_element<I, semver<N, P, B>>::type>::type &
	get(const semver<N, P, B> & value)
{
	return value.version();
}

template <::std::size_t I, class N, class P, class B>
typename ::std::enable_if<I == 1,
	const typename ::std::tuple_element<I, semver<N, P, B>>::type>::type &
	get(const semver<N, P, B> & value)
{
	return value.prerelease();
}

template <::std::size_t I, class N, class P, class B>
typename ::std::enable_if<I == 2,
	const typename ::std::tuple_element<I, semver<N, P, B>>::type>::type &
	get(const semver<N, P, B> & value)
{
	return value.build();
}

}} // namespace bfg::versioned

namespace std {

template <class N, class P, class B>
struct hash<::bfg::versioned::semver<N, P, B>>
{
	::std::size_t operator()(
		const ::bfg::versioned::semver<N, P, B> & value) const noexcept
	{
		return ::bfg::versioned::hash(value);
	}
};

#if defined(__clang__)
#	pragma clang diagnostic push
#	pragma clang diagnostic ignored "-Wmismatched-tags"
#endif

template <class N, class P, class B>
struct tuple_size<::bfg::versioned::semver<N, P, B>>
	: integral_constant<size_t, 3>
{ };

template <size_t I, class N, class P, class B>
struct tuple_element<I, ::bfg::versioned::semver<N, P, B>>
{
	using version_t = typename ::bfg::versioned::semver<N, P, B>::version_t;
	using prerelease_t =
		typename ::bfg::versioned::semver<N, P, B>::prerelease_t;
	using build_t = typename ::bfg::versioned::semver<N, P, B>::build_t;
	using type = const typename conditional<I == 0,
		version_t,
		typename conditional<I == 1, prerelease_t, build_t>::type>::type;
};

#if defined(__clang__)
#	pragma clang diagnostic pop
#endif

} // namespace std

#endif
