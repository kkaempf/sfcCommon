# $Id: Makefile.am,v 1.105 2010/02/10 20:51:15 buccella Exp $  
#
#  Makefile.am
# 
#   © Copyright IBM Corp. 2005, 2007
# 
#  THIS FILE IS PROVIDED UNDER THE TERMS OF THE ECLIPSE PUBLIC LICENSE
#  ("AGREEMENT"). ANY USE, REPRODUCTION OR DISTRIBUTION OF THIS FILE
#  CONSTITUTES RECIPIENTS ACCEPTANCE OF THE AGREEMENT.
# 
#  You can obtain a current copy of the Eclipse Public License from
#  http://www.opensource.org/licenses/eclipse-1.0.php
# 
#  Author:        Viktor Mihajlovski <mihajlov@de.ibm.com>
#  Contributions: Adrian Schuur <schuur@de.ibm.com>
#                 Sven Schuetz <sven@de.ibm.com>
# 
#  Description:
# 
#  Makefile process input for sfcb.
# 
# 

sfcblibdir=$(libdir)/sfcb
#sfcCommonlibdir=$(libdir)/sfcCommon
sfcCommonlibdir=$(libdir)
#instdir=/usr/local/include/sfcCommon
instdir=$(includedir)/sfcCommon

AM_YFLAGS=-d
AM_CPPFLAGS= -DLIBDIR=\"$(libdir)\" 
#AM_CPPFLAGS= -DSFCB_LIBDIR=\"$(sfcblibdir)\" \
#	-DLIBDIR=\"$(libdir)\" 
#AM_LDFLAGS=@SFCB_LDFLAGS@

SUBDIRS=. 

#sfcb_sharedobjects = \
#   $(sfcblibdir)/*.so

sfcCommonlib_LTLIBRARIES = \
   libsfcUtil.la 

libsfcUtil_la_SOURCES = \
        sfcUtil/genericlist.c \
	sfcUtil/hashtable.c \
	sfcUtil/utilFactory.c \
	sfcUtil/utilHashtable.c \
	sfcUtil/utilStringBuffer.c \
	sfcUtil/utilTypeCk.c \
	sfcUtil/libsfcUtil.Versions
libsfcUtil_la_LDFLAGS = -Wl,--version-script,$(srcdir)/sfcUtil/libsfcUtil.Versions

inst_HEADERS= sfcUtil/hashtable.h sfcUtil/utilft.h sfcUtil/genericlist.h

pretty:
	for i in `find $(srcdir) -name \*.[ch]`; do \
          sed -i '/\/\* MODELINES \*\//,$$ d' $$i ; \
          indent $$i ; \
	  cat extra/modelines >> $$i ; \
	done;
