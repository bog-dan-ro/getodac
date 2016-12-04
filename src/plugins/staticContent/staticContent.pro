TEMPLATE = lib

CONFIG += plugin c++14
TARGET = staticFiles

VERSION =
QT =

INCLUDEPATH += $$PWD/../../include

DESTDIR = ../../../plugins

QMAKE_LFLAGS += -Wl,--no-undefined

QMAKE_CFLAGS += -Wall -Wextra -Werror
QMAKE_CXXFLAGS += -Wall -Wextra -Werror -fnon-call-exceptions

#release {
#    QMAKE_CXXFLAGS += -Ofast -fdevirtualize -fdevirtualize-speculatively -fdevirtualize-at-ltrans
#    QMAKE_CFLAGS += -Ofast -fdevirtualize -fdevirtualize-speculatively -fdevirtualize-at-ltrans
#}

LIBS += -lboost_coroutine -lboost_context -lboost_system -lboost_filesystem -lboost_iostreams

#!android {
#    SANITIZE = -fsanitize=address -fsanitize=unreachable -fsanitize=bounds -fsanitize=object-size -fsanitize=enum
#    QMAKE_CXXFLAGS += -fno-omit-frame-pointer $$SANITIZE
#    QMAKE_CFLAGS   += -fno-omit-frame-pointer $$SANITIZE
#    QMAKE_LFLAGS += $$SANITIZE
#}

#!android {
#    QMAKE_CXXFLAGS += -fsanitize=thread -fno-omit-frame-pointer
#    QMAKE_CFLAGS += -fsanitize=thread -fno-omit-frame-pointer
#    QMAKE_LFLAGS += -fsanitize=thread
#}

SOURCES += \
    staticContentPlugin.cpp
