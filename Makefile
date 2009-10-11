# This makefile is inspired by abuild (http://www.abuild.org), which
# was used during the development of qpdf.  The goal here is to have a
# non-recursive build with all the proper dependencies so we can start
# the build from anywhere and get the right stuff.  Each directory has
# a build.mk that is included from here and is written from this
# directory's perspective.  Each directory also has a proxy Makefile
# that allows you to start the build from any directory and get
# reasonable semantics for the all, check, and clean targets.

# Our "build items" are directories.  They are listed here such that
# no item precedes any item it depends on.  Therefore, each item can
# safely reference variables set in its predecessors.

# For each build item B, you can run make build_B, make check_B, or
# make clean_B to build, test, or clean B.  Full dependencies are
# represented across all the items, so it is possible to start
# anywhere.  From the top level, the "all", "check", and "clean"
# targets build, test, or clean everything.

# Although this is not a GNU package and does not use automake, you
# can still run make clean to remove everything that is compiled, make
# distclean to remove everything that is generated by the end user,
# and make maintainer-clean to remove everything that is generated
# including things distributed with the source distribution.  You can
# pass CLEAN=1 to prevent this Makefile from complaining if
# ./configure has not been run.  If you are cleaning from Windows, you
# may also need to add BUILD_EXTERNAL_LIBS=1 to make sure you can that
# as well.

# The install target works as usual and obeys --prefix and so forth
# passed to ./configure.  You can also pass DESTDIR=/dir to make
# install to install in a separate location.  This is useful for
# packagers.

BUILD_ITEMS := manual libqpdf zlib-flate libtests qpdf examples
OUTPUT_DIR = build
ALL_TARGETS =

.PHONY: default
default: all

CLEAN ?=
ifneq ($(CLEAN),1)
ifeq ($(words $(wildcard autoconf.mk)),0)
DUMMY := $(shell echo 1>&2)
DUMMY := $(shell echo 1>&2 Please run ./configure before running $(MAKE))
DUMMY := $(shell echo 1>&2)
$(error unable to continue with build)
endif

autoconf.mk:

include autoconf.mk

endif

ifeq ($(BUILD_EXTERNAL_LIBS),1)
  BUILD_ITEMS := external-libs $(BUILD_ITEMS)
endif

# Prevent gnu make from trying to rebuild .dep files
$(foreach B,$(BUILD_ITEMS),$(eval \
  $(B)/$(OUTPUT_DIR)/%.dep: ;))

# Prevent gnu make from trying to rebuild .mk files
$(foreach B,$(BUILD_ITEMS),$(eval \
  $(B)/%.mk: ;))
%.mk: ;
make/%.mk: ;

BUILDRULES ?= libtool
include make/rules.mk

DUMMY := $(shell mkdir $(foreach B,$(BUILD_ITEMS),$(B)/$(OUTPUT_DIR)) 2>/dev/null)

include $(foreach B,$(BUILD_ITEMS),$(B)/build.mk)

ALL_TARGETS = $(foreach B,$(BUILD_ITEMS),$(TARGETS_$(B)))

TEST_ITEMS = $(foreach D,\
                 $(wildcard $(foreach B,$(BUILD_ITEMS),$(B)/qtest)),\
                 $(subst /,,$(dir $(D))))

TEST_TARGETS = $(foreach B,$(TEST_ITEMS),check_$(B))

CLEAN_TARGETS = $(foreach B,$(BUILD_ITEMS),clean_$(B))

# For test suitse
export QPDF_BIN = $(abspath qpdf/$(OUTPUT_DIR)/qpdf)
export SKIP_TEST_COMPARE_IMAGES

clean: $(CLEAN_TARGETS)

.PHONY: $(CLEAN_TARGETS)
$(foreach B,$(BUILD_ITEMS),$(eval \
  clean_$(B): ; \
	$(RM) -r $(B)/$(OUTPUT_DIR)))

distclean: clean
	$(RM) -r autoconf.mk autom4te.cache config.log config.status libtool
	$(RM) libqpdf/qpdf/qpdf-config.h
	$(RM) manual/html.xsl
	$(RM) manual/print.xsl
	$(RM) doc/*.1

maintainer-clean: distclean
	$(RM) configure doc/qpdf-manual.* libqpdf/qpdf/qpdf-config.h.in
	$(RM) aclocal.m4

.PHONY: $(TEST_TARGETS)
$(foreach B,$(TEST_ITEMS),$(eval \
  check_$(B): $(TARGETS_$(B))))

.PHONY: $(foreach B,$(BUILD_ITEMS),build_$(B))
$(foreach B,$(BUILD_ITEMS),$(eval \
  build_$(B): $(TARGETS_$(B))))

.PHONY: all
all: $(ALL_TARGETS) ;

check: $(TEST_TARGETS)

install_docs::
install: all
	./mkinstalldirs $(DESTDIR)$(libdir)
	./mkinstalldirs $(DESTDIR)$(bindir)
	./mkinstalldirs $(DESTDIR)$(includedir)/qpdf
	./mkinstalldirs $(DESTDIR)$(docdir)
	./mkinstalldirs $(DESTDIR)$(mandir)/man1
	$(LIBTOOL) --mode=install install -s -c \
		libqpdf/$(OUTPUT_DIR)/libqpdf.la \
		$(DESTDIR)$(libdir)/libqpdf.la
	$(LIBTOOL) --finish $(DESTDIR)$(libdir)
	$(LIBTOOL) --mode=install install -s -c \
		qpdf/$(OUTPUT_DIR)/qpdf \
		$(DESTDIR)$(bindir)/qpdf
	$(LIBTOOL) --mode=install install -s -c \
		zlib-flate/$(OUTPUT_DIR)/zlib-flate \
		$(DESTDIR)$(bindir)/zlib-flate
	cp qpdf/fix-qdf $(DESTDIR)$(bindir)
	cp include/qpdf/*.hh $(DESTDIR)$(includedir)/qpdf
	cp doc/stylesheet.css $(DESTDIR)$(docdir)
	cp doc/qpdf-manual.html $(DESTDIR)$(docdir)
	cp doc/qpdf-manual.pdf $(DESTDIR)$(docdir)
	cp doc/*.1 $(DESTDIR)$(mandir)/man1

QTEST=$(abspath qtest/bin/qtest-driver)
$(TEST_TARGETS):
	@echo running qtest-driver for $(subst check_,,$@)
	@(cd $(subst check_,,$@)/$(OUTPUT_DIR); \
         if TC_SRCS="$(foreach T,$(TC_SRCS_$(subst check_,,$@)),../../$(T))" \
	 $(QTEST) -bindirs .:.. -datadir ../qtest -covdir ..; then \
	    true; \
	 else \
	    cat -v qtest.log; \
	    false; \
	 fi)
