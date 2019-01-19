add_executable (mikron_storetool
	block_span_analyzer.hpp
	block_span_analyzer.cpp
	blockstore_tool.hpp
	blockstore_tool.cpp
	entry.cpp)

target_link_libraries (mikron_storetool
	node
	secure
	argon2
	Boost::boost
	${PLATFORM_LIBS}
)

target_compile_definitions(mikron_storetool
	PRIVATE
		-DRAIBLOCKS_VERSION_MAJOR=${CPACK_PACKAGE_VERSION_MAJOR}
		-DRAIBLOCKS_VERSION_MINOR=${CPACK_PACKAGE_VERSION_MINOR}
		-DRAIBLOCKS_VERSION_PATCH=${CPACK_PACKAGE_VERSION_PATCH})

set_target_properties (mikron_storetool
	PROPERTIES
		COMPILE_FLAGS
			"-DQT_NO_KEYWORDS -DBOOST_ASIO_HAS_STD_ARRAY=1")

if (RAIBLOCKS_GUI AND NOT APPLE)
	install(TARGETS mikron_storetool
			RUNTIME DESTINATION bin
			LIBRARY DESTINATION lib
			ARCHIVE DESTINATION lib
	)
endif()
