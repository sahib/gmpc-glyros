cmake_minimum_required(VERSION 2.6)
PROJECT(gmpc-glyros C)

SET( GMPC_GLYROS_VERSION_MAJOR "0")
SET( GMPC_GLYROS_VERSION_MINOR "1")

message("-- Make sure to install both gmpc-dev/header and libglyr first")
message("-- For simplictiy reasons those are not checked to be installed.")
message("-- Compiling will result in errors otherwise.")

FIND_PACKAGE(GTK2 REQUIRED)

CONFIGURE_FILE(
		"src/config.h.in"
		"src/config.h"
	      )

SET(CMAKE_INSTALL_PREFIX "/usr")

ADD_DEFINITIONS(-Wall -W -Wno-unused-variable -Wno-unused-parameter -Wno-strict-prototypes)
INCLUDE_DIRECTORIES(${GTK2_INCLUDE_DIRS})
INCLUDE_DIRECTORIES("/usr/include/libmpd-1.0")
INCLUDE_DIRECTORIES("/usr/include/gmpc")
ADD_LIBRARY(gmpc-glyros SHARED src/plugin.c)
TARGET_LINK_LIBRARIES(gmpc-glyros glyr) 
TARGET_LINK_LIBRARIES(gmpc-glyros ${GTK2_LIBRARIES}) 
ADD_CUSTOM_COMMAND (
		TARGET gmpc-glyros 
		POST_BUILD
		COMMAND ${CMAKE_COMMAND} -E rename libgmpc-glyros.so glyrosplugin.so
)
INSTALL(FILES glyrosplugin.so DESTINATION lib/gmpc/plugins)
