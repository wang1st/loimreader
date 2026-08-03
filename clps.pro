QT       += core gui widgets printsupport network xml
#windeployqt.exe --compiler-runtime --no-translations --no-opengl-sw C:\Users\Administrator\Documents\Qt\Releasex86\ctdy123.exe
#/Users/wangzhen/Qt5.14.2/5.14.2/clang_64/bin/macdeployqt /Users/wangzhen/build-clps-Desktop_Qt_5_14_2_clang_64bit-Release/ctdy123.app
greaterThan(QT_MAJOR_VERSION, 4): QT += widgets

TARGET = ctdy123
VERSION = 2.6 #版本号修改后，要全部重新构建工程才生效
TEMPLATE = app

DEFINES += FA_APP_VERSION=\\\"$$VERSION\\\"
DEFINES += FA_APP_NAME=\\\"$$TARGET\\\"

# 已弃用 Fervor 自动更新框架（迁移到自研更新对话框）

CONFIG += c++11

#QMAKE_CXXFLAGS_DEBUG += "-gstabs+"
#QMAKE_CFLAGS_DEBUG += "-gstabs+"

# The following define makes your compiler emit warnings if you use
# any Qt feature that has been marked deprecated (the exact warnings
# depend on your compiler). Please consult the documentation of the
# deprecated API in order to know how to port your code away from it.
DEFINES += QT_DEPRECATED_WARNINGS

# You can also make your code fail to compile if it uses deprecated APIs.
# In order to do so, uncomment the following line.
# You can also select to disable deprecated APIs only up to a certain version of Qt.
#DEFINES += QT_DISABLE_DEPRECATED_BEFORE=0x060000    # disables all the APIs deprecated before Qt 6.0.0

SOURCES += \
    logindialog.cpp \
    machineid.cpp \
    main.cpp \
    mainview.cpp \
    mainwindow.cpp \
    pageitem.cpp \
    previewitem.cpp \
    leftview.cpp \
    popup.cpp \
    util.cpp \
    protection.cpp \
    versionmanager.cpp \
    simpleupdater.cpp \
    updatedialog.cpp \
    aboutdialog.cpp

HEADERS += \
    logindialog.h \
    machineid.h \
    mainview.h \
    mainwindow.h \
    pageitem.h \
    previewitem.h \
    leftview.h \
    popup.h \
    util.h \
    protection.h \
    versionmanager.h \
    simpleupdater.h \
    updatedialog.h \
    aboutdialog.h

FORMS +=

# Default rules for deployment.
qnx: target.path = /tmp/$${TARGET}/bin
else: unix:!android: target.path = /opt/$${TARGET}/bin
!isEmpty(target.path): INSTALLS += target

DISTFILES += \
    genlicense.py \
    icons/buy.png \
    icons/extand.png \
    icons/open.png \
    icons/print.png \
    icons/save.png \
    icons/shrink.png \
    icons/zoomin.png \
    icons/zoomout.png \
    images/myshop.png \
    poppler/poppler-qt5.pc.cmake

RESOURCES += \
    clps.qrc

TRANSLATIONS += i18n/ctdy123_ch.ts\
    i18n/ctdy123_en.ts

RC_ICONS = icons/main.ico

# Qt6 兼容：默认禁用 Poppler（可用DEFINES+=NO_POPPLER覆盖），Qt5保留
greaterThan(QT_MAJOR_VERSION, 5) {
    DEFINES += NO_POPPLER
} else {
    win32:CONFIG(release, debug|release): LIBS += -L$$PWD/lib/ -llibpoppler-qt5
    else:win32:CONFIG(debug, debug|release): LIBS += -L$$PWD/lib/ -llibpoppler-qt5
    macx: LIBS += //Users/wangzhen/longimgprint/lib/libpoppler-qt5.dylib
    INCLUDEPATH += $$PWD/include
    DEPENDPATH += $$PWD/include
}
