#
# Copyright (C) 2015, 2019  T+A elektroakustik GmbH & Co. KG
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

if WITH_CUTTER
TESTS = run_test.sh

if WITH_VALGRIND
TESTS += valgrind.sh
endif

AM_TESTS_ENVIRONMENT = CUTTER="$(CUTTER)" XSLTPROC="$(XSLTPROC)" VALGRIND="$(VALGRIND)"

AM_LDFLAGS = -module -rpath $(libdir) -avoid-version -no-undefined

DBUS_IFACES = $(top_srcdir)/dbus_interfaces

AM_CPPFLAGS = $(CPPCUTTER_CFLAGS)
AM_CPPFLAGS += -I$(top_srcdir)/src -I$(top_builddir)/src
AM_CPPFLAGS += -I$(DBUS_IFACES)

AM_CFLAGS = $(CWARNINGS)

AM_CXXFLAGS = $(CXXWARNINGS)

LIBS += $(CPPCUTTER_LIBS)

check_LTLIBRARIES = \
    test_device_manager.la

test_device_manager_la_SOURCES = \
    test_device_manager.cc mock_expectation.hh \
    mock_devices_os.hh mock_devices_os.cc \
    mock_messages.hh mock_messages.cc \
    mock_os.hh mock_os.cc
test_device_manager_la_LIBADD = $(top_builddir)/src/libdevice_manager.la
test_device_manager_la_CFLAGS = $(AM_CFLAGS) $(MOUNTA_DEPENDENCIES_CFLAGS)
test_device_manager_la_CXXFLAGS = $(AM_CXXFLAGS) $(MOUNTA_DEPENDENCIES_CFLAGS)

CLEANFILES = test_report.xml test_report_junit.xml valgrind.xml

EXTRA_DIST = cutter2junit.xslt
EXTRA_DIST += cutter-1_2_4.supp

check-local: check-TESTS
	@if $(GREP) -w cutter $(TEST_LOGS); then \
	    echo "Unit tests failed (check log)"; \
	    exit 1; \
	fi

endif
