TEMPLATE = app
CONFIG += link_pkgconfig
PKGCONFIG += qbinder libgbinder libglibutil glib-2.0
QMAKE_CXXFLAGS += -Wno-unused-parameter -Wno-missing-field-initializers
QT -= gui

SOURCES += qbinder-ping.cpp
