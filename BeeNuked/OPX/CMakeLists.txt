set(YMF271_INCLUDE_DIR "${CMAKE_CURRENT_SOURCE_DIR}")

set(YMF271_SOURCES
	ymf271.cpp)

set(YMF271_HEADERS
	ymf271.h)

add_library(ymf271 STATIC ${YMF271_SOURCES} ${YMF271_HEADERS})
target_include_directories(ymf271 PUBLIC
	${YMF271_INCLUDE_DIR})