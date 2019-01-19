add_executable (mikron_node
	daemon.cpp
	daemon.hpp
	entry.cpp)

target_link_libraries (mikron_node
	node
	secure
	argon2
	Boost::boost
	${PLATFORM_LIBS}
)

target_compile_definitions(mikron_node
	PRIVATE
		-DRAIBLOCKS_VERSION_MAJOR=${CPACK_PACKAGE_VERSION_MAJOR}
		-DRAIBLOCKS_VERSION_MINOR=${CPACK_PACKAGE_VERSION_MINOR}
		-DRAIBLOCKS_VERSION_PATCH=${CPACK_PACKAGE_VERSION_PATCH})

set_target_properties (mikron_node
	PROPERTIES
		COMPILE_FLAGS
			"-DQT_NO_KEYWORDS -DBOOST_ASIO_HAS_STD_ARRAY=1")

if (RAIBLOCKS_GUI AND NOT APPLE)
	install(TARGETS mikron_node
			RUNTIME DESTINATION bin
			LIBRARY DESTINATION lib
			ARCHIVE DESTINATION lib
	)
endif()
