#ifndef MATCHER_H
#define MATCHER_H
#include "GraphCorresponder.h"


class Matcher
{
public:
    Matcher(QVector<Structure::Graph *> inputGraphs);
    void doMatch();
    void Correspond();

public:
    GraphCorresponder *gcorr;
    QVector<Structure::Graph*> graphs;

};

#endif // MATCHER_H
