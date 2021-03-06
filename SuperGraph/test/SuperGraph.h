#ifndef SUPERGRAPH_H
#define SUPERGRAPH_H
#include <QWidget>

#include "DynamicGraph.h"
#include "StructureGraph.h"
#include "GraphCorresponder.h"
#include "SeedRegion.h"


class SuperGraph
{
public:
    enum SaveType{ ACTIVE, TARGET, CORRESPOND };
    SuperGraph(GraphCorresponder * useCorresponder, int i);
    ~SuperGraph();

    /// Super graphs operations:
    //Jerry
    void genSuperGraphsfromSource();
    void genSuperGraphsfromTarget();

    void correspondSuperEdges();
    void equalizeSuperNodeResolutions();
    void equalizeSuperNodeTypes();
    bool convertSheetToCurve(QString sheetID, QString curveID, Structure::Graph* sheetG, Structure::Graph* curveG);
    //Jerry
    void correspondNodesfromSource();
    void correspondNodesfromTarget();
    void correspondEdgesfromSource();
    void correspondEdgesfromTarget();


    /// Helper functions:
    QVector<QString> cloneGraphNode(Structure::Graph *g, QString nodeID, int N);
    Structure::Link * addMissingLink( Structure::Graph *g, Structure::Link * link );

    void correspondTwoEdges( Structure::Link *slink, Structure::Link *tlink, bool isFlip, Structure::Graph* source );

    void postprocessSuperEdges();

    // Edge correspondence cases
    void correspondTrivialEdges( Structure::Graph * source, Structure::Graph * target );
    void correspondSimilarType( Structure::Graph * source, Structure::Graph * target );
    void connectNullNodes( Structure::Graph * source, Structure::Graph * target, bool force);
    void correspondChangedEnds( Structure::Graph * source, Structure::Graph * target, bool force );
    void correspondRemainingOfNull( Structure::Graph * source, Structure::Graph * target, bool force);
    void connectFloatingRealNodes(Structure::Graph * source, Structure::Graph * target);
    void removeRedundantEdges( Structure::Graph * source );

    // Query
    bool isExtraNode( Structure::Node *node );

    // Edge lists filtering
    QVector<Structure::Link*> edgesContain(QVector<Structure::Link*> edges, QString property_name);
    QVector<Structure::Link*> edgesNotContain(QVector<Structure::Link*> edges, QString property_name);

    //我的函数
    void saveSkeleton(QString fileName, SaveType type);

    ///Seed Region
    void ComputeSeedRegions();

    // DEBUG:
    public:
        std::vector< Vector3 > debugPoints;
        std::vector< PairVector3 > debugLines;
        void drawDebug();
        void debugSuperGraphs(QString info);

public:
    Structure::Graph * sg;
    Structure::Graph * tg;
    GraphCorresponder * gcoor;

    /// Super graphs:
    Structure::Graph * super_sg;
    Structure::Graph * active;
    Structure::Graph * super_tg;
    QMap<QString, QString> superNodeCorr;
    QMap<QString, QString> superEdgeCorr;

    int Nth;


};

#endif // SUPERGRAPH_H
