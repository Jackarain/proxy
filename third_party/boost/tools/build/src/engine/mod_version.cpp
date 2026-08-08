/*
Copyright 2019-2022 René Ferdinand Rivera Morell
Distributed under the Boost Software License, Version 1.0.
(See accompanying file LICENSE.txt or https://www.bfgroup.xyz/b2/LICENSE.txt)
*/

#include "mod_version.h"

#include "ext_bfgroup_versioned.hpp"
#include "value.h"

namespace b2 {

bool version_less(const std::vector<int> & lhs, const std::vector<int> & rhs)
{
	auto left = lhs.begin();
	auto left_end = lhs.end();
	auto right = rhs.begin();
	auto right_end = rhs.end();
	while (left != left_end || right != right_end)
	{
		int left_value = 0;
		int right_value = 0;
		if (left != left_end) left_value = *(left++);
		if (right != right_end) right_value = *(right++);
		if (left_value > right_value) return false;
		if (left_value < right_value) return true;
	}
	return false;
}

bool semver_verify(value_ref v)
{
	bfg::versioned::semver<> v_semver;
	auto v_string = v->as_string();
	auto v_first = v_string.cbegin();
	auto v_last = v_first + v_string.size();
	auto r = from_chars(v_first, v_last, v_semver);
	return static_cast<bool>(r) && (r.ptr == v_last);
}

list_ref semver_resolve(std::tuple<value_ref, value_ref> ab)
{
	list_ref result;
	bfg::versioned::semver<> a_semver;
	bfg::versioned::semver<> b_semver;
	auto a_string = std::get<0>(ab)->as_string();
	auto b_string = std::get<1>(ab)->as_string();
	auto a_result = from_chars(
		a_string.cbegin(), a_string.cbegin() + a_string.size(), a_semver);
	auto b_result = from_chars(
		b_string.cbegin(), b_string.cbegin() + b_string.size(), b_semver);
	if (a_result && b_result && a_semver.major() == b_semver.major()
		&& (a_semver < b_semver || a_semver == b_semver))
		result.push_back(std::get<1>(ab));
	return result;
}

} // namespace b2
