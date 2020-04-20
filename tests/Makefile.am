#
# Copyright (C) 2015, 2019, 2020  T+A elektroakustik GmbH & Co. KG
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

if WITH_DOCTEST
check_PROGRAMS = test_device_manager

TESTS = run_tests.sh

if WITH_VALGRIND
AM_TESTS_ENVIRONMENT =  VALGRIND="$(VALGRIND)"
AM_TESTS_ENVIRONMENT += VALGRIND_OPTIONS="--suppressions=$(srcdir)/valgrind.suppressions"
TESTS += valgrind.sh
endif

EXTRA_DIST = run_tests.sh valgrind.sh valgrind.suppressions
CLEANFILES = *.junit.xml *.valgrind.xml

AM_CPPFLAGS = -DDOCTEST_CONFIG_TREAT_CHAR_STAR_AS_STRING
AM_CPPFLAGS += -I$(top_srcdir)/src -I$(top_builddir)/src
AM_CPPFLAGS += -I$(top_srcdir)/dbus_interfaces
AM_CFLAGS = $(CWARNINGS)
AM_CXXFLAGS = $(CXXWARNINGS)

noinst_LTLIBRARIES = libtestrunner.la

libtestrunner_la_SOURCES = testrunner.cc

test_device_manager_SOURCES = \
    test_device_manager.cc \
    mock_devices_os.hh mock_devices_os.cc \
    mock_messages.hh mock_messages.cc \
    mock_os.hh mock_os.cc \
    mock_backtrace.hh mock_backtrace.cc \
    mock_expectation.hh
test_device_manager_LDADD = \
    libtestrunner.la \
    $(top_builddir)/src/libdevice_manager.la
test_device_manager_CPPFLAGS = $(AM_CPPFLAGS) $(MOUNTA_DEPENDENCIES_CFLAGS)
test_device_manager_CXXFLAGS = $(AM_CXXFLAGS) $(MOUNTA_DEPENDENCIES_CFLAGS)

doctest: $(check_PROGRAMS)
	for p in $(check_PROGRAMS); do \
	    if ./$$p $(DOCTEST_EXTRA_OPTIONS); then :; \
	    else \
	        echo "*****"; \
	        echo "***** Running $$p again in gdb due to failures"; \
	        echo "*****"; \
	        gdb -ex run --args ./$$p $(DOCTEST_EXTRA_OPTIONS); \
	    fi; \
	done

doctest-valgrind: $(check_PROGRAMS)
	for p in $(check_PROGRAMS); do $(VALGRIND) --leak-check=full --show-reachable=yes --error-limit=no ./$$p $(DOCTEST_EXTRA_OPTIONS); done
endif
