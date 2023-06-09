#
# Copyright (C) 2015, 2017, 2019, 2020  T+A elektroakustik GmbH & Co. KG
#
# This file is part of MounTA.
#
# This program is free software; you can redistribute it and/or
# modify it under the terms of the GNU General Public License
# as published by the Free Software Foundation; either version 2
# of the License, or (at your option) any later version.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with this program; if not, write to the Free Software
# Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston,
# MA  02110-1301, USA.
#

ACLOCAL_AMFLAGS = -I ../m4

bin_PROGRAMS = mounta

mounta_SOURCES = \
    mounta.cc messages.h messages.c backtrace.h backtrace.c \
    devices.hh devices_util.h device_manager.hh \
    devices_os.hh devices_os.cc \
    fdevents.hh fdevents.cc \
    automounter.hh automounter.cc \
    autodir.hh external_tools.hh \
    dbus_iface.c dbus_iface.h dbus_iface_deep.h \
    dbus_handlers.cc dbus_handlers.h \
    os.c os.h os.hh

DBUS_IFACES = $(top_srcdir)/dbus_interfaces

AM_CFLAGS = $(CWARNINGS)
AM_CXXFLAGS = $(CXXWARNINGS)

AM_CPPFLAGS = $(MOUNTA_DEPENDENCIES_CFLAGS)
AM_CPPFLAGS += -I$(DBUS_IFACES)

noinst_LTLIBRARIES = \
    libmounta_dbus.la \
    libdevice_manager.la

mounta_LDADD = $(noinst_LTLIBRARIES) $(MOUNTA_DEPENDENCIES_LIBS)

nodist_libmounta_dbus_la_SOURCES = de_tahifi_mounta.c de_tahifi_mounta.h
libmounta_dbus_la_CFLAGS = $(CRELAXEDWARNINGS)

libdevice_manager_la_SOURCES = \
    device_manager.hh device_manager.cc \
    devices.hh devices.cc \
    devices_util.h devices_util.c \
    autodir.cc autodir.hh
libdevice_manager_la_CFLAGS = $(AM_CFLAGS)
libdevice_manager_la_CXXFLAGS = $(AM_CXXFLAGS)

BUILT_SOURCES = \
    $(nodist_libmounta_dbus_la_SOURCES) \
    de_tahifi_mounta-doc.h \
    de_tahifi_mounta-doc.md \
    de_tahifi_mounta.stamp

CLEANFILES = $(BUILT_SOURCES)

de_tahifi_mounta-doc.h: de_tahifi_mounta.stamp
de_tahifi_mounta-doc.md: de_tahifi_mounta.stamp
de_tahifi_mounta.c: de_tahifi_mounta.stamp
de_tahifi_mounta.h: de_tahifi_mounta.stamp
de_tahifi_mounta.stamp: $(DBUS_IFACES)/de_tahifi_mounta.xml
	$(GDBUS_CODEGEN) --generate-c-code=de_tahifi_mounta --c-namespace tdbus --interface-prefix de.tahifi. $<
	$(DBUS_IFACES)/extract_documentation.py -i $< -o de_tahifi_mounta-doc.md -H de_tahifi_mounta-doc.h -c tdbus -s de.tahifi. -n "$(PACKAGE_NAME)"
	touch $@
