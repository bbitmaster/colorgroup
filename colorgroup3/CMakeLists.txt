CMAKE_MINIMUM_REQUIRED( VERSION 2.6 )

PROJECT(colorgroup)

SET(CMAKE_BUILD_TYPE Release)

set(Sources
	colorgroup.c
	mtwist.c
)

#add_definitions(-std=c99)
add_executable(colorgroup ${Sources})
find_package(PNG REQUIRED)
find_package(SDL REQUIRED)
include_directories(${PNG_INCLUDE_DIR} ${SDL_INCLUDE_DIR})
target_link_libraries(colorgroup m ${PNG_LIBRARY} ${SDL_LIBRARY})
