//////////////////////////////////////////////////////////////////////////////
//
// (C) Copyright Ion Gaztanaga 2025-2025. Distributed under the Boost
// Software License, Version 1.0. (See accompanying file
// LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//
// See http://www.boost.org/libs/container for documentation.
//
//////////////////////////////////////////////////////////////////////////////

#include <boost/core/lightweight_test.hpp>
#include <boost/container/pmr/global_resource.hpp>
#include <boost/container/pmr/memory_resource.hpp>
#include <boost/container/pmr/vector.hpp>
#include <boost/container/detail/operator_new_helpers.hpp>

#include <cstddef>
#include <cstring>

using namespace boost::container;

BOOST_CONSTEXPR_OR_CONST std::size_t max_align = dtl::alignment_of<dtl::max_align_t>::value;

// Helper to check alignment
bool is_aligned(void* ptr, std::size_t alignment)
{
   return (reinterpret_cast<std::size_t>(ptr) % alignment) == 0;
}

// Test: new_delete_resource() returns a non-null pointer
void test_returns_non_null()
{
   boost::container::pmr::memory_resource* mr = boost::container::pmr::new_delete_resource();
   BOOST_TEST(mr != 0);
}

// Test: new_delete_resource() always returns the same pointer (singleton)
void test_singleton()
{
   boost::container::pmr::memory_resource* mr1 = boost::container::pmr::new_delete_resource();
   boost::container::pmr::memory_resource* mr2 = boost::container::pmr::new_delete_resource();
   BOOST_TEST(mr1 == mr2);
}

// Test: allocate() returns properly aligned memory for default alignment
void test_allocate_default_alignment()
{
   boost::container::pmr::memory_resource* mr = boost::container::pmr::new_delete_resource();

   void* p = mr->allocate(100);
   BOOST_TEST(p != 0);
   BOOST_TEST(is_aligned(p, max_align));

   // Write to memory to ensure it's usable
   std::memset(p, 0xAB, 100);

   mr->deallocate(p, 100);
}

// Test: allocate() with various sizes
void test_allocate_various_sizes()
{
   boost::container::pmr::memory_resource* mr = boost::container::pmr::new_delete_resource();

   std::size_t sz = 1u;

   while (sz <= 8192u) {
      void* p = mr->allocate(sz);
      BOOST_TEST(p != 0);
      //See:
      //       WG14 N2293: Alignment requirements for memory management functions
      //
      //The alignment requirements of malloc, calloc, and realloc are ambiguously phrased for small
      //allocations. The "strong-alignment" reading demands _Alignof(max_align_t) alignment regardless
      //of size, while the "weak-alignment" reading requires only enough alignment to accommodate
      //types that could fit in the allocated memory—meaning small allocations need only align to the largest
      //power of two not exceeding the requested size. Many implementations use weak alignment
      BOOST_TEST(is_aligned(p, sz < max_align ? sz : max_align));

      // Write to allocated memory
      std::memset(p, 0xCD, sz);
      mr->deallocate(p, sz);
      sz *= 2u;
   }
}

// Test: allocate() with explicit alignments
void test_allocate_with_alignment()
{
   boost::container::pmr::memory_resource* mr = boost::container::pmr::new_delete_resource();

   for (std::size_t align = 1u; align <= max_align; align *= 2u) {
      void* p = mr->allocate(256, align);
      BOOST_TEST(p != 0);
      BOOST_TEST(is_aligned(p, align));

      // Write to allocated memory
      std::memset(p, 0xEF, 256);
      mr->deallocate(p, 256, align);
   }
}

// Test: allocate() with over-aligned memory (greater than max_align_t)
void test_allocate_over_aligned()
{
   boost::container::pmr::memory_resource* mr = boost::container::pmr::new_delete_resource();

   // Test alignments larger than max_align_t
   for (std::size_t align = max_align; align <= (max_align*16u); align *= 2u) {
      std::size_t sz = align;
      void* p = mr->allocate(sz, align);
      BOOST_TEST(p != 0);
      BOOST_TEST(is_aligned(p, sz));
      mr->deallocate(p, align, align);

      sz *= 2u;
      p = mr->allocate(sz, align);
      BOOST_TEST(p != 0);
      BOOST_TEST(is_aligned(p, align));
      mr->deallocate(p, sz, align);
   }
}

// Test: is_equal() - new_delete_resource equals itself
void test_is_equal_same()
{
   boost::container::pmr::memory_resource* mr1 = boost::container::pmr::new_delete_resource();
   boost::container::pmr::memory_resource* mr2 = boost::container::pmr::new_delete_resource();

   BOOST_TEST(mr1->is_equal(*mr2));
   BOOST_TEST(mr2->is_equal(*mr1));
}

// Test: is_equal() - comparison with other memory resources
void test_is_equal_different()
{
   boost::container::pmr::memory_resource* ndr = boost::container::pmr::new_delete_resource();
   boost::container::pmr::memory_resource* null_mr = boost::container::pmr::null_memory_resource();

   BOOST_TEST(!ndr->is_equal(*null_mr));
   BOOST_TEST(!null_mr->is_equal(*ndr));
}

// Test: is_equal() via operator==
void test_equality_operators()
{
   boost::container::pmr::memory_resource* mr1 = boost::container::pmr::new_delete_resource();
   boost::container::pmr::memory_resource* mr2 = boost::container::pmr::new_delete_resource();
   boost::container::pmr::memory_resource* null_mr = boost::container::pmr::null_memory_resource();

   BOOST_TEST(*mr1 == *mr2);
   BOOST_TEST(!(*mr1 != *mr2));

   BOOST_TEST(*mr1 != *null_mr);
   BOOST_TEST(!(*mr1 == *null_mr));
}

// Test: allocate() zero bytes (implementation-defined but should not crash)
void test_allocate_zero_bytes()
{
   boost::container::pmr::memory_resource* mr = boost::container::pmr::new_delete_resource();

   // Zero-size allocation behavior is implementation-defined
   // but it should not crash and should be deallocatable
   void* p = mr->allocate(0);
   // p may be 0 or a valid pointer depending on implementation
   mr->deallocate(p, 0);
}

// Test: multiple allocations without deallocation (stress test)
void test_multiple_allocations()
{
   boost::container::pmr::memory_resource* mr = boost::container::pmr::new_delete_resource();

   vector<void*> ptrs;
   ptrs.reserve(100);

   // Allocate many blocks
   for (std::size_t i = 0; i < 100; ++i) {
      void* p = mr->allocate(64 + i);
      BOOST_TEST(p != 0);
      ptrs.push_back(p);
   }

   // Deallocate all blocks
   for (std::size_t i = 0; i < 100; ++i) {
      mr->deallocate(ptrs[static_cast<std::size_t>(i)], 64 + i);
   }
}

void test_interleaved_alloc_dealloc()
{
   boost::container::pmr::memory_resource* mr = boost::container::pmr::new_delete_resource();

   void* p1 = mr->allocate(100);
   void* p2 = mr->allocate(200);
   void* p3 = mr->allocate(300);

   BOOST_TEST(p1 != 0);
   BOOST_TEST(p2 != 0);
   BOOST_TEST(p3 != 0);

   // Deallocate in different order
   mr->deallocate(p2, 200);

   void* p4 = mr->allocate(150);
   BOOST_TEST(p4 != 0);

   mr->deallocate(p1, 100);
   mr->deallocate(p3, 300);
   void* p5 = mr->allocate(250);
   BOOST_TEST(p5 != 0);
   mr->deallocate(p4, 150);
   mr->deallocate(p5, 250);
}

// Test: large allocation
void test_large_allocation()
{
   boost::container::pmr::memory_resource* mr = boost::container::pmr::new_delete_resource();

   // Allocate 1 MB
   BOOST_CONSTEXPR_OR_CONST std::size_t large_size = 1024 * 1024;
   void* p = mr->allocate(large_size);
   BOOST_TEST(p != 0);

   // Write to verify memory is accessible   
   std::memset(p, 0xFF, large_size);

   mr->deallocate(p, large_size);
}

// Test: memory resource can be used with pmr::vector
void test_with_pmr_vector()
{
   boost::container::pmr::memory_resource* mr = boost::container::pmr::new_delete_resource();
   boost::container::pmr::vector_of<std::size_t>::type vec(mr);

   for (std::size_t i = 0; i < 1000; ++i) {
      vec.push_back(i);
   }

   BOOST_TEST_EQ(vec.size(), 1000u);
   BOOST_TEST_EQ(vec[0], 0);
   BOOST_TEST_EQ(vec[999], 999);
   BOOST_TEST(vec.get_allocator().resource() == mr);
}

int main()
{
   test_returns_non_null();
   test_singleton();
   test_allocate_default_alignment();
   test_allocate_various_sizes();
   test_allocate_with_alignment();
   test_allocate_over_aligned();
   test_is_equal_same();
   test_is_equal_different();
   test_equality_operators();
   test_allocate_zero_bytes();
   test_multiple_allocations();
   test_interleaved_alloc_dealloc();
   test_large_allocation();
   test_with_pmr_vector();

   return boost::report_errors();
}
