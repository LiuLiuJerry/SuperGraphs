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
        gcorr = NULL;
    }
    foreach(Structure::Graph *super_g, superGraphs){
        if(super_g) delete super_g;
        super_g = NULL;
    }
    foreach(Structure::Graph *tg, superTargets){
        if(tg) delete tg;
        tg = NULL;
    }
}

void ShapeMatchers::MatchShapes(){
    //vector是一个构造对象，不能直接使用=符号进行复制，必须迭代每个元素来复制
    QVector<Structure::Graph*> &inputGraphs = shapeholder->inputGraphs;
    if(inputGraphs.size() < 2) return;

    SuperGraph * supergraph = NULL;
    //Structure::Graph *t = new Structure::Graph(*inputGraphs[0]);
    //superGraphs.push_back(t);
    superTargets.push_back(NULL);

    int len = inputGraphs.size();
    //第一次匹配：
    //第一步先生成一个MAX的集合，把所有source的内容拷贝到target里，相当于字典，第二步再生成seed region
    //两步都需要配准，不过第一步的配准主要目的是找出MAX set，第二次配准才是为了对齐所有的node并生成seed region
    Structure::Graph* target = inputGraphs[0];
    Structure::Graph* source;
    for(int i = 1; i < len; i++){
        source = inputGraphs[i];
        //清空target里的对应，防止上次匹配的结果影响下次的匹配
        resetGraph(target);

        //把第一个shape当作模板，全部和它进行匹配
        Corresponders[i] = doMatch(source, target, Corresponders[i]);
        Corresponders[i]->saveCorrespondences("correspondence_" + QString::number(i) + ".xml");

        if(supergraph) delete supergraph;
        supergraph = new SuperGraph(Corresponders[i], i);
        supergraph->genSuperGraphsfromSource();

        QString tName = "middle_sg_" + QString::number(i);
        supergraph->saveSkeleton(tName, "ACTIVE");

        //target是之前一步的supr_tg，是superGraph里面new出来的，不归ShapeHolder删
        superTargets.push_back(supergraph->super_tg);
        //superGraphs.push_back(supergraph->super_sg);

        target = supergraph->super_tg;

    }

    supergraph->saveSkeleton("middle_tg", "TARGET");

    QVector<Structure::Graph* >& middleGraphs = inputGraphs;
    //smiddleGraphs.swap(superGraphs);

    target = superTargets[len-1];
    superTargets.pop_back();
    resetGraph(target);

    foreach(Structure::Graph* st, superTargets){
        if(st) delete st;
    }
    superTargets.clear();


    //第二次匹配：
    //把内容从target拷贝回来
    //
    //seed region是直接向target匹配着算呢，还是找到图的来源再匹配着算？
    //如果是找到图的来源，还涉及计算active图和source图的配准问题
    //
    //终极问题：1.在拷贝node进来的时候它的边是怎样处理的？
    //          答：所有能从target拷贝的边都拷贝过来了，计算的时候只需要考虑哪些用来计算seed region就行了
    //        2.filterfromtarget和filteredges的区别在哪里？它们可以互相替换吗？
    //          答：应该不能，但是从active应该可以根据target计算seed region？

    for(int i = 0; i < len; i++){
        source = middleGraphs[i];
        //清空target里的对应，防止上次匹配的结果影响下次的匹配
        resetGraph(target);

        //其实这里没有必要全部重新匹配，可以进行优化
        Corresponders[i] = doMatch(source, target, Corresponders[i]);
        Corresponders[i]->saveCorrespondences("correspondence_" + QString::number(i) + ".xml");

        if(supergraph) delete supergraph;
        supergraph = new SuperGraph(Corresponders[i], i);
        //这次只需要给便source，不能改变target
        supergraph->genSuperGraphsfromTarget();

        supergraph->ComputeSeedRegions();

        QString tName = "super_sg_" + QString::number(i);
        supergraph->saveSkeleton(tName, "ACTIVE");

        superGraphs.push_back(supergraph->super_tg);

    }


    foreach(Structure::Graph *mg, middleGraphs){
        if(mg) delete mg;
    }
    middleGraphs.clear();

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
    //怎么感觉这里只匹配node不匹配边呢
    gcorr->computeCorrespondencesNew();

    return gcorr;

}

void ShapeMatchers::resetGraph(Structure::Graph* graph){
    foreach(Structure::Node* node, graph->nodes){
        PropertyMap::iterator it;
        it = node->property.find("correspond");
        if(it != node->property.end())
            node->property.erase(it);

    }

    foreach(Structure::Link* link, graph->edges){
        PropertyMap::iterator it;
        it = link->property.find("correspond");
        if(it != link->property.end())
            link->property.erase(it);
    }
}
