TEMPLATE = app

CONFIG += console c++14

CONFIG(debug, debug|release): CONFIG += sanitizer sanitize_address sanitize_undefined
else: {
    QMAKE_CFLAGS += -Ofast
    QMAKE_CXXFLAGS += -Ofast
}

TARGET = getodac

QT =

INCLUDEPATH += http-parser $$PWD/../../include

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

QMAKE_CFLAGS += -Wall -Wextra -Werror
QMAKE_CXXFLAGS += -Wall -Wextra -Werror -fnon-call-exceptions

LIBS += -lboost_coroutine -lboost_context -lboost_system -lboost_thread -lboost_program_options -lboost_filesystem -lcrypto -lssl -ldl
