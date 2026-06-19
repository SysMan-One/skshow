cmake_minimum_required(VERSION 3.16)

project(sockshow LANGUAGES C)


#++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
set (3RDPARTY_ROOT      3rdparty/ )
set (STARLET_DIR       ${3RDPARTY_ROOT}/utility_routines/ )
add_subdirectory(${STARLET_DIR})					# UTILITY$ROUTINES

add_definitions( -D__ARCH__NAME__="${CMAKE_HOST_SYSTEM_PROCESSOR}" )
add_definitions( -D_REENTRANT=1 )
#cmake_print_variables(CMAKE_CURRENT_SOURCE_DIR)



string(TOLOWER ${CMAKE_BUILD_TYPE} build_type)                          # Additional definitons for DEBUG builds
if (build_type STREQUAL debug)
        add_definitions( -D_DEBUG=1 -D__TRACE__=1  -pg )		# Add "-w" to disable warnings
else ()
        add_definitions( -w  )						# Add "-w" to disable warnings
endif()
#--------------------------------------------------------------------------------------------------------------------------------------


add_executable(sockshow sockshow.c README.md)
include_directories (
        ${STARLET_DIR}
        ./)
target_link_libraries(sockshow starlet)
