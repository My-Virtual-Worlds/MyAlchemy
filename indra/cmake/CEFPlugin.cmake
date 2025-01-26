# -*- cmake -*-
include(Linking)
include(Prebuilt)

include_guard()
add_library( ll::cef INTERFACE IMPORTED )

use_prebuilt_binary(dullahan)
target_include_directories( ll::cef SYSTEM INTERFACE  ${LIBS_PREBUILT_DIR}/include/cef)

if (WINDOWS)
    target_link_libraries( ll::cef INTERFACE
        ${ARCH_PREBUILT_DIRS_RELEASE}/libcef.lib
        ${ARCH_PREBUILT_DIRS_RELEASE}/libcef_dll_wrapper.lib
        ${ARCH_PREBUILT_DIRS_RELEASE}/dullahan.lib
    )
elseif (DARWIN)
    FIND_LIBRARY(APPKIT_LIBRARY AppKit)
    if (NOT APPKIT_LIBRARY)
        message(FATAL_ERROR "AppKit not found")
    endif()

    target_link_libraries( ll::cef INTERFACE
        ${ARCH_PREBUILT_DIRS_RELEASE}/libcef_dll_wrapper.a
        ${ARCH_PREBUILT_DIRS_RELEASE}/libdullahan.a
        ${APPKIT_LIBRARY}
       )

elseif (LINUX)
    target_link_libraries( ll::cef INTERFACE
        dullahan
        cef
        cef_dll_wrapper.a
    )
endif (WINDOWS)
