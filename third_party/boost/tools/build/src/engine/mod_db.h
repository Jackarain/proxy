/*
Copyright 2024-2026 René Ferdinand Rivera Morell
Distributed under the Boost Software License, Version 1.0.
(See accompanying file LICENSE.txt or https://www.bfgroup.xyz/b2/LICENSE.txt)
*/

#ifndef B2_MOD_DB_H
#define B2_MOD_DB_H

#include "config.h"

#include "bind.h"
#include "lists.h"
#include "value.h"

#include <cstddef>
#include <map>
#include <string>

/* tag::reference[]

= `db` module.

Classes and functions to manage structured data.

end::reference[] */

namespace b2 {

/* tag::reference[]

== `b2::property_db` (`property-db`)

Container for values structured as a tree with keys that are the tree path to
the value. Arrays and objects (named fields) are supported.

Values are indexed with a variable, i.e. a list of values, path. The path can
contain two kinds of position items: an array index or an object member. An
array index is `"[]" _n_`. Where the `"[]"` indicates that a zero based index
in the array, `_n_` follows. Anything else is treated as a member field name.

end::reference[] */
struct property_db : public object
{
	// Item access.
	void emplace(list_cref k, value_ref v);
	list_ref get(list_cref k) const;
	std::size_t size(list_cref k) const;

	// Write and read of data from files.
	bool write_file(value_ref filename, value_ref format);
	std::string dump(value_ref format);
	bool read_file(value_ref filename, value_ref format);
	bool parse(value_ref content, value_ref format);

	private:
	value_ref array_tag { "[]" };
	struct key_less
	{
		bool operator()(const list_ref & a, const list_ref & b) const
		{
			for (list_ref::size_type i = 0; i < a.size() && i < b.size(); ++i)
			{
				value_ref a_v = a[i];
				value_ref b_v = b[i];
				int c = a_v->compare(*b_v);
				if (c < 0) return true;
				if (c > 0) return false;
			}
			return a.size() < b.size();
		}
	};
	using db_type = std::map<list_ref, value_ref, key_less>;
	using size_type = std::map<list_ref, std::size_t, key_less>;
	db_type db;
	mutable size_type sizes;
	list_ref calc_db_key(list_cref k) const;
	bool write_file_json(value_ref filename);
	std::string dump_json();
	bool read_file_json(value_ref filename);
	template <typename Json>
	void build_db_from_json(const Json & j, b2::list_ref & path);
};

/* tag::reference[]

=== `b2::property_db::emplace`

====
[horizontal]
Jam:: `rule emplace ( key * : value )`
{CPP}:: `void emplace(list_cref k, value_ref v);`
====

Set, or add, element at path `key` with `value`.

end::reference[] */

/* tag::reference[]

=== `b2::property_db::get`

====
[horizontal]
Jam:: `rule get ( key * )`
{CPP}:: `list_ref get(list_cref k) const;`
====

Get element at path `key`. If the key is not found returns an empty list.

end::reference[] */

/* tag::reference[]

=== `b2::property_db::size`

====
[horizontal]
Jam:: `rule size ( key * )`
{CPP}:: `std::size_t size(list_cref k);`
====

Get the count of direct children at path `key`. If the key is not found returns
zero (`0`).

end::reference[] */

/* tag::reference[]

=== `b2::property_db::write_file`

====
[horizontal]
Jam:: `rule write-file ( filename : format ? )`
{CPP}:: `void write_file(value_ref filename, value_ref format);`
====

Writes out a representation of the data to the given `filename` file formatted
as `format`. Supported formats are: `JSON`.

end::reference[] */

/* tag::reference[]

=== `b2::property_db::dump`

====
[horizontal]
Jam:: `rule dump ( format ? )`
{CPP}:: `std::string dump(value_ref format);`
====

Writes out a representation of the data as a string formatted as `format`.
Supported formats are: `JSON`.

end::reference[] */

/* tag::reference[]

=== `b2::property_db::dump`

====
[horizontal]
Jam:: `rule dump ( format ? )`
{CPP}:: `std::string dump(value_ref format);`
====

Writes out a representation of the data as a string formatted as `format`.
Supported formats are: `JSON`.

end::reference[] */

/* tag::reference[]

=== `b2::property_db::read_file`

====
[horizontal]
Jam:: `rule read-file ( filename : format ? )`
{CPP}:: `void read_file(value_ref filename, value_ref format);`
====

Reads from the given `filename` file formatted as `format` adding the data
to this database. Supported formats are: `JSON`. Returns a boolean result
on success. On failure the database may have partial data added to it.

end::reference[] */

/* tag::reference[]

=== `b2::property_db::parse`

====
[horizontal]
Jam:: `rule parse ( content : format ? )`
{CPP}:: `bool parse(value_ref content, value_ref format);`
====

Reads the given `content` data formatted as `format` adding the data
to this database. Supported formats are: `JSON`. Returns a boolean result
on success. On failure the database may have partial data added to it.

end::reference[] */

struct db_module : b2::bind::module_<db_module>
{
	const char * module_name = "db";
	static const char * init_code;

	template <class Binder>
	void def(Binder & binder)
	{
		binder.def_class("property-db", type_<property_db>())
			.def(init_<>())
			.def(&property_db::emplace, "emplace", ("key" * _n) | ("value" * _1))
			.def(&property_db::get, "get", ("key" * _n))
			.def(&property_db::size, "size", ("key" * _n))
			.def(&property_db::write_file, "write-file",
				("filename" * _1) | ("format" * _01))
			.def(&property_db::dump, "dump", ("format" * _01))
			.def(&property_db::read_file, "read-file",
				("filename" * _1) | ("format" * _01))
			.def(&property_db::parse, "parse",
				("content" * _1) | ("format" * _01));
		binder.eval(init_code);
		binder.loaded();
	}
};

} // namespace b2

#endif
