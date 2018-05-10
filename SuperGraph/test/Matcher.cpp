#include "Matcher.h"

Matcher::Matcher(QVector<Structure::Graph *> inputGraphs)
{
    graphs = inputGraphs;
    gcorr = NULL;
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
            //emit( corresponderCreated(gcorr) );

            //manualMode();
            //DemoPage::show();

            return;
        }
    }

    // Make corresponder
    gcorr = new GraphCorresponder(graphs[0], graphs[1]);

    // Check for existing correspondence file
    /*QString path = "dataset/corr/", ext = ".txt";
    QString g1n = graphs[0]->name, g2n = graphs[1]->name;
    QStringList files; files << (path+g1n+"_"+g2n+ext) << (path+g2n+"_"+g1n+ext);
    int fileidx = -1; //看有没有之前存过的文件
    for(int i = 0; i < 2; i++){
        QString f = files[i];
        if(!QFileInfo (f).exists()) continue;
        fileidx = i;
    }

    //手动或者自动配准
    if( fileidx != -1 ) //找到了存下的匹配文件
    {
        bool corrReversed = (fileidx == 0) ? false : true;

        //property["corrFile"] = files[fileidx];
        //property["corrReversed"] = corrReversed;

        //manualMode();

        // Make sure manual button is selected
        //emit( correspondenceFromFile() );
    }
    else
    {
        Correspond();
    }*/

    Correspond();

}

void Matcher::Correspond(){
    gcorr->clear();
    gcorr->computeCorrespondences();

    //prevCorrAuto.clear();
    //prevCorrManual.clear();
}
