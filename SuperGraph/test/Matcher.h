#ifndef MATCHER_H
#define MATCHER_H
#include "GraphCorresponder.h"


class Matcher
{
public:
    Matcher(QVector<Structure::Graph *> inputGraphs);
    void doMatch();
    void Correspond();
    ~Matcher();

public:
    GraphCorresponder *gcorr;
    QVector<Structure::Graph*> graphs; //vector会自动析构

};

#endif // MATCHER_H
