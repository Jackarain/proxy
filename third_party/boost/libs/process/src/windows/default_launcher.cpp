// Copyright (c) 2022 Klemens D. Morgenstern (klemens dot morgenstern at gmx dot net)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#include <boost/process/v2/detail/config.hpp>

#if defined(BOOST_PROCESS_V2_WINDOWS)
#include <boost/asio/windows/basic_object_handle.hpp>
#endif


#if defined(BOOST_PROCESS_V2_WINDOWS)

#include <boost/process/v2/windows/default_launcher.hpp>

BOOST_PROCESS_V2_BEGIN_NAMESPACE
namespace windows
{

  std::size_t default_launcher::escaped_argv_length(basic_string_view<wchar_t> ws)
  {
    if (ws.empty())
      return 2u; // just quotes

    constexpr static auto quote = L'"';
    const auto needs_quotes = ws.find_first_of(L" \t") != basic_string_view<wchar_t>::npos;

    std::size_t needed_escapes = 0u;
    for (auto itr = ws.begin(); itr != ws.end(); itr ++)
    {
      if (*itr == quote)
        needed_escapes++;
      else if (*itr == L'\\')
      {
        auto nx = std::next(itr);
        if (nx != ws.end() && *nx == L'"')
          needed_escapes ++;
        else if (nx == ws.end())
          needed_escapes ++;      
      }
    }
    
    return ws.size() + needed_escapes + (needs_quotes ? 2u : 0u);
  }


  std::size_t default_launcher::escape_argv_string(wchar_t * itr, std::size_t max_size, 
                                        basic_string_view<wchar_t> ws)
  { 
    constexpr static auto quote = L'"';
    const auto needs_quotes = ws.find_first_of(L" \t") != basic_string_view<wchar_t>::npos;
    const auto needed_escapes = std::count(ws.begin(), ws.end(), quote);
    
    const auto sz = ws.size() + needed_escapes + (needs_quotes ? 2u : 0u);
    
    if (sz > max_size)
      return 0u;

    if (ws.empty())      
    {
      itr[0] = quote;
      itr[1] = quote;
      return 2u;
    }

    const auto end = itr + sz; 
    const auto begin = itr;
    if (needs_quotes)
      *(itr++) = quote;

    for (auto it = ws.begin(); it != ws.end(); it ++)
    {
      if (*it == quote) // makes it \"
        *(itr++) = L'\\';
        
      if (*it == L'\\') // \" needs to become \\\"
      {
        auto nx = std::next(it);
        if (nx != ws.end() && *nx == L'"')
          *(itr++) = L'\\';
        else if (nx == ws.end())
          *(itr++) = L'\\';

      }

      *(itr++) = *it;
    }

    if (needs_quotes)
      *(itr++) = quote;
    return itr - begin;
  }

  LPPROC_THREAD_ATTRIBUTE_LIST default_launcher::get_thread_attribute_list(error_code & ec)
  {
    if (startup_info.lpAttributeList != nullptr)
      return startup_info.lpAttributeList;
    SIZE_T size;
    if (!(::InitializeProcThreadAttributeList(NULL, 1, 0, &size) ||
          GetLastError() == ERROR_INSUFFICIENT_BUFFER))
    {
      BOOST_PROCESS_V2_ASSIGN_LAST_ERROR(ec);
      return nullptr;
    }

    LPPROC_THREAD_ATTRIBUTE_LIST lpAttributeList = reinterpret_cast<LPPROC_THREAD_ATTRIBUTE_LIST>(::HeapAlloc(::GetProcessHeap(), 0, size));
    if (lpAttributeList == nullptr)
      return nullptr;

    if (!::InitializeProcThreadAttributeList(lpAttributeList, 1, 0, &size))
    {
      BOOST_PROCESS_V2_ASSIGN_LAST_ERROR(ec);
      ::HeapFree(GetProcessHeap(), 0, lpAttributeList);
      return nullptr;
    }

    proc_attribute_list_storage.reset(lpAttributeList);
    startup_info.lpAttributeList = proc_attribute_list_storage.get();
    return startup_info.lpAttributeList;
  }

  void default_launcher::set_handle_list(error_code & ec)
  {
    auto tl = get_thread_attribute_list(ec);
    if (ec)
      return;
    
    auto itr  = std::unique(inherited_handles.begin(), inherited_handles.end());
    auto size = std::distance(inherited_handles.begin(), itr);
      
    if (!::UpdateProcThreadAttribute(
        tl, 0, PROC_THREAD_ATTRIBUTE_HANDLE_LIST,
        inherited_handles.data(), size * sizeof(HANDLE), nullptr, nullptr))
      BOOST_PROCESS_V2_ASSIGN_LAST_ERROR(ec);
  }

}
BOOST_PROCESS_V2_END_NAMESPACE

#endif

