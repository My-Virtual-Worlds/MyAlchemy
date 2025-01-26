# -*- cmake -*-
include(Linking)
include(Prebuilt)

include_guard()

# ND: Turn this off by default, the openal code in the viewer isn't very well maintained, seems
# to have memory leaks, has no option to play music streams
# It probably makes sense to to completely remove it

set(USE_OPENAL ON CACHE BOOL "Enable OpenAL")
# ND: To streamline arguments passed, switch from OPENAL to USE_OPENAL
# To not break all old build scripts convert old arguments but warn about it
if(OPENAL)
  message( WARNING "Use of the OPENAL argument is deprecated, please switch to USE_OPENAL")
  set(USE_OPENAL ${OPENAL})
endif()

if (USE_OPENAL)
  add_library( ll::openal INTERFACE IMPORTED )
  target_include_directories( ll::openal SYSTEM INTERFACE "${LIBS_PREBUILT_DIR}/include/")
  target_compile_definitions( ll::openal INTERFACE LL_OPENAL=1)
  use_prebuilt_binary(openal)

  if(WINDOWS)
    target_link_libraries( ll::openal INTERFACE
            debug ${ARCH_PREBUILT_DIRS_DEBUG}/OpenAL32.lib
            optimized ${ARCH_PREBUILT_DIRS_RELEASE}/OpenAL32.lib
            debug ${ARCH_PREBUILT_DIRS_DEBUG}/alut.lib
            optimized ${ARCH_PREBUILT_DIRS_RELEASE}/alut.lib
        )
  elseif (DARWIN)
    target_link_libraries( ll::openal INTERFACE
            debug ${ARCH_PREBUILT_DIRS_DEBUG}/libopenal.dylib
            optimized ${ARCH_PREBUILT_DIRS_RELEASE}/libopenal.dylib
            debug ${ARCH_PREBUILT_DIRS_DEBUG}/libalut.dylib
            optimized ${ARCH_PREBUILT_DIRS_RELEASE}/libalut.dylib
        )
  elseif (LINUX)
    target_link_libraries( ll::openal INTERFACE
            debug ${ARCH_PREBUILT_DIRS_DEBUG}/libopenal.so
            optimized ${ARCH_PREBUILT_DIRS_RELEASE}/libopenal.so
            debug ${ARCH_PREBUILT_DIRS_DEBUG}/libalut.so
            optimized ${ARCH_PREBUILT_DIRS_RELEASE}/libalut.so
            )
  else()
    message(FATAL_ERROR "OpenAL is not available for this platform")
  endif()
endif ()
