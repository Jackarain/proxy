/* Proof-of-concept implementation of a rolling Bloom filter.
 * 
 * Copyright 2025 Joaquin M Lopez Munoz.
 * Distributed under the Boost Software License, Version 1.0.
 * (See accompanying file LICENSE_1_0.txt or copy at
 * http://www.boost.org/LICENSE_1_0.txt)
 *
 * See https://www.boost.org/libs/bloom for library home page.
 */

#include <boost/bloom.hpp>
#include <cassert>
#include <cmath>
#include <memory>
#include <vector>
#include <iostream>

/* Regular Bloom filters don't "forget", that is, the number of elements
 * in the filter keeps growing (and the FPR increasing) until the entire bit
 * array is reset. This is how a *rolling filter* can be devised that keeps
 * track of the last elements inserted only:
 * 
 *   - A so-called *window size* w and a number of windows n are specified.
 *   - n regular Bloom filters are kept simultaneously, one for each window.
 *   - We keep an index i to designate the i-th filter as the *active filter*.
 *   - Insertion of a new element is done in the active filter only.
 *   - Lookup queries all the filters.
 *   - Every w insertions, we bump i (mod n) to switch to a new active filter
 *     and clear it before continuing with the insertions.
 * 
 * It's not hard to see that this algorithm implements a filter that remembers
 * only the last s elements inserted, with w * (n-1) < s <= w * n. The
 * resulting FPR oscillates between 1 - (1 - sub_fpr)^(n-1) and 
 * 1 - (1 - sub_fpr)^n, where sub_fpr is the FPR of an individual filter
 * after w insertions.
 */

template<
  typename T, std::size_t K,
  typename Subfilter = boost::bloom::block<unsigned char,1>,
  std::size_t Stride = 0,
  typename Hash = boost::hash<T>,
  typename Allocator=std::allocator<unsigned char>
>
class rolling_filter
{
public:
  rolling_filter(std::size_t w_, std::size_t n, double max_fpr): 
    w(w_), fs(n) 
  {
    assert(w > 0);
    assert(n >= 2);
    assert(max_fpr >= 0.0 && max_fpr <= 1.0);

    /* Adjust the capacity of each individual filter so that
     * this->max_fpr() ~ max_fpr. 
     */

    auto sub_fpr = 1.0 - std::pow(1.0 - max_fpr, 1.0 / n);
    auto m = filter_type::capacity_for(w, sub_fpr);
    for(auto& f: fs) f.reset(m);
  }

  std::size_t min_size() const
  {
    /* Minimum size under *stationary* conditions: the actual size can be
     * smaller than this if we haven't yet inserted that many elements.
     */

    return w * (fs.size() - 1);
  }

  std::size_t max_size() const
  {
    return w * fs.size();
  }

  double min_fpr() const
  {
    double sub_fpr = filter_type::fpr_for(w, fs[0].capacity());
    return 1.0 - std::pow(1.0 - sub_fpr, (double)fs.size() - 1.0);
  }

  double max_fpr() const
  {
    double sub_fpr = filter_type::fpr_for(w, fs[0].capacity());
    return 1.0 - std::pow(1.0 - sub_fpr, (double)fs.size());
  }

  std::size_t capacity() const
  {
    return fs[0].capacity() * fs.size();
  }

  void insert(const T& x)
  {
    if(++count > w) {
      count = 1;
      if(++i >= fs.size()) i = 0;
      fs[i].clear();
    }
    fs[i].insert(x);
  }

  bool may_contain(const T& x) const
  {
    for(const auto& f: fs) {
      if(f.may_contain(x)) return true;
    }
    return false;
  }

private:
  using filter_type = boost::bloom::filter<
    T, K, Subfilter, Stride, Hash, Allocator
  >;
  using vector_type = std::vector<
    filter_type, 
    typename std::allocator_traits<Allocator>::
      template rebind_alloc<filter_type>
  >;

  std::size_t w;
  vector_type fs;
  std::size_t count = 0,
              i = 0;
};

int main()
{
  /* Construct a rolling filter with a size between
   * 9,000 and 10,000 elements.
   */

  const std::size_t window_size = 1000;
  const std::size_t num_windows = 10;
  const double      max_fpr = 0.01;

  rolling_filter<std::size_t, 5> rf(window_size, num_windows, max_fpr);
  std::cout << "rolling filter capacity: " << rf.capacity() << " bits\n";

  /* Run the filter through more than 10x the elements it can hold. */

  const std::size_t num_elements = rf.max_size() * 10 + window_size / 2;
  for(std::size_t i = 0 ; i < num_elements; ++i) rf.insert(i);

  /* Check the filter has actually forgotten the first 
   * num_elements - rf.max_size() elements.
   */

  std::size_t count = 0;
  for(std::size_t i = 0 ; i < num_elements - rf.max_size(); ++i) {
    count += rf.may_contain(i);
  }
  std::cout << "measured fpr: " 
            << (double)count / (num_elements - rf.max_size())
            << " (should be between " << rf.min_fpr() 
            << " and " << rf.max_fpr() << ")\n";

  /* The remaining elements must be mostly in the filter. */

  count = 0;
  for(std::size_t i = num_elements - rf.max_size() ; i < num_elements; ++i) {
    count += rf.may_contain(i);
  }
  std::cout << "elements found: " << count
            << " (must be between " << rf.min_size()
            << " and " << rf.max_size() << ")\n";
}
