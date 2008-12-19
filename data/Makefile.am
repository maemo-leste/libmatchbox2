SUBDIRS=themes

pcfiles = libmatchbox2-$(MBWM2_API_VERSION).pc \
	  libmatchbox2.pc

# clutter-<major>.<minor>.pc - for generic dependencies
libmatchbox2-$(MBWM2_API_VERSION).pc: libmatchbox2.pc
	@cp $< $@

if ENABLE_LIBMATCHBOX
pkgconfigdir = $(libdir)/pkgconfig
pkgconfig_DATA = $(pcfiles)
endif
