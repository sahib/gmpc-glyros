plugindir=${libdir}/gmpc/plugins/

plugin_LTLIBRARIES = glyrosplugin.la
INCLUDES = -DG_LOG_DOMAIN=\"GlyrosPlugin\"

lastfmplugin_la_SOURCES=	\
	plugin.c 


lastfmplugin_la_LIBADD= @gmpcglyros_LIBS@ 
lastfmplugin_la_CFLAGS= @gmpcglyros_CFLAGS@  -Wall -Wmissing-declarations
lastfmplugin_la_LDFLAGS= -module -avoid-version 
