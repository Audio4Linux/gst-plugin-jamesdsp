# Note: plugindir is set in configure

plugin_LTLIBRARIES = libgstjdspfx.la

# sources used to compile this plug-in
libgstjdspfx_la_SOURCES = gstjdspfx.cpp \
    gstjdspfx.h \
    EffectDSPMain.h \
    Effect.h \
    EffectDSPMain.cpp \
    Effect.cpp \
    kissfft/kiss_fft.c \
    kissfft/kiss_fftr.c \
    gstinterface.h \
    JLimiter.c \
    reverb.c \
    compressor.c \
    AutoConvolver.c \
    mnspline.c \
    ArbFIRGen.c \
    vdc.c \
    bs2b.c \
    valve/12ax7amp/Tube.c \
    valve/12ax7amp/wdfcircuits_triode.c


# compiler and linker flags used to compile this plugin, set in configure.ac
libgstjdspfx_la_CXXFLAGS = $(GST_CFLAGS)
libgstjdspfx_la_CFLAGS = $(GST_CFLAGS)
libgstjdspfx_la_LIBADD = $(GST_LIBS)
libgstjdspfx_la_LDFLAGS = $(GST_PLUGIN_LDFLAGS) -lsamplerate -lsndfile -rdynamic -ldl -Wl --gc-sections
libgstjdspfx_la_LIBTOOLFLAGS = --tag=disable-static

# headers we need but don't want installed
noinst_HEADERS = gstjdspfx.h
