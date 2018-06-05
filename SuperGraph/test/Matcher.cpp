#include "Matcher.h"

Matcher::Matcher(QVector<Structure::Graph *> inputGraphs)
{
    graphs = inputGraphs;
    gcorr = NULL;
}

Matcher::~Matcher(){
    delete gcorr;
}

void Matcher::doMatch(){
    if(graphs.size() < 2)return;

    //相同的一对模型已经匹配过了
    if( gcorr )
    {
        bool isSameSource = gcorr->sg == graphs[0];
        bool isSameTarget = gcorr->tg == graphs[1];

        if( isSameSource && isSameTarget )
        {
            return;
        }
    }

    // Make corresponder
    gcorr = new GraphCorresponder(graphs[0], graphs[1]);


    Correspond();

}

void Matcher::Correspond(){
    gcorr->clear();
    gcorr->computeCorrespondences();

}

