#include "Shapeholder.h"

ShapeHolder::ShapeHolder()
{

}
void ShapeHolder::loadDataset(DatasetMap dataset){
    if(!dataset.size()) return;
    m_dataset = dataset;

    foreach(QString shape, dataset.keys()){
        QString graphFile = dataset[shape]["graphFile"].toString();
        Structure::Graph* g = new Structure::Graph(graphFile);
        inputGraphs.push_back(g);
    }

}

