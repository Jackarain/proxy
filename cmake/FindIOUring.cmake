# - Find IOUring
# Find the native liburing includes and library
#
#  IOUring_INCLUDE_DIR - where to find liburing.h, etc.
#  IOUring_LIBRARIES   - List of libraries when using liburing.
#  IOUring_FOUND       - True if liburing found.

find_path(IOUring_INCLUDE_DIR liburing.h)

find_library(IOUring_LIBRARY NAMES uring)

if (IOUring_INCLUDE_DIR AND IOUring_LIBRARY)
  set(IOUring_FOUND TRUE)
  set(IOUring_LIBRARIES ${IOUring_LIBRARY})

  # Create an imported target for modern target-based linking.
  if (NOT TARGET IOUring::IOUring)
    add_library(IOUring::IOUring INTERFACE IMPORTED)
    set_target_properties(IOUring::IOUring PROPERTIES
      INTERFACE_INCLUDE_DIRECTORIES "${IOUring_INCLUDE_DIR}"
      INTERFACE_LINK_LIBRARIES "${IOUring_LIBRARY}"
    )
  endif()
else()
  set(IOUring_FOUND FALSE)
  set(IOUring_LIBRARIES)
endif()

if (IOUring_FOUND)
  message(STATUS "Found IOUring: ${IOUring_LIBRARY}")
else()
  message(STATUS "Not Found IOUring")
  if (IOUring_FIND_REQUIRED)
    message(STATUS "Looked for IOUring libraries named ${IOUring_LIBRARY}.")
    message(FATAL_ERROR "Could NOT find IOUring library")
  endif()
endif()

mark_as_advanced(
  IOUring_LIBRARY
  IOUring_INCLUDE_DIR
)
