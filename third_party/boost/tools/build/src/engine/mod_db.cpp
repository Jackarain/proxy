/*
Copyright 2024 René Ferdinand Rivera Morell
Distributed under the Boost Software License, Version 1.0.
(See accompanying file LICENSE.txt or https://www.bfgroup.xyz/b2/LICENSE.txt)
*/

#include "mod_db.h"

#include "ext_nlohmann_json.hpp"
#include "lists.h"
#include "value.h"

#include <cstddef>
#include <cstdio>
#include <cstdlib>
#include <iostream>
#include <string>
#include <unordered_set>

namespace {
void echo(const b2::list_ref & v)
{
#if false
	std::cerr << ">";
	for (auto e : v) std::cerr << " " << e->to_string()->str();
	std::cerr << "\n";
#endif
}
} // namespace

b2::list_ref b2::property_db::calc_db_key(b2::list_cref k) const
{
	list_ref nk;
	for (list_cref::size_type i = 0; i < k.size(); ++i)
	{
		value_ref a = k[i];
		if (a == array_tag)
		{
			nk.push_back(value_ptr(array_tag));
			i += 1;
			if (i < k.size())
			{
				// Normalize array indices to number type for proper ordering.
				a = k[i];
				if (a->get_type() != value::type::number)
				{
					double index = list_cref::size_type(std::atof(a->str()));
					a = value::make(index);
				}
				nk.push_back(value_ptr(a));
			}
		}
		else
			nk.push_back(value_ptr(a));
	}
	return nk;
}

void b2::property_db::emplace(list_cref k, value_ref v)
{
	list_ref nk = calc_db_key(k);
	db[nk] = v;
	// Invalidate the count of the parents as we possibly inserted new values.
	while (!nk.empty())
	{
		nk.pop_back();
		sizes.erase(nk);
	}
}

b2::list_ref b2::property_db::get(list_cref k) const
{
	list_ref nk = calc_db_key(k);
	auto nk_size = nk.size();
	auto i = db.lower_bound(nk);
	// Directly reference a key/value.
	if (i->first.size() == nk_size) return list_ref { i->second };
	// Reference a parent key, return unique children.
	list_ref result;
	value_ref last;
	while (i != db.end() && i->first.starts_with(nk))
	{
		if (i->first.size() > nk_size && i->first[nk_size] != last)
		{
			last = i->first[nk_size];
			result.push_back(last->to_string());
		}
		++i;
	}
	return result;
}

std::size_t b2::property_db::size(list_cref k) const
{
	list_ref nk = calc_db_key(k);
	auto i = sizes.find(nk);
	if (i == sizes.end())
	{
		// Check if we have an item we haven´t computed the count for yet.
		// We count the unique set of child keys as the count. This covers
		// arrays or objects as the root.
		std::unordered_set<value_ref, value_ref::hash_function> child_set;
		for (auto child_i = db.lower_bound(nk);
			child_i != db.end() && child_i->first.starts_with(nk); ++child_i)
		{
			if (child_i->first[0]->equal_to(*array_tag)
				&& child_i->first.size() >= 2)
			{
				auto array_item = array_tag + child_i->first[1]->to_string();
				child_set.emplace(array_item);
			}
			else
			{
				child_set.emplace(child_i->first[0]);
			}
		}
		// Cache the count and return it.
		sizes[nk] = child_set.size();
		return child_set.size();
	}
	else
	{
		return i->second;
	}
}

bool b2::property_db::write_file(value_ref filename, value_ref format)
{
	if (!format.has_value())
	{
		format = b2::value::make("json");
	}
	if (format == "json") return write_file_json(filename);
	return false;
}

std::string b2::property_db::dump(value_ref format)
{
	if (!format.has_value())
	{
		format = b2::value::make("json");
	}
	if (format == "json") return dump_json();
	return "";
}

bool b2::property_db::read_file(value_ref filename, value_ref format)
{
	if (!format.has_value())
	{
		format = b2::value::make("json");
	}
	if (format == "json") return read_file_json(filename);
	return false;
}

namespace {
template <class T>
void build_json_from_db(const T & db, nlohmann::json & out)
{
	b2::value_ref array_tag { "[]" };
	for (auto & item : db)
	{
		auto & k = item.first;
		auto & v = item.second;
		if (v->get_type() != b2::value::type::number
			&& v->get_type() != b2::value::type::string)
			continue;
		nlohmann::json * current = &out;
		for (b2::list_cref::size_type i = 0; i < k.size(); ++i)
		{
			b2::value_ref a = k[i];
			if (a == array_tag)
			{
				if (++i >= k.size()) break;
				auto index = b2::list_cref::size_type(k[i]->as_number());
				if (current->is_null()) *current = nlohmann::json::array();
				if (!current->is_array()) break;
				while (current->size() <= nlohmann::json::size_type(index))
					current->push_back(nlohmann::json());
				current = &(*current)[index];
			}
			else
			{
				if (current->is_null()) *current = nlohmann::json::object();
				current = &(*current)[a->str()];
			}
		}
		if (v->get_type() == b2::value::type::number)
			*current = nlohmann::json(v->as_number());
		else if (v->get_type() == b2::value::type::string)
			*current = nlohmann::json(v->str());
	}
}
} // namespace

bool b2::property_db::write_file_json(value_ref filename)
{
	nlohmann::json out;
	build_json_from_db(db, out);
	FILE * file = std::fopen(filename->str(), "w");
	if (!file) return false;
	bool result = false;
	try
	{
		auto data = out.dump(0);
		result = std::fwrite(data.c_str(), data.size(), 1, file) == 1;
	}
	catch (const std::exception &)
	{ }
	std::fclose(file);
	return result;
}

std::string b2::property_db::dump_json()
{
	nlohmann::json out;
	build_json_from_db(db, out);
	try
	{
		return out.dump(-1);
	}
	catch (const std::exception &)
	{ }
	return "";
}

namespace b2 {
template <>
void property_db::build_db_from_json<nlohmann::json>(
	const nlohmann::json & j, b2::list_ref & path)
{
	if (j.is_array())
	{
		path.push_back(value_ptr(array_tag));
		echo(path);
		sizes[path] = j.size();
		path.push_back(0.0);
		for (std::size_t idx = 0; idx < j.size(); ++idx)
		{
			path[path.length() - 1] = b2::value::make(double(idx));
			build_db_from_json(j.at(idx), path);
		}
		path.pop_back();
		path.pop_back();
	}
	else if (j.is_boolean())
	{
		echo(path);
		if (j.get<bool>())
		{
			sizes[path] = 1;
			db[path] = b2::value::make("true");
		}
		else
			sizes[path] = 0;
	}
	else if (j.is_null())
	{
		echo(path);
		sizes[path] = 0;
	}
	else if (j.is_number())
	{
		echo(path);
		sizes[path] = 1;
		db[path] = b2::value::make(j.get<double>());
	}
	else if (j.is_object())
	{
		echo(path);
		sizes[path] = j.size();
		for (auto & e : j.items())
		{
			path.push_back(b2::value::make(e.key()));
			build_db_from_json(e.value(), path);
			path.pop_back();
		}
	}
	else if (j.is_string())
	{
		echo(path);
		sizes[path] = 1;
		db[path] = b2::value::make(j.get<std::string>());
	}
}
} // namespace b2

bool b2::property_db::read_file_json(value_ref filename)
{
	FILE * file = std::fopen(filename->str(), "r");
	if (!file) return false;
	nlohmann::json in = in.parse(file, nullptr, false);
	bool result = false;
	if (!in.is_discarded())
	{
		list_ref path;
		build_db_from_json(in, path);
		result = true;
	}
	std::fclose(file);
	return result;
}

bool b2::property_db::parse(value_ref content, value_ref format)
{
	nlohmann::json in = in.parse(std::string(content), nullptr, false);
	bool result = false;
	if (!in.is_discarded())
	{
		list_ref path;
		build_db_from_json(in, path);
		result = true;
	}
	return result;
}

const char * b2::db_module::init_code = R"jam(
rule __test__ ( )
{
	import assert ;
	import "class" : new ;

	local pdb = [ new property-db ] ;
	assert.result "null" : $(pdb).dump ;
	assert.result 0 : $(pdb).size ;
	$(pdb).emplace "[]" 1 "one" : 1 ;
	assert.result "[null,{\"one\":\"1\"}]" : $(pdb).dump "json" ;
	assert.result 1 : $(pdb).size ;
	assert.result 1 : $(pdb).size "[]" ;
	assert.result 1 : $(pdb).size "[]" 1 "one" ;
	$(pdb).emplace "[]" 0 "zero" "sub" : "qwerty" ;
	assert.result "[{\"zero\":{\"sub\":\"qwerty\"}},{\"one\":\"1\"}]" : $(pdb).dump "json" ;
	assert.result 2 : $(pdb).size "[]" ;
	assert.result 1 : $(pdb).size "[]" 0 "zero" ;
	assert.result 1 : $(pdb).size "[]" 0 "zero" "sub" ;
	assert.result 0 : $(pdb).size "[]" 0 "zero" "sub" "qwerty" ;
	pdb = [ new property-db ] ;
	$(pdb).parse "[{\"zero\":{\"sub\":\"qwerty\"}},{\"one\":\"1\"}]" : "json" ;
	assert.result "[{\"zero\":{\"sub\":\"qwerty\"}},{\"one\":\"1\"}]" : $(pdb).dump "json" ;
	assert.result 2 : $(pdb).size "[]" ;
	assert.result 1 : $(pdb).size "[]" 0 "zero" ;
	assert.result 1 : $(pdb).size "[]" 0 "zero" "sub" ;
	assert.result 0 : $(pdb).size "[]" 0 "zero" "sub" "qwerty" ;
	assert.result qwerty : $(pdb).get "[]" 0 "zero" "sub" ;
	assert.result 1 : $(pdb).get "[]" 1 "one" ;
	assert.result sub : $(pdb).get "[]" 0 "zero" ;
	assert.result 0.000000 1.000000 : $(pdb).get "[]" ;
}
)jam";
