CMAKE_MINIMUM_REQUIRED( VERSION 2.6 )

PROJECT(colorgroup)

SET(CMAKE_BUILD_TYPE Release)

set(Sources
	colorgroup.c
	mtwist.c
)

add_executable(colorgroup ${Sources})
find_package(PNG REQUIRED)
include_directories(${PNG_INCLUDE_DIR})
target_link_libraries(colorgroup ${PNG_LIBRARY})
