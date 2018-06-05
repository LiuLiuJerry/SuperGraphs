#ifndef SEEDREGION_H
#define SEEDREGION_H

#include "global.h"
#include "StructureGraph.h"
#include "GraphDistance.h"
#include <QVector>
//每个node都有
class SeedRegion
{
public:
    enum TaskType{ SHRINK, MERGE, MORPH, SPLIT, GROW };
    SeedRegion(Structure::Graph * activeGraph, Structure::Graph * targetGraph, QString node_ID);
    void ComputeSeedRegion();
    void SeedCurveOneEdge(Structure::Link * tlink);
    void SeedCurveTwoEdges(QVector<Structure::Link*> tedges);
    void SeedSheetOneEdge( Structure::Link * l );
    void SeedSheetTwoEdges( Structure::Link * linkA, Structure::Link * linkB );

    QVector<Structure::Link*> filterEdges( Structure::Node * n, QVector<Structure::Link*> allEdges);
    QVector<Structure::Link*> filteredFromTargetEdges();
    //Usages
    Structure::Node* node(){ return active->getNode(this->property["nodeID"].toString());  }
    Structure::Node* targetNode();
    bool ungrownNode( QString nid );
    SheetEncoding encodeSheetAsCurve( Structure::Sheet * sheet, Vector3 start, Vector3 end);
    Structure::Link * SeedRegion::preferredEnd(Structure::Node * n, QVector<Structure::Link*> edges, Structure::Graph * g);
public:
    Structure::Graph *active, *target;
    QMap<QString, QVariant> property;
    QString nodeID;
};

#endif // SEEDREGION_H
