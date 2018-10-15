TEMPLATE = lib

CONFIG += plugin c++14
TARGET = staticFiles

CONFIG(debug, debug|release): CONFIG += sanitizer sanitize_address sanitize_undefined
else: {
    QMAKE_CFLAGS += -Ofast
    QMAKE_CXXFLAGS += -Ofast
}

VERSION =
QT =

INCLUDEPATH += $$PWD/../../../include

DESTDIR = ../../../plugins

QMAKE_LFLAGS += -Wl,--no-undefined

QMAKE_CFLAGS += -Wall -Wextra -Werror
QMAKE_CXXFLAGS += -Wall -Wextra -Werror -fnon-call-exceptions

LIBS += -lboost_coroutine -lboost_context -lboost_system -lboost_filesystem -lboost_iostreams

SOURCES += \
    staticContentPlugin.cpp
