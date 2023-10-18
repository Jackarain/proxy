//
// Copyright (C) 2019 Jack.
//
// Author: jack
// Email:  jack.wgm at gmail dot com
//

#ifndef INCLUDE__2023_10_18__SCOPED_EXIT_HPP
#define INCLUDE__2023_10_18__SCOPED_EXIT_HPP


#include <type_traits>

#ifdef __cpp_lib_concepts
#	include <concepts>
#endif


template <typename T
#if !defined(__cpp_lib_concepts)
	, typename = typename std::enable_if<std::is_invocable_v<T>>::type
#endif
>
#ifdef __cpp_lib_concepts
	requires std::invocable<T>
#endif
class scoped_exit
{
	scoped_exit() = delete;
	scoped_exit(scoped_exit const&) = delete;
	scoped_exit& operator=(scoped_exit const&) = delete;
	scoped_exit(scoped_exit&&) = delete;
	scoped_exit& operator=(scoped_exit&&) = delete;

public:
	explicit scoped_exit(T&& f)
		: f_(std::move(f))
	{}

	explicit scoped_exit(const T& f)
		: f_(f)
	{}

	~scoped_exit()
	{
		if (stop_token_)
			return;

		f_();
	}

	inline void operator()()
	{
		if (stop_token_)
			return;

		stop_token_ = true;

		f_();
	}

	inline void cancel()
	{
		stop_token_ = true;
	}

private:
	T f_;
	bool stop_token_{ false };
};

#endif // INCLUDE__2023_10_18__SCOPED_EXIT_HPP
