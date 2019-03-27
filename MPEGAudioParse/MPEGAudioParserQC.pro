TEMPLATE = app
CONFIG += console c++1z
CONFIG -= app_bundle
CONFIG -= qt

SOURCES += \
    mpeg_audio_test.cpp

HEADERS += \
    include/fast_string.h \
    include/my_files_enum.hpp \
    include/my_io.hpp \
    include/my_macros.hpp \
    include/my_mpeg.hpp \
    include/my_sbo_buffer.hpp \
    include/my_string_view.hpp

LIBS += -lstdc++fs
