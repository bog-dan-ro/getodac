TEMPLATE = app

CONFIG += console c++14

TARGET = getodac

QT =

INCLUDEPATH += http-parser $$PWD/../include

HEADERS += \
    server_plugin.h \
    server_service_sessions.h \
    server_session.h \
    server.h \
    sessions_event_loop.h \
    x86_64-signal.h \
    http-parser/http_parser.h \
    secured_server_session.h

SOURCES += \
    main.cpp \
    server_plugin.cpp \
    server_service_sessions.cpp \
    server_session.cpp \
    server.cpp \
    sessions_event_loop.cpp \
    http-parser/http_parser.c \
    secured_server_session.cpp

QMAKE_CFLAGS += -Wall -Wextra -Werror -fsplit-stack -fdevirtualize -fvisibility=hidden
QMAKE_CXXFLAGS += -Wall -Wextra -Werror -fsplit-stack -fdevirtualize -fvisibility=hidden -fnon-call-exceptions

release {
    QMAKE_CXXFLAGS += -Ofast
    QMAKE_CFLAGS += -Ofast
}

LIBS += -lboost_coroutine -lboost_context -lboost_system -lboost_thread -lboost_program_options -lboost_filesystem -lcrypto -lssl -ldl

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
