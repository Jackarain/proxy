/* Serialization of boost::bloom::filter.
 * 
 * Copyright 2025 Joaquin M Lopez Munoz.
 * Distributed under the Boost Software License, Version 1.0.
 * (See accompanying file LICENSE_1_0.txt or copy at
 * http://www.boost.org/LICENSE_1_0.txt)
 *
 * See https://www.boost.org/libs/bloom for library home page.
 */

#include <boost/bloom/filter.hpp>
#include <boost/bloom/multiblock.hpp>
#include <boost/core/detail/splitmix64.hpp>
#include <boost/uuid/uuid.hpp>
#include <cstdint>
#include <cstring>
#include <fstream>
#include <iostream>

/* emits a deterministic pseudorandom sequence of UUIDs */

struct uuid_generator
{
  boost::uuids::uuid operator()()
  {
    std::uint8_t    data[16];
    std::uint64_t x = rng();
    std::memcpy(&data[0], &x, sizeof(x));
    x = rng();
    std::memcpy(&data[8], &x, sizeof(x));

    return {data};
  }

  boost::detail::splitmix64 rng;
};

using filter = boost::bloom::filter<
  boost::uuids::uuid, 1, boost::bloom::multiblock<std::uint64_t, 8> >;

static constexpr std::size_t num_elements = 10000;

/* creates a filter with num_elements UUIDs */

filter create_filter()
{
  uuid_generator gen;
  filter         f(num_elements, 0.005);
  for(std::size_t i = 0; i < num_elements; ++i) f.insert(gen());
  return f;
}

void save_filter(const filter& f, const char* filename)
{
  std::ofstream out(filename, std::ios::binary | std::ios::trunc);
  std::size_t c=f.capacity();
  out.write(reinterpret_cast<const char*>(&c), sizeof(c)); /* save capacity (bits) */
  auto s = f.array();
  out.write(reinterpret_cast<const char*>(s.data()), s.size()); /* save array */
}

filter load_filter(const char* filename)
{
  std::ifstream in(filename, std::ios::binary);
  std::size_t c;
  in.read(reinterpret_cast<char*>(&c), sizeof(c));
  filter f(c);
  auto s = f.array();
  in.read(reinterpret_cast<char*>(s.data()), s.size()); /* load array */
  return f;
}

int main()
{
  static constexpr const char* filename = "filter.bin";

  auto f1 = create_filter();
  save_filter(f1, filename);
  auto f2 = load_filter(filename);

  if (f1 == f2) std::cout << "serialization correct\n";
  else          std::cout << "something went wrong\n";
}
