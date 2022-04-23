//
// Copyright (C) 2019 Jack.
//
// Author: jack
// Email:  jack.wgm at gmail dot com
//

#pragma once

#include <type_traits>

template<class T>
class scoped_exit
{
public:
	explicit scoped_exit(T&& f) : f_(std::move(f)), dismiss_(false) {}
	explicit scoped_exit(const T& f) : f_(f), dismiss_(false) {}
	inline void dismiss() { dismiss_ = true; }
	~scoped_exit() { if (dismiss_) return; f_(); }

private:
	T f_;
	bool dismiss_;
};
