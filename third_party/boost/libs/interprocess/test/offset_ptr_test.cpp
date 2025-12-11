//////////////////////////////////////////////////////////////////////////////
//
// (C) Copyright Ion Gaztanaga 2007-2012. Distributed under the Boost
// Software License, Version 1.0. (See accompanying file
// LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//
// See http://www.boost.org/libs/interprocess for documentation.
//
//////////////////////////////////////////////////////////////////////////////

#include <boost/interprocess/offset_ptr.hpp>
#include <boost/interprocess/detail/type_traits.hpp>
#include <boost/intrusive/pointer_traits.hpp>
#include <boost/core/lightweight_test.hpp>

using namespace boost::interprocess;

class Base
{
   int padding;
   public:
   Base() : padding(0){}
   int *get() { return &padding; }
   virtual ~Base(){}
};

class Base2
{
   int padding;
   public:
   Base2() : padding(0){}
   int *get() { return &padding; }
   virtual ~Base2(){}
};

class Base3
{
   int padding;
   public:
   Base3() : padding(0){}
   int *get() { return &padding; }
   virtual ~Base3(){}
};

class Derived
   : public Base
{};

class Derived3
   : public Base, public Base2, public Base3
{};

class VirtualDerived
   : public virtual Base
{};

class VirtualDerived3
   : public virtual Base, public virtual Base2, public virtual Base3
{};

void test_types_and_conversions()
{
   typedef offset_ptr<int>                pint_t;
   typedef offset_ptr<const int>          pcint_t;
   typedef offset_ptr<volatile int>       pvint_t;
   typedef offset_ptr<const volatile int> pcvint_t;

   BOOST_INTERPROCESS_STATIC_ASSERT((ipcdetail::is_same<pint_t::element_type, int>::value));
   BOOST_INTERPROCESS_STATIC_ASSERT((ipcdetail::is_same<pcint_t::element_type, const int>::value));
   BOOST_INTERPROCESS_STATIC_ASSERT((ipcdetail::is_same<pvint_t::element_type, volatile int>::value));
   BOOST_INTERPROCESS_STATIC_ASSERT((ipcdetail::is_same<pcvint_t::element_type, const volatile int>::value));

   BOOST_INTERPROCESS_STATIC_ASSERT((ipcdetail::is_same<pint_t::value_type,   int>::value));
   BOOST_INTERPROCESS_STATIC_ASSERT((ipcdetail::is_same<pcint_t::value_type,  int>::value));
   BOOST_INTERPROCESS_STATIC_ASSERT((ipcdetail::is_same<pvint_t::value_type,  int>::value));
   BOOST_INTERPROCESS_STATIC_ASSERT((ipcdetail::is_same<pcvint_t::value_type, int>::value));
   int dummy_int = 9;

   {  pint_t pint(&dummy_int);
      pcint_t  pcint(pint);
      BOOST_TEST(pcint.get() == &dummy_int);
   }
   {  pint_t pint(&dummy_int);
      pvint_t  pvint(pint);
      BOOST_TEST(pvint.get() == &dummy_int);
   }
   {  pint_t pint(&dummy_int);
      pcvint_t  pcvint(pint);
      BOOST_TEST(pcvint.get() == &dummy_int);
   }
   {  pcint_t pcint(&dummy_int);
      pcvint_t  pcvint(pcint);
      BOOST_TEST(pcvint.get() == &dummy_int);
   }
   {  pvint_t pvint(&dummy_int);
      pcvint_t  pcvint(pvint);
      BOOST_TEST(pcvint.get() == &dummy_int);
   }

   pint_t   pint(0);
   pcint_t  pcint(0);
   pvint_t  pvint(0);
   pcvint_t pcvint(0);

   {
      pint_t   pint2 = 0;
      pcint_t  pcint2 = 0;
      pvint_t  pvint2 = 0;
      pcvint_t pcvint2 = 0;
      (void)pint2;
      (void)pcint2;
      (void)pvint2;
      (void)pcvint2;
   }

   {
      pint_t   pint2 = op_nullptr_t();
      pcint_t  pcint2 = op_nullptr_t();
      pvint_t  pvint2 = op_nullptr_t();
      pcvint_t pcvint2 = op_nullptr_t();
      (void)pint2;
      (void)pcint2;
      (void)pvint2;
      (void)pcvint2;
   }

   {
      pint_t   pint2((op_nullptr_t()));
      pcint_t  pcint2((op_nullptr_t()));
      pvint_t  pvint2((op_nullptr_t()));
      pcvint_t pcvint2((op_nullptr_t()));
      (void)pint2;
      (void)pcint2;
      (void)pvint2;
      (void)pcvint2;
   }

   pint     = &dummy_int;
   pcint    = &dummy_int;
   pvint    = &dummy_int;
   pcvint   = &dummy_int;

   {  pcint  = pint;
      BOOST_TEST(pcint.get() == &dummy_int);
   }
   {  pvint  = pint;
      BOOST_TEST(pvint.get() == &dummy_int);
   }
   {  pcvint = pint;
      BOOST_TEST(pcvint.get() == &dummy_int);
   }
   {  pcvint = pcint;
      BOOST_TEST(pcvint.get() == &dummy_int);
   }
   {  pcvint = pvint;
      BOOST_TEST(pcvint.get() == &dummy_int);
   }

   BOOST_TEST(pint);

   pint = 0;
   BOOST_TEST(!pint);

   BOOST_TEST(pint == 0);

   BOOST_TEST(0 == pint);

   pint = &dummy_int;
   BOOST_TEST(0 != pint);

   pcint = &dummy_int;

   BOOST_TEST( (pcint - pint) == 0);
   BOOST_TEST( (pint - pcint) == 0);

   typedef offset_ptr<void>                pvoid_t;
   typedef offset_ptr<const void>          pcvoid_t;
   typedef offset_ptr<volatile void>       pvvoid_t;
   typedef offset_ptr<const volatile void> pcvvoid_t;
   {
      pvoid_t   pvoid  = pint;
      pcvoid_t  pcvoid = pcint;
      pvvoid_t  pvvoid = pvint;
      pcvvoid_t pcvvoid = pcvint;
      (void)pvoid;
      (void)pcvoid;
      (void)pvvoid;
      (void)pcvvoid;
   }
   {
      pvoid_t   pvoid(pint);
      pcvoid_t  pcvoid(pcint);
      pvvoid_t  pvvoid(pvint);
      pcvvoid_t pcvvoid(pcvint);
      (void)pvoid;
      (void)pcvoid;
      (void)pvvoid;
      (void)pcvvoid;
   }
   {
      pvoid_t   pvoid;
      pcvoid_t  pcvoid;
      pvvoid_t  pvvoid;
      pcvvoid_t pcvvoid;

      pvoid = pint;
      pcvoid = pcint;
      pvvoid = pvint;
      pcvvoid = pcvint;

      (void)pvoid;
      (void)pcvoid;
      (void)pvvoid;
      (void)pcvvoid;
   }
}

template<class BasePtr, class DerivedPtr>
void test_base_derived_impl()
{
   typename DerivedPtr::element_type d;
   DerivedPtr pderi(&d);

   {  BasePtr pbase2 = pderi;  (void)pbase2; }

   BasePtr pbase(pderi);
   pbase = pderi;
   BOOST_TEST(pbase == pderi);
   BOOST_TEST(!(pbase != pderi));
   BOOST_TEST((pbase - pderi) == 0);
   BOOST_TEST(!(pbase < pderi));
   BOOST_TEST(!(pbase > pderi));
   BOOST_TEST(pbase <= pderi);
   BOOST_TEST((pbase >= pderi));
}

void test_base_derived()
{
   typedef offset_ptr<Base>               pbase_t;
   typedef offset_ptr<const Base>         pcbas_t;
   typedef offset_ptr<Derived>            pderi_t;
   typedef offset_ptr<VirtualDerived>     pvder_t;

   test_base_derived_impl<pbase_t, pderi_t>();
   test_base_derived_impl<pbase_t, pvder_t>();
   test_base_derived_impl<pcbas_t, pderi_t>();
   test_base_derived_impl<pcbas_t, pvder_t>();
}

void test_arithmetic()
{
   typedef offset_ptr<int> pint_t;
   const int NumValues = 5;
   int values[NumValues];

   //Initialize p
   pint_t p = values;
   BOOST_TEST(p.get() == values);

   //Initialize p + NumValues
   pint_t pe = &values[NumValues];
   BOOST_TEST(pe != p);
   BOOST_TEST(pe.get() == &values[NumValues]);

   //ptr - ptr
   BOOST_TEST((pe - p) == NumValues);

   //ptr - integer
   BOOST_TEST((pe - NumValues) == p);

   //ptr + integer
   BOOST_TEST((p + NumValues) == pe);

   //integer + ptr
   BOOST_TEST((NumValues + p) == pe);

   //indexing
   BOOST_TEST(pint_t(&p[NumValues]) == pe);
   BOOST_TEST(pint_t(&pe[-NumValues]) == p);

   //ptr -= integer
   pint_t p0 = pe;
   p0-= NumValues;
   BOOST_TEST(p == p0);

   //ptr += integer
   pint_t penew = p0;
   penew += NumValues;
   BOOST_TEST(penew == pe);

   //++ptr
   penew = p0;
   for(int j = 0; j != NumValues; ++j, ++penew);
   BOOST_TEST(penew == pe);

   //--ptr
   p0 = pe;
   for(int j = 0; j != NumValues; ++j, --p0);
   BOOST_TEST(p == p0);

   //ptr++
   penew = p0;
   for(int j = 0; j != NumValues; ++j){
      pint_t p_new_copy = penew;
      BOOST_TEST(p_new_copy == penew++);
   }
   //ptr--
   p0 = pe;
   for(int j = 0; j != NumValues; ++j){
      pint_t p0_copy = p0;
      BOOST_TEST(p0_copy == p0--);
   }
}

void test_comparison()
{
   typedef offset_ptr<int> pint_t;
   const int NumValues = 5;
   int values[NumValues];

   //Initialize p
   pint_t p = values;
   BOOST_TEST(p.get() == values);

   //Initialize p + NumValues
   pint_t pe = &values[NumValues];
   BOOST_TEST(pe != p);

   BOOST_TEST(pe.get() == &values[NumValues]);

   //operators
   BOOST_TEST(p != pe);
   BOOST_TEST(p == p);
   BOOST_TEST((p < pe));
   BOOST_TEST((p <= pe));
   BOOST_TEST((pe > p));
   BOOST_TEST((pe >= p));
}

bool test_pointer_traits()
{
   typedef offset_ptr<int> OInt;
   typedef boost::intrusive::pointer_traits< OInt > PTOInt;
   BOOST_INTERPROCESS_STATIC_ASSERT((ipcdetail::is_same<PTOInt::element_type, int>::value));
   BOOST_INTERPROCESS_STATIC_ASSERT((ipcdetail::is_same<PTOInt::pointer, OInt >::value));
   BOOST_INTERPROCESS_STATIC_ASSERT((ipcdetail::is_same<PTOInt::difference_type, OInt::difference_type >::value));
   BOOST_INTERPROCESS_STATIC_ASSERT((ipcdetail::is_same<PTOInt::rebind_pointer<double>::type, offset_ptr<double> >::value));
   int dummy;
   OInt oi(&dummy);
   if(boost::intrusive::pointer_traits<OInt>::pointer_to(dummy) != oi){
      return false;
   }
   return true;
}

struct node 
{
   offset_ptr<node> next;
};

void test_pointer_plus_bits()
{
   BOOST_INTERPROCESS_STATIC_ASSERT((boost::intrusive::max_pointer_plus_bits< offset_ptr<void>, boost::move_detail::alignment_of<node>::value >::value >= 1U));
   typedef boost::intrusive::pointer_plus_bits< offset_ptr<node>, 1u > ptr_plus_bits;

   node n, n2;
   offset_ptr<node> pnode(&n);

   BOOST_TEST(ptr_plus_bits::get_pointer(pnode) == &n);
   BOOST_TEST(0 == ptr_plus_bits::get_bits(pnode));
   ptr_plus_bits::set_bits(pnode, 1u);
   BOOST_TEST(1 == ptr_plus_bits::get_bits(pnode));
   BOOST_TEST(ptr_plus_bits::get_pointer(pnode) == &n);

   ptr_plus_bits::set_pointer(pnode, &n2);
   BOOST_TEST(ptr_plus_bits::get_pointer(pnode) == &n2);
   BOOST_TEST(1 == ptr_plus_bits::get_bits(pnode));
   ptr_plus_bits::set_bits(pnode, 0u);
   BOOST_TEST(0 == ptr_plus_bits::get_bits(pnode));
   BOOST_TEST(ptr_plus_bits::get_pointer(pnode) == &n2);

   ptr_plus_bits::set_pointer(pnode, offset_ptr<node>());
   BOOST_TEST(ptr_plus_bits::get_pointer(pnode) ==0);
   BOOST_TEST(0 == ptr_plus_bits::get_bits(pnode));
   ptr_plus_bits::set_bits(pnode, 1u);
   BOOST_TEST(1 == ptr_plus_bits::get_bits(pnode));
   BOOST_TEST(ptr_plus_bits::get_pointer(pnode) == 0);
}

void test_cast()
{
   typedef offset_ptr<int>                pint_t;
   typedef offset_ptr<const int>          pcint_t;
   typedef offset_ptr<volatile int>       pvint_t;
   typedef offset_ptr<const volatile int> pcvint_t;
   typedef offset_ptr<void>                pvoid_t;
   typedef offset_ptr<const void>          pcvoid_t;
   typedef offset_ptr<volatile void>       pvvoid_t;
   typedef offset_ptr<const volatile void> pcvvoid_t;

   int dummy_int = 9;

   {  pint_t   pint(&dummy_int);
      pcint_t  pcint(pint);
      pvint_t  pvint = pint;
      pcvint_t pcvint = pvint;

      pvoid_t  pvoid = pint;
      pcvoid_t pcvoid = pvoid;
      pvvoid_t pvvoid = pvoid;
      pcvvoid_t pcvvoid = pvoid;
      pcvvoid = pvvoid;

      //Test valid static_cast conversions
      pint  = static_pointer_cast<int>(pvoid);
      pcint = static_pointer_cast<const int>(pcvoid);
      pvint = static_pointer_cast<volatile int>(pvoid);
      pcvint = static_pointer_cast<const volatile int>(pvoid);

      BOOST_TEST(pint == pvoid);
      BOOST_TEST(pcint == pint);
      BOOST_TEST(pvint == pint);
      BOOST_TEST(pcvint == pint);

      //Test valid static_cast conversions
      {
         pint   = static_pointer_cast<int>(pvoid);
         pcint  = static_pointer_cast<const int>(pcvoid);
         pvint  = static_pointer_cast<volatile int>(pvoid);
         pcvint = static_pointer_cast<const volatile int>(pvoid);

         Derived d;
         offset_ptr<Derived> pd(&d);
         offset_ptr<Base> pb;
         //Downcast
         pb = static_pointer_cast<Base>(pd);
         //Upcast
         pd = static_pointer_cast<Derived>(pb);

         Derived3 d3;
         offset_ptr<Derived3> pd3(&d3);
         offset_ptr<Base3> pb3;

         //Downcast
         pb3 = static_pointer_cast<Base3>(pd3);
         //Upcast
         pd3 = static_pointer_cast<Derived3>(pb3);
         //Test addresses don't match in multiple inheritance
         BOOST_TEST((pvoid_t)pb3 != (pvoid_t)pd3);
         BOOST_TEST(pb3.get() == static_cast<Base3*>(pd3.get()));
      }

      //Test valid const_cast conversions
      {
         pint = const_pointer_cast<int>(pcint);
         pint = const_pointer_cast<int>(pvint);
         pint = const_pointer_cast<int>(pcvint);

         pvint = const_pointer_cast<volatile int>(pcint);
         pvint = const_pointer_cast<volatile int>(pcvint);

         pcint = const_pointer_cast<const int>(pvint);
         pcint = const_pointer_cast<const int>(pcvint);

         //Test valid reinterpret_cast conversions
         pint   = reinterpret_pointer_cast<int>(pvoid);
         pcint  = reinterpret_pointer_cast<const int>(pcvoid);
         pvint  = reinterpret_pointer_cast<volatile int>(pvoid);
         pcvint = reinterpret_pointer_cast<const volatile int>(pvoid);
      }

      //Test valid dynamic_cast conversions
      {
         {
            Derived3 d3;
            offset_ptr<Derived3> pd3(&d3);
            offset_ptr<Base2> pb2;

            //Downcast
            pb2 = dynamic_pointer_cast<Base2>(pd3);
            //Upcast
            pd3 = dynamic_pointer_cast<Derived3>(pb2);
            BOOST_TEST((pvoid_t)pb2 != (pvoid_t)pd3);
            BOOST_TEST(pb2.get() == dynamic_cast<Base2*>(&d3));
            BOOST_TEST(static_cast<void*>(pb2.get()) != static_cast<void*>(pd3.get()));
         }
         {
            VirtualDerived3 vd3;
            offset_ptr<VirtualDerived3> pdv3(&vd3);
            offset_ptr<Base3> pb3;
            offset_ptr<Base2> pb2;
            offset_ptr<Base>  pb;

            //Downcast
            pb3 = dynamic_pointer_cast<Base3>(pdv3);
            pb2 = dynamic_pointer_cast<Base2>(pdv3);
            pb  = dynamic_pointer_cast<Base> (pdv3);
            //Upcast
            pdv3 = dynamic_pointer_cast<VirtualDerived3>(pb);
            pdv3 = dynamic_pointer_cast<VirtualDerived3>(pb2);
            pdv3 = dynamic_pointer_cast<VirtualDerived3>(pb3);
            //Test addresses don't match in multiple inheritance
            BOOST_TEST((pvoid_t)pb2 != (pvoid_t)pdv3);
            BOOST_TEST(pb2.get() != static_cast<void*>(pdv3.get()));
            BOOST_TEST((pvoid_t)pb3 != (pvoid_t)pdv3);
            BOOST_TEST(pb3.get() != static_cast<void*>(pdv3.get()));
         }
      }
   }
}

int main()
{
   test_types_and_conversions();
   test_base_derived();
   test_arithmetic();
   test_comparison();
   test_pointer_traits();
   test_pointer_plus_bits();
   test_cast();
   return ::boost::report_errors();
}
