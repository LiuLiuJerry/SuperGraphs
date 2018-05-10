#ifndef GLOBAL_H
#define GLOBAL_H

#include <QString>
#include <QVariant>
#include <QDebug>
#include <QElapsedTimer>
#include <QtConcurrent/QtConcurrentRun>
#include <QDir>
#include <QFileInfo>
#include <QtWidgets>
#include <QGLWidget>
#include <QPainter>

#include <QGraphicsScene>

//QVariant 多种数据类型的组合
typedef QMap<QString, QVariant> PropertyMap;
typedef QMap<QString, PropertyMap> DatasetMap;

#endif
