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
    GraphCorresponder* doMatch(Structure::Graph* sourceGraph, Structure::Graph* targetGraph, GraphCorresponder *gcorr);

public:
    ShapeHolder *shapeholder;
    QVector<Structure::Graph *> superGraphs;

    QVector<GraphCorresponder *> Corresponders;

};

#endif // SHAPEMATCHERS_H
