#ifndef SHAPEHOLDER_H
#define SHAPEHOLDER_H
#include "global.h"
#include "StructureGraph.h"

class ShapeHolder
{
public:
    ShapeHolder();
    ~ShapeHolder();
    void loadDataset(DatasetMap dataset);

public:
    QVector<Structure::Graph *> inputGraphs;  //两个需要匹配的物体
    DatasetMap m_dataset;


};

#endif // SHAPEHOLDER_H
