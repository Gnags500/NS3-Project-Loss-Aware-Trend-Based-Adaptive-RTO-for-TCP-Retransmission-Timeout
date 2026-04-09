# scratch/submission_checklist/CMakeLists.txt
#
# Registers the wired and wireless simulation programs with ns-3's build system.
# This file is required because ns-3's scratch discovery only goes one level deep.
# The create_scratch() function is defined in scratch/CMakeLists.txt and is
# available here because add_subdirectory() propagates function scope.

file(GLOB wired_sources    CONFIGURE_DEPENDS
     ${CMAKE_CURRENT_SOURCE_DIR}/wired/[^.]*.cc)

file(GLOB wireless_sources CONFIGURE_DEPENDS
     ${CMAKE_CURRENT_SOURCE_DIR}/wireless/[^.]*.cc)

if(wired_sources)
    create_scratch("${wired_sources}")
endif()

if(wireless_sources)
    create_scratch("${wireless_sources}")
endif()
