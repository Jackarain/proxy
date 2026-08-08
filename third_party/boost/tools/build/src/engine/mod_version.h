/*
Copyright 2019-2022 René Ferdinand Rivera Morell
Distributed under the Boost Software License, Version 1.0.
(See accompanying file LICENSE.txt or https://www.bfgroup.xyz/b2/LICENSE.txt)
*/

#ifndef B2_MOD_VERSION_H
#define B2_MOD_VERSION_H

#include "config.h"

#include "bind.h"
#include "lists.h"
#include "value.h"

#include <vector>

namespace b2 {
/* tag::reference[]

[[b2.reference.modules.version]]
= `version` module.

end::reference[] */

/* tag::reference[]

== `b2::version_less`

====
[horizontal]
Jam:: `rule version-less ( lhs + : rhs + )`
{CPP}:: `bool version_less(const std::vector<int> & lhs, const std::vector<int>
& rhs);`
====

Returns `true` if the first version, `lhs`, is semantically less than the
second version, `rhs`.

end::reference[] */
bool version_less(const std::vector<int> & lhs, const std::vector<int> & rhs);

/* tag::reference[]

== `b2::version_verify`

====
[horizontal]
Jam:: `rule semver-verify ( semver )`
{CPP}:: `bool semver_verify(value_ref semver);`
====

Returns `true` if the given `semver` value is a fully valid semantic version.

end::reference[] */
bool semver_verify(value_ref semver);

list_ref semver_resolve(std::tuple<value_ref, value_ref> ab);

struct version_module : b2::bind::module_<version_module>
{
	const char * module_name = "version";

	template <class Binder>
	void def(Binder & binder)
	{
		binder.def(&b2::version_less, "version-less", "lhs" * _1n | "rhs" * _1n);
		binder.def(&b2::semver_verify, "semver-verify", "semver" * _1);
		binder.def(&b2::semver_resolve, "semver-resolve", "a" * _1 + "b" * _1);
	}
};
} // namespace b2

#endif
