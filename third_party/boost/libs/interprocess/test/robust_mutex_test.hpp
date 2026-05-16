//////////////////////////////////////////////////////////////////////////////
//
// (C) Copyright Ion Gaztanaga 2010-2012. Distributed under the Boost
// Software License, Version 1.0. (See accompanying file
// LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//
// See http://www.boost.org/libs/interprocess for documentation.
//
//////////////////////////////////////////////////////////////////////////////

#ifndef BOOST_INTERPROCESS_TEST_ROBUST_MUTEX_TEST_HEADER
#define BOOST_INTERPROCESS_TEST_ROBUST_MUTEX_TEST_HEADER

#include <boost/interprocess/detail/config_begin.hpp>
#include <iostream>
#include <cstdlib> //std::system
#include <boost/interprocess/sync/scoped_lock.hpp>
#include <boost/interprocess/managed_shared_memory.hpp>
#include <boost/interprocess/sync/spin/wait.hpp>
#include "get_process_id_name.hpp"
#include "mutex_test_template.hpp"
#include <iostream>

namespace boost{
namespace interprocess{
namespace test{

template<class RobustMutex>
int robust_mutex_test(int argc, char *argv[])
{
   BOOST_INTERPROCESS_TRY{
   if(argc == 1){  //Parent process
      //First usual mutex tests
      {
       //  test_all_lock<RobustMutex>();
//         test_all_mutex<true, RobustMutex>();
      }
      std::cout << "PARENT: robust mutex recovery test" << std::endl;

      //Remove shared memory on construction and destruction
      class shm_remove
      {
         public:
         shm_remove(){ shared_memory_object::remove
            (::boost::interprocess::test::get_process_id_name()); }
         ~shm_remove(){ shared_memory_object::remove
            (::boost::interprocess::test::get_process_id_name()); }
      } remover;
      (void)remover;

      //Construct managed shared memory
      managed_shared_memory segment(create_only, get_process_id_name(), 65536);

      //Create two robust mutexes
      RobustMutex *instance = segment.construct<RobustMutex>
         ("robust mutex")[2]();

      //Create a flag to notify that both mutexes are
      //locked and the owner is going to die soon.
      bool *go_ahead = segment.construct<bool> ("go ahead")(false);

      //Launch child process
      std::string s(argv[0]); s += " child ";
      s += get_process_id_name();
      std::cout << "PARENT: ... launching child: " << s << std::endl;
      int r = std::system(s.c_str());
      if(0 != r){
         std::cout << "PARENT: std::system" << s.c_str() << " FAILED with " << r << std::endl;
         return 1;
      }

      std::cout << "PARENT: ... launched child: " << s << std::endl;

      //Wait until child locks the mutexes and dies
      spin_wait swait;
      while(!*go_ahead){
         swait.yield();
      }

      std::cout << "PARENT: ... recovering mutex[0]" << std::endl;
      //First try to recover lock[0], put into consistent
      //state and relock it again
      {
         //Done, now try to lock it to see if robust
         //mutex recovery works
         instance[0].lock();
         if(!instance[0].previous_owner_dead()){
            std::cout << "PARENT: instance[0].previous_owner_dead() FAILED!" << std::endl;
            return 1;
         }
         instance[0].consistent();
         instance[0].unlock();
         //Since it's consistent, locking is possible again
         instance[0].lock();
         instance[0].unlock();
      }
      //Now with lock[1], but dont' put it in consistent state
      //so the mutex is no longer usable
      std::cout << "PARENT: ... recovering mutex[1]" << std::endl;
      {
         //Done, now try to lock it to see if robust
         //mutex recovery works
         instance[1].lock();
         if(!instance[1].previous_owner_dead()){
            std::cout << "PARENT: instance[1].previous_owner_dead() FAILED!" << std::endl;
            return 1;
         }
         //Unlock a recovered mutex without putting it into
         //into consistent state marks mutex as unusable.
         instance[1].unlock();
         //Since it's NOT consistent, locking is NOT possible again
         bool exception_thrown = false;
         BOOST_INTERPROCESS_TRY{
            instance[1].lock();
         }
         BOOST_INTERPROCESS_CATCH(interprocess_exception &){
            exception_thrown = true;
         } BOOST_INTERPROCESS_CATCH_END
         if(!exception_thrown){
            std::cout << "PARENT: instance[1].lock() did NOT throw an exception!" << std::endl;
            return 1;
         }
      }
      //Now with lock[2], this was locked by child but not
      //unlocked
      std::cout << "PARENT: ... recovering mutex[2]" << std::endl;
      {
         //Done, now try to lock it to see if robust
         //mutex recovery works
         instance[2].lock();
         if(!instance[2].previous_owner_dead()){
            std::cout << "PARENT: instance[2].previous_owner_dead() FAILED" << std::endl;
            return 1;
         }
         //Unlock a recovered mutex without putting it into
         //into consistent state marks mutex as unusable.
         instance[2].unlock();
         //Since it's NOT consistent, locking is NOT possible again
         bool exception_thrown = false;
         BOOST_INTERPROCESS_TRY{
            instance[2].lock();
         }
         BOOST_INTERPROCESS_CATCH(interprocess_exception &){
            exception_thrown = true;
         } BOOST_INTERPROCESS_CATCH_END
         if(!exception_thrown){
            std::cout << "PARENT: instance[2].lock() did not throw an Exception!" << std::endl;
            return 1;
         }
      }
   }
   else{
      //Open managed shared memory
      managed_shared_memory segment(open_only, argv[2]);
      //Find mutexes
      RobustMutex *instance = segment.find<RobustMutex>("robust mutex").first;
      assert(instance);
      if(std::string(argv[1]) == std::string("child")){
         std::cout << "CHILD: starting" << std::endl;
         //Find flag
         bool *go_ahead = segment.find<bool>("go ahead").first;
         assert(go_ahead);
         //Lock, flag and die
         bool try_lock_res = instance[0].try_lock() && instance[1].try_lock();
         assert(try_lock_res);
         if(!try_lock_res){
            std::cout << "CHILD: 'instance[0].try_lock() && instance[1].try_lock()' FAILED!" << std::endl;
            return 1;
         }

         bool *go_ahead2 = segment.construct<bool>("go ahead2")(false);
         assert(go_ahead2);
         //Launch grandchild
         std::string s(argv[0]); s += " grandchild ";
         s += argv[2];
         std::cout << "CHILD: launching grandchild: " << s << std::endl;
         int r = std::system(s.c_str());
         if(0 != r){
            std::cout << "CHILD: --> std::system" << s.c_str() << " FAILED with " << r << std::endl;
            return 1;
         }
         std::cout << "CHILD: Launched grandchild: " << s << std::endl;

         //Wait until child locks the 2nd mutex and dies
         spin_wait swait;
         while(!*go_ahead2){
            swait.yield();
         }

         std::cout << "CHILD: go ahead2 received" << std::endl;

         //Done, now try to lock number 3 to see if robust
         //mutex recovery works
         instance[2].lock();

         std::cout << "CHILD: instance[2].lock() OK" << std::endl;

         if(!instance[2].previous_owner_dead()){
            std::cout << "CHILD: instance[2].previous_owner_dead() FAILED!" << std::endl;
            return 1;
         }
         std::cout << "CHILD: instance[2].previous_owner_dead() OK" << std::endl;
         *go_ahead = true;
         std::cout << "CHILD: Exiting" << std::endl;
      }
      else{
         std::cout << "GRANDCHILD: Starting" << std::endl;
         //grandchild locks the lock and dies
         bool *go_ahead2 = segment.find<bool>("go ahead2").first;
         assert(go_ahead2);
         //Lock, flag and die
         bool try_lock_res = instance[2].try_lock();
         assert(try_lock_res);
         std::cout << "GRANDCHILD: instance[2].try_lock() OK" << std::endl;
         if(!try_lock_res){
            std::cout << "GRANDCHILD: instance[2].try_lock() FAILED!" << std::endl;
            return 1;
         }
         *go_ahead2 = true;
         std::cout << "GRANDCHILD: Exiting" << std::endl;
      }
   }
   }BOOST_INTERPROCESS_CATCH(...){
      std::cout << "Exception thrown error!" << std::endl;
      BOOST_INTERPROCESS_RETHROW
   } BOOST_INTERPROCESS_CATCH_END
   return 0;
}

}  //namespace test{
}  //namespace interprocess{
}  //namespace boost{

#include <boost/interprocess/detail/config_end.hpp>

#endif   //BOOST_INTERPROCESS_TEST_ROBUST_EMULATION_TEST_HEADER
