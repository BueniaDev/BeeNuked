set(YM3526_INCLUDE_DIR "${CMAKE_CURRENT_SOURCE_DIR}")

set(YM3526_SOURCES
	ym3526.cpp)

set(YM3526_HEADERS
	ym3526.h)

add_library(ym3526 STATIC ${YM3526_SOURCES} ${YM3526_HEADERS})
target_include_directories(ym3526 PUBLIC
	${YM3526_INCLUDE_DIR})