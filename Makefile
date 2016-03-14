# contrib/pg_grab_statement/Makefile

EXTENSION = pg_grab_statement
EXTVERSION = 1.0

MODULE_big = $(EXTENSION)
OBJS = $(EXTENSION).o $(WIN32RES)

DATA = $(EXTENSION)--$(EXTVERSION).sql
PGFILEDESC = "$(EXTENSION) - dump SQL statements"

ifdef USE_PGXS
PG_CONFIG = pg_config
PGXS := $(shell $(PG_CONFIG) --pgxs)
include $(PGXS)
else
subdir = contrib/$(EXTENSION)
top_builddir = ../..
include $(top_builddir)/src/Makefile.global
include $(top_srcdir)/contrib/contrib-global.mk
endif
