AM_CFLAGS = \
	$(WARNING_CFLAGS) \
	$(CODE_COVERAGE_CFLAGS)

AM_LDFLAGS = \
	$(CODE_COVERAGE_LIBS)

AM_CPPFLAGS = \
	-I$(top_srcdir) \
	-I$(top_srcdir)/src/include \
	-I$(top_builddir)/src/common/libflux \
	$(ZMQ_CFLAGS) $(JANSSON_CFLAGS)

fluxmod_LTLIBRARIES = job-manager.la

job_manager_la_SOURCES = job-manager.c \
			 job.c \
			 job.h \
			 queue.c \
			 queue.h \
			 active.h \
			 active.c \
			 purge.h \
			 purge.c \
			 list.h \
			 list.c \
			 priority.h \
			 priority.c

job_manager_la_LDFLAGS = $(fluxmod_ldflags) -module
job_manager_la_LIBADD = $(fluxmod_libadd) \
		    $(top_builddir)/src/common/libflux-internal.la \
		    $(top_builddir)/src/common/libflux-core.la \
		    $(top_builddir)/src/common/libflux-optparse.la \
		    $(ZMQ_LIBS)

TESTS = \
	test_queue.t \
	test_list.t

test_ldadd = \
	$(top_builddir)/src/common/libtap/libtap.la \
	$(top_builddir)/src/common/libflux-internal.la \
	$(top_builddir)/src/common/libflux-core.la \
	$(ZMQ_LIBS) $(LIBPTHREAD) $(JANSSON_LIBS)

test_cppflags = \
	$(AM_CPPFLAGS)

check_PROGRAMS = $(TESTS)

TEST_EXTENSIONS = .t
T_LOG_DRIVER = env AM_TAP_AWK='$(AWK)' $(SHELL) \
       $(top_srcdir)/config/tap-driver.sh

test_queue_t_SOURCES = test/queue.c
test_queue_t_CPPFLAGS = $(test_cppflags)
test_queue_t_LDADD = \
        $(top_builddir)/src/modules/job-manager/queue.o \
        $(top_builddir)/src/modules/job-manager/job.o \
        $(test_ldadd)

test_list_t_SOURCES = test/list.c
test_list_t_CPPFLAGS = $(test_cppflags)
test_list_t_LDADD = \
        $(top_builddir)/src/modules/job-manager/list.o \
        $(top_builddir)/src/modules/job-manager/queue.o \
        $(top_builddir)/src/modules/job-manager/job.o \
        $(test_ldadd)
