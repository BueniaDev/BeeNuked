set(YM2151_INCLUDE_DIR "${CMAKE_CURRENT_SOURCE_DIR}")

set(YM2151_SOURCES
	ym2151.cpp)

set(YM2151_HEADERS
	ym2151.h)

add_library(ym2151 STATIC ${YM2151_SOURCES} ${YM2151_HEADERS})
target_include_directories(ym2151 PUBLIC
	${YM2151_INCLUDE_DIR})