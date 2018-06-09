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

    SuperGraph * supergraph = NULL;
    Structure::Graph *t = new Structure::Graph(*inputGraphs[0]);
    superGraphs.push_back(t);

    int len = inputGraphs.size();
    for(int i = 1; i < len; i++){
        Structure::Graph* source = inputGraphs[i];
        Structure::Graph* target = superGraphs[0];
        //target是不是应该被标记一下没有被标记？
        Corresponders[i] = doMatch(source, target, Corresponders[i]);

        if(supergraph) delete supergraph;
        supergraph = new SuperGraph(Corresponders[i]);
        supergraph->generateSuperGraphs();

        supergraph->ComputeSeedRegions(); //$^&$*)#@

        QString objName = "super_sg_" + QString::number(i);
        supergraph->saveSkeleton(objName, "ACTIVE");

        superGraphs.push_back(supergraph->super_sg);
        t = superGraphs[0];
        delete t;
        superGraphs[0] = supergraph->super_tg;

    }
    supergraph->saveSkeleton("super_tg", "TARGET");

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
