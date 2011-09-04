CMAKE_MINIMUM_REQUIRED(VERSION 2.6)
PROJECT(gmpc-glyros C)
INCLUDE(FindPkgConfig)

FIND_PACKAGE(GTK2 REQUIRED)

SET( GMPC_GLYROS_VERSION_MAJOR "0")
SET( GMPC_GLYROS_VERSION_MINOR "1")

CONFIGURE_FILE(
		"src/config.h.in"
		"src/config.h"
     )

# Default to /usr, use -DCMAKE_INSTALL_PREFIX="path" to change it
SET(CMAKE_INSTALL_PREFIX "/usr")
PKG_CHECK_MODULES(GMPCPKG gmpc REQUIRED)

ADD_DEFINITIONS(-Wall -W -Wno-unused-variable -Wno-unused-parameter -Wno-strict-prototypes)
INCLUDE_DIRECTORIES(${GMPCPKG_INCLUDE_DIRS})

ADD_LIBRARY(gmpc-glyros SHARED src/plugin.c)
TARGET_LINK_LIBRARIES(gmpc-glyros glyr) # TODO 
TARGET_LINK_LIBRARIES(gmpc-glyros ${GTK2_LIBRARIES}) 

# Im pretty sure there's a nice way..
ADD_CUSTOM_COMMAND (
		TARGET gmpc-glyros 
		POST_BUILD
		COMMAND ${CMAKE_COMMAND} -E rename libgmpc-glyros.so glyrosplugin.so
)
INSTALL(FILES glyrosplugin.so DESTINATION lib/gmpc/plugins)
