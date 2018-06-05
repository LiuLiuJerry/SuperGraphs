#include "ShapeHolder.h"

ShapeHolder::ShapeHolder()
{

}
ShapeHolder::~ShapeHolder(){
    int l = inputGraphs.size();
    for(int i = 0; i < l; i++){
        Structure::Graph* g = inputGraphs[i];
        delete g;
    }
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


