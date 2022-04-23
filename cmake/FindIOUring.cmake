# - Find Tcmalloc
# Find the native Tcmalloc includes and library
#
#  Tcmalloc_INCLUDE_DIR - where to find Tcmalloc.h, etc.
#  Tcmalloc_LIBRARIES   - List of libraries when using Tcmalloc.
#  Tcmalloc_FOUND       - True if Tcmalloc found.

find_path(IOUring_INCLUDE_DIR liburing.h)

find_library(IOUring_LIBRARY NAMES uring)

if (IOUring_INCLUDE_DIR AND IOUring_LIBRARY)
  set(IOUring_FOUND TRUE)
  set(IOUring_LIBRARIES ${IOUring_LIBRARY} )
else ()
  set(IOUring_FOUND FALSE)
  set( IOUring_LIBRARIES )
endif ()

if (IOUring_FOUND)
  message(STATUS "Found IOUring: ${IOUring_LIBRARY}")
else ()
  message(STATUS "Not Found IOUring")
  if (IOUring_FIND_REQUIRED)
	  message(STATUS "Looked for IOUring libraries named ${IOUring_LIBRARY}.")
	  message(FATAL_ERROR "Could NOT find IOUring library")
  endif ()
endif ()

mark_as_advanced(
  IOUring_LIBRARY
  IOUring_INCLUDE_DIR
)

