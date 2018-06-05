#include "ShapeMatchers.h"

ShapeMatchers::ShapeMatchers(ShapeHolder *shapeholder)
{
    this->shapeholder = shapeholder;
    for(int i = 0; i < shapeholder->inputGraphs.size(); i++){
        GraphCorresponder *gcorr  = NULL;
        Corresponders.push_back(gcorr);
    }
}

ShapeMatchers::~ShapeMatchers(){
    foreach(GraphCorresponder *gcorr, Corresponders){
        if(gcorr) delete gcorr;
    }
    foreach(Structure::Graph *super_g, superGraphs){
        if(super_g) delete super_g;
    }
}

void ShapeMatchers::MatchShapes(){
    //vector是一个构造对象，不能直接使用=符号进行复制，必须迭代每个元素来复制
    QVector<Structure::Graph*> &inputGraphs = shapeholder->inputGraphs;
    if(inputGraphs.size() < 2) return;
    Structure::Graph* target = inputGraphs[0];
    for(int i = 1; i < inputGraphs.size(); i++){
        Structure::Graph* source = inputGraphs[i];
        //指针指向的东西在里面被更改不行吗……
        Corresponders[i] = doMatch(source, target, Corresponders[i]);

        SuperGraph * supergraph = new SuperGraph(Corresponders[i]);
        supergraph->generateSuperGraphs();
        supergraph->ComputeSeedRegions();
        superGraphs[i] = supergraph->super_sg;
        target = supergraph->super_tg;

        QString objName = "active_" + QString::number(i);
        supergraph->saveSkeleton(objName);

        //delete supergraph;

    }
    superGraphs[0] = target;

}

GraphCorresponder* ShapeMatchers::doMatch(Structure::Graph* sourceGraph, Structure::Graph* targetGraph, GraphCorresponder *gcorr){
    //相同的一对模型已经匹配过了
    if( gcorr )
    {
        bool isSameSource = gcorr->sg == sourceGraph;
        bool isSameTarget = gcorr->tg == targetGraph;

        if( isSameSource && isSameTarget )
        {
            return gcorr;
        }
    }

    // Make corresponder
    gcorr = new GraphCorresponder(sourceGraph, targetGraph);

    gcorr->clear();
    gcorr->computeCorrespondences();

    return gcorr;

}
