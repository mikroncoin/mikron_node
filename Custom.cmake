# UCM macros, taken from https://github.com/onqtam/ucm, 1.7.10. (MIT license)
# Used for (Win) static linking flags

# ucm_print_flags
# Prints all compiler flags for all configurations
macro(ucm_print_flags)
    ucm_gather_flags(1 flags_configs)
    message("")
    foreach(flags ${flags_configs})
        message("${flags}: ${${flags}}")
    endforeach()
    message("")
endmacro()

# ucm_gather_flags
# Gathers all lists of flags for printing or manipulation
macro(ucm_gather_flags with_linker result)
    set(${result} "")
    # add the main flags without a config
    list(APPEND ${result} CMAKE_C_FLAGS)
    list(APPEND ${result} CMAKE_CXX_FLAGS)
    if(${with_linker})
        list(APPEND ${result} CMAKE_EXE_LINKER_FLAGS)
        list(APPEND ${result} CMAKE_MODULE_LINKER_FLAGS)
        list(APPEND ${result} CMAKE_SHARED_LINKER_FLAGS)
        list(APPEND ${result} CMAKE_STATIC_LINKER_FLAGS)
    endif()

    if("${CMAKE_CONFIGURATION_TYPES}" STREQUAL "" AND NOT "${CMAKE_BUILD_TYPE}" STREQUAL "")
        # handle single config generators - like makefiles/ninja - when CMAKE_BUILD_TYPE is set
        string(TOUPPER ${CMAKE_BUILD_TYPE} config)
        list(APPEND ${result} CMAKE_C_FLAGS_${config})
        list(APPEND ${result} CMAKE_CXX_FLAGS_${config})
        if(${with_linker})
            list(APPEND ${result} CMAKE_EXE_LINKER_FLAGS_${config})
            list(APPEND ${result} CMAKE_MODULE_LINKER_FLAGS_${config})
            list(APPEND ${result} CMAKE_SHARED_LINKER_FLAGS_${config})
            list(APPEND ${result} CMAKE_STATIC_LINKER_FLAGS_${config})
        endif()
    else()
        # handle multi config generators (like msvc, xcode)
        foreach(config ${CMAKE_CONFIGURATION_TYPES})
            string(TOUPPER ${config} config)
            list(APPEND ${result} CMAKE_C_FLAGS_${config})
            list(APPEND ${result} CMAKE_CXX_FLAGS_${config})
            if(${with_linker})
                list(APPEND ${result} CMAKE_EXE_LINKER_FLAGS_${config})
                list(APPEND ${result} CMAKE_MODULE_LINKER_FLAGS_${config})
                list(APPEND ${result} CMAKE_SHARED_LINKER_FLAGS_${config})
                list(APPEND ${result} CMAKE_STATIC_LINKER_FLAGS_${config})
            endif()
        endforeach()
    endif()
endmacro()

# ucm_set_runtime
# Sets the runtime (static/dynamic) for msvc/gcc
macro(ucm_set_runtime)
    cmake_parse_arguments(ARG "STATIC;DYNAMIC" "" "" ${ARGN})

    if(ARG_UNPARSED_ARGUMENTS)
        message(FATAL_ERROR "unrecognized arguments: ${ARG_UNPARSED_ARGUMENTS}")
    endif()

    if(CMAKE_CXX_COMPILER_ID MATCHES "Clang" STREQUAL "")
        message(AUTHOR_WARNING "ucm_set_runtime() does not support clang yet!")
    endif()

    ucm_gather_flags(0 flags_configs)

    # add/replace the flags
    # note that if the user has messed with the flags directly this function might fail
    # - for example if with MSVC and the user has removed the flags - here we just switch/replace them
    if("${ARG_STATIC}")
        foreach(flags ${flags_configs})
            if(CMAKE_CXX_COMPILER_ID MATCHES "GNU")
                if(NOT ${flags} MATCHES "-static-libstdc\\+\\+")
                    set(${flags} "${${flags}} -static-libstdc++")
                endif()
                if(NOT ${flags} MATCHES "-static-libgcc")
                    set(${flags} "${${flags}} -static-libgcc")
                endif()
            elseif(MSVC)
                if(${flags} MATCHES "/MD")
                    string(REGEX REPLACE "/MD" "/MT" ${flags} "${${flags}}")
                endif()
            endif()
        endforeach()
    elseif("${ARG_DYNAMIC}")
        foreach(flags ${flags_configs})
            if(CMAKE_CXX_COMPILER_ID MATCHES "GNU")
                if(${flags} MATCHES "-static-libstdc\\+\\+")
                    string(REGEX REPLACE "-static-libstdc\\+\\+" "" ${flags} "${${flags}}")
                endif()
                if(${flags} MATCHES "-static-libgcc")
                    string(REGEX REPLACE "-static-libgcc" "" ${flags} "${${flags}}")
                endif()
            elseif(MSVC)
                if(${flags} MATCHES "/MT")
                    string(REGEX REPLACE "/MT" "/MD" ${flags} "${${flags}}")
                endif()
            endif()
        endforeach()
    endif()
endmacro()


option(DISABLE_NATIVE_ARCH "Disable the addition of -march=native" ON)

#set (RAIBLOCKS_SECURE_RPC TRUE)  # set in cmake command

include(CheckCXXCompilerFlag)

if (APPLE)
	CHECK_CXX_COMPILER_FLAG(-march=core2 COMPILER_SUPPORTS_MARCH_CORE2)
	if (COMPILER_SUPPORTS_MARCH_CORE2)
		set (CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -march=core2")
		set (CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -march=core2")
	endif ()
else ()
	CHECK_CXX_COMPILER_FLAG(-march=nocona COMPILER_SUPPORTS_MARCH_NOCONA)
	if (COMPILER_SUPPORTS_MARCH_NOCONA)
		set (CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -march=nocona -mno-sse3")
		set (CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -march=nocona -mno-sse3")
	endif ()
endif ()

CHECK_CXX_COMPILER_FLAG(-fvisibility=hidden COMPILER_SUPPORTS_VISIBILITY_HIDDEN)
if (COMPILER_SUPPORTS_VISIBILITY_HIDDEN)
	set (CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -fvisibility=hidden")
	set (CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fvisibility=hidden")
endif ()

if (APPLE)
	set (CMAKE_MACOSX_RPATH TRUE)
	set (CMAKE_OSX_DEPLOYMENT_TARGET 10.10)

	set (MACOSX_BUNDLE_BUNDLE_NAME "rai_node")
	set (MACOSX_BUNDLE_GUI_IDENTIFIER "com.nanowalletcompany.node")
	set (MACOSX_BUNDLE_SHORT_VERSION_STRING "${CPACK_PACKAGE_VERSION_MAJOR}.${CPACK_PACKAGE_VERSION_MINOR}.${CPACK_PACKAGE_VERSION_PATCH}")
	string(TIMESTAMP MACOSX_BUNDLE_BUNDLE_VERSION "+%Y%m%d%H%M%S" UTC)
	set (MACOSX_BUNDLE_COPYRIGHT "Copyright © 2018 Nano Wallet Company LLC. All rights reserved.")
	configure_file(Info.plist.in "CMakeFiles/Info.plist")

	set (CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -Wl,-sectcreate,__TEXT,__info_plist,${PROJECT_SOURCE_DIR}/CMakeFiles/Info.plist")
endif ()
