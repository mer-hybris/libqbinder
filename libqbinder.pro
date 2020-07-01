TARGET = qbinder
TEMPLATE = lib
CONFIG += create_pc create_prl no_install_prl link_pkgconfig

include(version.pri)

# LIBGLIBUTIL_PATH and LIBGBINDER_PATH can be defined for side-by-side
# builds (i.e. when those are just built elsewhere, and not being properly
# installed). These variables can be either passed to qmake from the
# command line or defined as environment variables. Values passed to
# qmake take precedence.

isEmpty(LIBGLIBUTIL_PATH) {
    LIBGLIBUTIL_PATH = $$(LIBGLIBUTIL_PATH)
}

isEmpty(LIBGBINDER_PATH) {
    LIBGBINDER_PATH = $$(LIBGBINDER_PATH)
}

isEmpty(LIBGLIBUTIL_PATH) {
} else {
    system(make -C $${LIBGLIBUTIL_PATH} release)
    PKGCONFIG += glib-2.0
    INCLUDEPATH += $${LIBGLIBUTIL_PATH}/include
}

isEmpty(LIBGBINDER_PATH) {
    PKGCONFIG += libgbinder
} else {
    system(make -C $${LIBGBINDER_PATH} release)
    INCLUDEPATH += $${LIBGBINDER_PATH}/include
    LIBS += $${LIBGBINDER_PATH}/build/release/libgbinder.so
}

QT -= gui
QMAKE_CXXFLAGS += -Wno-unused-parameter

DEFINES += QBINDER_LIBRARY
INCLUDEPATH += include

PKGCONFIG_NAME = $${TARGET}

isEmpty(PREFIX) {
    PREFIX=/usr
}

OTHER_FILES += \
    $${PKGCONFIG_NAME}.prf \
    rpm/libqbinder.spec \
    README

SOURCES += \
    src/qbinder_eventloop.cpp

PUBLIC_HEADERS += \
    include/qbinder.h \
    include/qbinder_eventloop.h \
    include/qbinder_types.h

HEADERS += \
    $${PUBLIC_HEADERS}

target.path = $$[QT_INSTALL_LIBS]

headers.files = $${PUBLIC_HEADERS}
headers.path = $${INSTALL_ROOT}$${PREFIX}/include/qbinder

pkgconfig.files = $${PKGCONFIG_NAME}.pc
pkgconfig.path = $$[QT_INSTALL_LIBS]/pkgconfig

QMAKE_PKGCONFIG_NAME = $${PKGCONFIG_NAME}
QMAKE_PKGCONFIG_DESTDIR = pkgconfig
QMAKE_PKGCONFIG_INCDIR = $$headers.path
QMAKE_PKGCONFIG_DESCRIPTION = Qt integration for libgbinder
QMAKE_PKGCONFIG_PREFIX = $${PREFIX}
QMAKE_PKGCONFIG_VERSION = $${VERSION}

INSTALLS += target headers pkgconfig
