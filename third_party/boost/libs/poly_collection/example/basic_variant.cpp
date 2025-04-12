/* Copyright 2024 Joaquin M Lopez Munoz.
 * Distributed under the Boost Software License, Version 1.0.
 * (See accompanying file LICENSE_1_0.txt or copy at
 * http://www.boost.org/LICENSE_1_0.txt)
 *
 * See http://www.boost.org/libs/poly_collection for library home page.
 */

/* basic usage of boost::variant_collection */

#include <boost/poly_collection/variant_collection.hpp>
#include <random>
#include "rolegame.hpp"

template<typename... Ts>
struct overloaded:Ts...{using Ts::operator()...;};
template<class... Ts>
overloaded(Ts...)->overloaded<Ts...>;

int main()
{
//[basic_variant_1
//=  #include <boost/poly_collection/variant_collection.hpp>
//=  ...
//=
  boost::variant_collection<
    boost::mp11::mp_list<warrior,juggernaut,goblin,elf,std::string,window>
  > c;
//]

  {
//[basic_variant_2
  boost::variant_collection_of<
    warrior,juggernaut,goblin,elf,std::string,window
  > c;
//]
  }

  // populate with sprites
  std::mt19937                 gen{92754}; // some arbitrary random seed
  std::discrete_distribution<> rnd{{1,1,1,1}};
  for(int i=0;i<5;++i){        // assign each type with 1/4 probability
    switch(rnd(gen)){ 
      case 0: c.insert(warrior{i});break;
      case 1: c.insert(juggernaut{i});break;
      case 2: c.insert(goblin{i});break;
      case 3: c.insert(elf{i});break;
    }
  }

  // populate with messages
  c.insert(std::string{"\"stamina: 10,000\""});
  c.insert(std::string{"\"game over\""});

  // populate with windows
  c.insert(window{"pop-up 1"});
  c.insert(window{"pop-up 2"});

  {
//[basic_variant_3
//=    // usual utility to construct a visitor
//=    template<typename... Ts>
//=    struct overloaded:Ts...{using Ts::operator()...;};
//=    template<class... Ts>
//=    overloaded(Ts...)->overloaded<Ts...>;

    const char* comma="";
    for(const auto& r:c){
      std::cout<<comma;
      visit(overloaded{
        [](const sprite& s)       { s.render(std::cout); },
        [](const std::string& str){ std::cout<<str; },
        [](const window& w)       { w.display(std::cout); }
      },r);
      comma=",";
    }
    std::cout<<"\n";
//]
  }

  {
//[basic_variant_4
    auto print_sprite=[](const sprite& s)       { s.render(std::cout); };
    auto print_string=[](const std::string& str){ std::cout<<str; };
    auto print_window=[](const window& w)       { w.display(std::cout); };

    const char* comma="";
    for(const auto& r:c){
      std::cout<<comma;
      visit_by_index(
        r,
        print_sprite,  // type #0: warrior
        print_sprite,  // type #1: juggernaut
        print_sprite,  // type #2: goblin
        print_sprite,  // type #3: elf
        print_string,  // type #4
        print_window); // type #5
      comma=",";
    }
    std::cout<<"\n";
//]
  }
}
