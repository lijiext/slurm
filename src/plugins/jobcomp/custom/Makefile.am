# Makefile for jobcomp/custom plugin

AUTOMAKE_OPTIONS = foreign

PLUGIN_FLAGS = -module -avoid-version --export-dynamic

AM_CPPFLAGS = -DSLURM_PLUGIN_DEBUG -I$(top_srcdir) -I$(top_srcdir)/src/common

pkglib_LTLIBRARIES = jobcomp_custom.la

# Custom JSON job completion logging plugin.
jobcomp_custom_la_SOURCES = jobcomp_custom.c
jobcomp_custom_la_LDFLAGS = $(PLUGIN_FLAGS)
