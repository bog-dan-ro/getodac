TEMPLATE = lib

CONFIG += plugin c++14
TARGET = staticFiles

VERSION =
QT =

INCLUDEPATH += $$PWD/../../include

DESTDIR = ../../../plugins

QMAKE_CFLAGS += -Wall -Wextra -Werror -fdevirtualize -fvisibility=hidden
QMAKE_CXXFLAGS += -Wall -Wextra -Werror -fdevirtualize -fvisibility=hidden -fnon-call-exceptions

QMAKE_LFLAGS += -Wl,--no-undefined

#release {
#    QMAKE_CXXFLAGS += -Ofast
#    QMAKE_CFLAGS += -Ofast
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
