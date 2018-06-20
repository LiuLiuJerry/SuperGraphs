#ifndef SHAPEMATCHERS_H
#define SHAPEMATCHERS_H
#include "ShapeHolder.h"
#include "SuperGraph.h"
#include "SeedRegion.h"

#include <QVector>


class ShapeMatchers
{
public:
    ShapeMatchers(ShapeHolder *shapeholder);
    ~ShapeMatchers();
    void MatchShapes();
    void resetGraph(Structure::Graph* graph);
    GraphCorresponder* doMatch(Structure::Graph* sourceGraph, Structure::Graph* targetGraph, GraphCorresponder *gcorr);

public:
    ShapeHolder *shapeholder;
    QVector<Structure::Graph *> superGraphs;
    QVector<Structure::Graph *> superTargets;

    QVector<GraphCorresponder *> Corresponders;

    //QVector<QMap<QString, QVector<QString>>> Dictionary;

};

#endif // SHAPEMATCHERS_H
