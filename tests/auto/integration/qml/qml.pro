TEMPLATE=app
TARGET=tst_qml
CONFIG += warn_on qmltestcase
SOURCES += tst_qml.cpp


importFiles.files = soundeffect

importFiles.path = .
DEPLOYMENT += importFiles