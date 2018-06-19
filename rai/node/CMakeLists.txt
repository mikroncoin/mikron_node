if (RAIBLOCKS_SECURE_RPC)
	set (secure_rpc_sources rpc_secure.cpp rpc_secure.hpp)
endif ()

if (${CMAKE_SYSTEM_NAME} MATCHES "Darwin")
	# No opencl
elseif (${CMAKE_SYSTEM_NAME} MATCHES "Windows")
	set (platform_sources plat/windows/openclapi.cpp)
elseif (${CMAKE_SYSTEM_NAME} MATCHES "Linux")
	set (platform_sources plat/posix/openclapi.cpp)
elseif (${CMAKE_SYSTEM_NAME} MATCHES "FreeBSD")
	set (platform_sources plat/posix/openclapi.cpp)
else ()
	error ("Unknown platform: ${CMAKE_SYSTEM_NAME}")
endif ()

add_library (node
	${platform_sources}
	${secure_rpc_sources}
	bootstrap.cpp
	bootstrap.hpp
	cli.hpp
	cli.cpp
	common.cpp
	common.hpp
	node.hpp
	node.cpp
	openclwork.cpp
	openclwork.hpp
	rpc.hpp
	rpc.cpp
	testing.hpp
	testing.cpp
	wallet.hpp
	wallet.cpp
	stats.hpp
	stats.cpp
	working.hpp
	xorshift.hpp)

target_link_libraries (node
	lmdb
	ed25519
	rai_lib
	libminiupnpc-static
	argon2
	Boost::filesystem
	Boost::log
	Boost::log_setup
	Boost::program_options
	Boost::system
	Boost::thread
	Boost::boost
	)