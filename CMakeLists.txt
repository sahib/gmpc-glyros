CMAKE_MINIMUM_REQUIRED(VERSION 2.6)
PROJECT(gmpc-glyros C)
INCLUDE(FindPkgConfig)

# Version
SET( GMPC_GLYROS_VERSION_MAJOR "0")
SET( GMPC_GLYROS_VERSION_MINOR "1")
CONFIGURE_FILE("src/config.h.in" "src/config.h")

# Default to /usr, use -DCMAKE_INSTALL_PREFIX="path" to change it
SET(CMAKE_INSTALL_PREFIX "/usr")

# Check deps 
PKG_CHECK_MODULES(GMPCPKG gmpc REQUIRED)
PKG_CHECK_MODULES(GLYRPKG libglyr REQUIRED)
PKG_CHECK_MODULES(GTK2PKG gtk+-2.0 REQUIRED)

ADD_DEFINITIONS(-Wall -W -Wno-unused-variable -Wno-unused-parameter -Wno-strict-prototypes)
INCLUDE_DIRECTORIES(${GTK2PKG_INCLUDE_DIRS})
INCLUDE_DIRECTORIES(${GMPCPKG_INCLUDE_DIRS})

# Compile shared library
ADD_LIBRARY(gmpc-glyros SHARED src/plugin.c)
TARGET_LINK_LIBRARIES(gmpc-glyros ${GLYRPKG_LIBRARIES}) 
TARGET_LINK_LIBRARIES(gmpc-glyros ${GTK2PKG_LIBRARIES}) 

INSTALL(FILES glyrosplugin.so DESTINATION lib/gmpc/plugins)
ADD_CUSTOM_COMMAND(
	TARGET gmpc-glyros 
	POST_BUILD
	COMMAND ${CMAKE_COMMAND} -E rename libgmpc-glyros.so glyrosplugin.so
)
