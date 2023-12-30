#
# Copyright(c) 2011-2014 Renesas Electronics Corporation
# RENESAS ELECTRONICS CONFIDENTIAL AND PROPRIETARY.
# This program must be used solely for the purpose for which
# it was furnished by Renesas Electronics Corporation. No part of this
# program may be reproduced or disclosed to others, in any
# form, without the prior written permission of Renesas Electronics
# Corporation.
#

PROG       = OES3_Texture_Pixmap_EGLImage

# CC         = $(CROSS_COMPILE)gcc
# CXX        = $(CROSS_COMPILE)g++

TARGET     = $(PROG)
OBJS       = $(PROG).o

PKGROOT ?= 

# CFLAGS     = -I$(PKGROOT)/usr/include
# CFLAGS    += $(LIBDRM_INCLUDE_FLAGS)
# LDFLAGS    = -L$(PKGROOT)/usr/lib -Xlinker -rpath-link=$(PKGROOT)/usr/lib
# LDFLAGS   += $(LIBDRM_LIBRARY_FLAGS)

LIBS       = -lEGL -lGLESv2 -lpthread

# for libdrm
DRMPKG	= libdrm libkms
CFLAGS	+= $(shell pkg-config --cflags-only-I $(DRMPKG))
LDFLAGS	+= $(shell pkg-config --libs-only-L $(DRMPKG))
LIBS	+= $(shell pkg-config --libs-only-l $(DRMPKG))

.c.o:
	$(CC) -c $(INCLUDE) $(CFLAGS) $*.c

.cpp.o:
	$(CXX) -c $(INCLUDE) $(CFLAGS) $*.cpp

all:    $(TARGET)

$(TARGET): $(OBJS)
	$(CXX) -o $@ $(OBJS) $(LDFLAGS) $(LIBS)

$(OBJS) : $(HEADS)

clean :
	-$(RM) $(OBJS)
	-$(RM) $(TARGET)

