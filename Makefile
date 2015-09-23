# contrib/pg_grab_statement/Makefile

MODULE_big = pg_grab_statement
OBJS = pg_grab_statement.o $(WIN32RES)

EXTENSION = pg_grab_statement
DATA = pg_grab_statement--1.0.sql
PGFILEDESC = "pg_grab_statement - dump SQL statements"

ifdef USE_PGXS
PG_CONFIG = pg_config
PGXS := $(shell $(PG_CONFIG) --pgxs)
include $(PGXS)
else
subdir = contrib/pg_grab_statement
top_builddir = ../..
include $(top_builddir)/src/Makefile.global
include $(top_srcdir)/contrib/contrib-global.mk
endif
