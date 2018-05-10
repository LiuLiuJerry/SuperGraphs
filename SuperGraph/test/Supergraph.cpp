#include <set>
#include <QStack>
#include <QFileSystemModel>
#include <QDockWidget>
#include <QMainWindow>

#include "SuperGraph.h"
using namespace Structure;

#include "ExportDynamicGraph.h"
using namespace DynamicGraphs;

#include "TaskCurve.h"
#include "TaskSheet.h"

// Temporary solution for output
#include "surface_mesh/IO.h"

typedef std::pair<QString, QString> PairQString;

Q_DECLARE_METATYPE( Structure::Sheet* )
Q_DECLARE_METATYPE( QSet<Structure::Node*> )



SuperGraph::SuperGraph(GraphCorresponder * useCorresponder)
{
    /// STEP 1) Use a corresponder, automatic or user assisted
    this->gcoor = useCorresponder;
    this->sg = useCorresponder->sg;
    this->tg = useCorresponder->tg;

    /// STEP 2) Generate super graphs
    generateSuperGraphs();
}

SuperGraph::~SuperGraph()
{
    if(super_sg) delete super_sg;
    if(super_tg) delete super_tg;
}

bool SuperGraph::isExtraNode( Structure::Node *node )
{
    return node->property["correspond"].toString().contains("_null");
}

QVector<QString> SuperGraph::cloneGraphNode( Structure::Graph *g, QString nodeID, int N )
{
    Structure::Node * node = g->getNode(nodeID);

    // Clone the nodes
    QVector<QString> cloned_node_IDs;
    QVector<Structure::Node *> cloned_nodes;
    for (int i = 0; i < N; i++)
    {
        Structure::Node * n = node->clone();
        n->id = nodeID + "_" + QString::number(i);

        g->addNode(n);

        cloned_node_IDs.push_back(n->id);
        cloned_nodes.push_back(n);
    }

    // Link the cloned nodes
    bool isModifyEdges = true;

    if( isModifyEdges )
    {
        int currCloned = 0;
        foreach(Structure::Link *link, g->getEdges(nodeID))
        {
            Structure::Node * other = link->otherNode(nodeID);
            QString otherID = other->id;

            if (isExtraNode(other))
            {
                if (currCloned >= N)
                {
                    continue;
                    qDebug() << "Warning: clone nodes error.";
                }

                // Link other to one of the cloned node
                QString clonedID = cloned_nodes[currCloned++]->id;
                Structure::Node * cnode = g->getNode(clonedID);

                LinkCoords c1 = link->getCoord(other->id);
                LinkCoords c2 = link->getCoordOther(other->id);
                g->addEdge(other, cnode, c1, c2, g->linkName(other,cnode));
            }
            else
            {
                // Link other to all cloned nodes
                foreach (Structure::Node* cnode, cloned_nodes)
                {
                    LinkCoords c1 = link->getCoord(other->id);
                    LinkCoords c2 = link->getCoordOther(other->id);
                    g->addEdge(other, cnode, c1, c2, g->linkName(other,cnode));
                }
            }

            // Remove this link
            g->removeEdge(node, other);
        }
    }

    // remove the original node
    g->removeNode(nodeID);

    // remove its groups
    foreach(QVector<QString> group, g->groupsOf(nodeID))
        g->removeGroup(group);

    return cloned_node_IDs;
}

void SuperGraph::correspondSuperNodes()
{
    // Add virtual corresponding nodes for missing nodes

    // To shrink:
    foreach(QString snodeID, gcoor->nonCorresSource())
    {
        Structure::Node *snode = super_sg->getNode(snodeID);
        Structure::Node *ctnode = snode->clone();
        ctnode->id = snodeID + "_null";

        assert(!super_tg->getNode(ctnode->id));
        super_tg->addNode(ctnode);

        superNodeCorr[snodeID] = ctnode->id;
    }

    // To grow:
    foreach(QString tnodeID, gcoor->nonCorresTarget())
    {
        Structure::Node *tnode = super_tg->getNode(tnodeID);
        Structure::Node *csnode = tnode->clone();
        csnode->id = tnodeID + "_null";

        assert(!super_sg->getNode(csnode->id));
        super_sg->addNode(csnode);

        superNodeCorr[csnode->id] = tnodeID;
    }

    // Copy growing groups from target
    QStringList nonCorresTarget;
    foreach(QString tnodeID, gcoor->nonCorresTarget()) nonCorresTarget << tnodeID;
    foreach(QVector<QString> tgroup, super_tg->groups)
    {
        // Add this groups
        QVector<QString> sgroup;
        //超图中的每一个组中的每一个节点，如果被没有匹配的List包含的话，就放到sgroup中去
        foreach(QString tnodeID, tgroup) {
            if(nonCorresTarget.contains(tnodeID))
                sgroup << (tnodeID + "_null");
        }

        if(!sgroup.size()) continue;
        super_sg->addGroup(sgroup); //在target中是一个组的，如果没有被匹配，拷贝到source中也应该是一个组
    }

    // Build node correspondence for corresponded nodes
    QVector< PairQString > core_pairs;
    foreach (PART_LANDMARK vec2vec, gcoor->correspondences)
    {
        QVector<QString> sNodes = vec2vec.first;
        QVector<QString> tNodes = vec2vec.second;

        int sN = sNodes.size();
        int tN = tNodes.size();
        Structure::Node * snode = super_sg->getNode(sNodes.front());
        Structure::Node * tnode = super_tg->getNode(tNodes.front());

        // 1-to-1
        if (sN == 1 && tN == 1){
            superNodeCorr[snode->id] = tnode->id;
        }

        // N-to-1
        if (sN > 1)	{
            QVector<QString> ctnodeIDs = cloneGraphNode(super_tg, tnode->id, sN);
            for (int i = 0; i < sN; i++) superNodeCorr[sNodes[i]] = ctnodeIDs[i];
            super_tg->addGroup(ctnodeIDs);
        }

        // 1-to-N
        if (tN > 1)	{
            QVector<QString> csnodeIDs = cloneGraphNode(super_sg, snode->id, tN);
            for (int i = 0; i < tN; i++) superNodeCorr[csnodeIDs[i]] = tNodes[i];
            super_sg->addGroup(csnodeIDs);
        }
    }

    // Store correspondences in the graphs
    foreach(QString snode, superNodeCorr.keys())
    {
        QString tnode = superNodeCorr[snode];

        Structure::Node * sn = super_sg->getNode(snode);
        Structure::Node * tn = super_tg->getNode(tnode);

        if(!sn || !tn){
            qDebug() << "ERROR: many to many cases are not handled";
            continue;
        }

        sn->property["correspond"] = tnode;
        tn->property["correspond"] = snode;
    }
}

void SuperGraph::correspondTwoEdges( Structure::Link *slink, Structure::Link *tlink, bool isFlip, Structure::Graph* source )
{
    if(slink->property.contains("correspond") || tlink->property.contains("correspond")) return;

    // Force flip order of nodes of link on super_sg
    if (isFlip)
    {
        QString n1 = slink->n1->id;
        QString n2 = slink->n2->id;
        Array1D_Vector4d c1 = slink->coord.front();
        Array1D_Vector4d c2 = slink->coord.back();

        source->removeEdge(n1,n2);
        slink = source->addEdge(source->getNode(n2), source->getNode(n1), c2, c1, source->linkName(n2,n1) );
    }

    slink->property["correspond"] = tlink->property["uid"].toInt();
    tlink->property["correspond"] = slink->property["uid"].toInt();
}

// This function could be written with [graphA] and [graphB] but we should
// keep it this way to ensure correspondence happen together, once and at same time
void SuperGraph::correspondSuperEdges()
{
    bool CASE_1 = true;
    bool CASE_2 = true;
    bool CASE_3 = true;
    bool CASE_4 = true;
    bool CASE_5 = true;
    bool CASE_6 = true;
    bool CASE_7 = true;

    bool dbg = false;

    if(dbg) debugSuperGraphs("00");

    /// Correspond trivial edges, i.e. both nodes exist on both graphs
    if( CASE_1 )
    {
        correspondTrivialEdges ( super_sg, super_tg );
        correspondTrivialEdges ( super_tg, super_sg );
    }

    if(dbg) debugSuperGraphs("01");

    /// Correspond edges between two [extra] or two [missing] nodes
    if( CASE_2 )
    {
        correspondSimilarType( super_sg, super_tg );
        correspondSimilarType( super_tg, super_sg );
    }

    if(dbg) debugSuperGraphs("02");

    /// Creating one edge for floating null nodes [has heuristic]
    if (CASE_3)
    {
        connectNullNodes( super_sg, super_tg );
        connectNullNodes( super_tg, super_sg );
    }

    if(dbg) debugSuperGraphs("03");

    /// Correspond edges with changed ends
    if( CASE_4 )
    {
        correspondChangedEnds( super_sg, super_tg );
    }

    if(dbg) debugSuperGraphs("04");

    /// Do remaining edges of null nodes
    if( CASE_5 )
    {
        correspondRemainingOfNull( super_sg, super_tg );
        correspondRemainingOfNull( super_tg, super_sg );
    }

    if(dbg) debugSuperGraphs("05");

    /// Link flying real nodes [same as case 4 but for cores]
    if (CASE_6)
    {
        connectFloatingRealNodes(super_sg, super_tg);
        connectFloatingRealNodes(super_tg, super_sg);
    }

    if(dbg) debugSuperGraphs("06");

    /// Remove excess edges (edges of core nodes still uncorresponded)
    if( CASE_7 )
    {
        removeRedundantEdges( super_sg );
        removeRedundantEdges( super_tg );
    }

    if(dbg) debugSuperGraphs("07");

    // Visualization: assign global unique ids
    int viz_uid = 0;
    foreach(Structure::Link * slink, super_sg->edges){
        if(!slink->property.contains("correspond")) continue;
        Link* tlink = super_tg->getEdge(slink->property["correspond"].toInt());
        if(!tlink) continue;
        slink->property["viz_uid"] = tlink->property["viz_uid"] = viz_uid++;
    }
}

void SuperGraph::correspondTrivialEdges( Structure::Graph * source, Structure::Graph * target )
{
    foreach(Structure::Link * slink, source->edges)
    {
        Structure::Node *sn1 = slink->n1, *sn2 = slink->n2;
        Structure::Node *tn1 = target->getNode( sn1->property["correspond"].toString() ),
            *tn2 = target->getNode( sn2->property["correspond"].toString() );

        Structure::Link * tlink = target->getEdge(tn1->id, tn2->id);

        if(!tlink) continue;
        //是不是翻过来的
        bool isFlip = ( tlink->n1->id != tn1->id );
        correspondTwoEdges(slink, tlink, isFlip, source);
    }
}

void SuperGraph::correspondSimilarType( Structure::Graph * source, Structure::Graph * target )
{
    foreach(Structure::Link * slink, source->edges)
    {
        if (slink->property.contains("correspond")) continue;

        Structure::Node *sn1 = slink->n1, *sn2 = slink->n2;
        Structure::Node *tn1 = target->getNode(sn1->property["correspond"].toString()),
            *tn2 = target->getNode(sn2->property["correspond"].toString());

        // We are only looking for missing edges
        if(!(tn1->id.contains("_null") && tn2->id.contains("_null"))) continue;

        Structure::Link * tlink = addMissingLink(target, slink);
        correspondTwoEdges(slink, tlink, false, source);
    }
}

void SuperGraph::connectNullNodes( Structure::Graph * source, Structure::Graph * target )
{
    // Sorted by valence descending
    QVector<Structure::Node*> nullNodes;
    typedef QPair<int,Node*> ValenceNode;
    QMap<Node*,int> nodeV;
    foreach(Structure::Node * n, source->nodes){
        if (!n->id.contains("_null")) continue;
        //新加入的空节点，按照价排序 the degree (or valency) of a vertex of a graph is the number of edges incident to the vertex
        nodeV[n] = source->valence(n);
    }

    // high to low //source中每个节点的价从高到低排序
    foreach(ValenceNode pair, sortQMapByValue(nodeV))
        nullNodes.push_front(pair.second);

    foreach(Structure::Node * snode, nullNodes)
    {
        Structure::Node * tnode = target->getNode( snode->property["correspond"].toString() );

        // Get edges of target node //source中的node对应的target node的相连的边中没有被匹配的边
        QVector<Structure::Link*> tedges = edgesNotContain( target->getEdges(tnode->id), "correspond" );
        if(tedges.isEmpty()) continue;

        // Make sure our connected sub-graph is in fact disconnected
        bool isConnectedViaOthers = false;
        // 如果没有相邻的节点或者所有和snode相连的node中，如果全都是新加进来的null node，说明没有其他节点与它相连（怎么会有其他null和他相连呢……）
        foreach(QString nid, source->nodesCanVisit(snode))	if(!nid.contains("_null")) { isConnectedViaOthers = true; break; }
        if( isConnectedViaOthers ) continue;

        if (tedges.size() == 1)
        {   //在source中加入缺少的边
            Link* tlink = tedges.front();
            Link* slink = addMissingLink(source, tlink);
            correspondTwoEdges(slink, tlink, false, source);
        }
        else if (tedges.size() == 2)
        {
            foreach( Link* tlink, tedges)
            {
                Link* slink = addMissingLink(source, tlink);
                correspondTwoEdges(slink, tlink, false, source);
            }
        }
        else
        {
            // Collect names, in source, of target's neighboring nodes
            QStringList nodesKeep;
            foreach(Link * tlink, tedges){
                Node* tOther = tlink->otherNode(tnode->id);
                Node* sOther = source->getNode(tOther->property["correspond"].toString());
                nodesKeep << sOther->id; //对应的应该被链接的节点
            }

            // Keep only subgraph with names from above
            Structure::Graph copy(*source);
            foreach(Node*n, source->nodes) if(!nodesKeep.contains(n->id)) copy.removeNode(n->id);

            // Find node with most valence, otherwise pick first
            int bestValence = 0;
            QString bestID = copy.nodes.front()->id;
            foreach(Node * n, copy.nodes){
                int valence = copy.valence(n);
                if (valence > bestValence){
                    bestID = n->id;
                    bestValence = valence;
                }
            }

            // Find corresponding link on target
            Node* bestTNode = target->getNode( source->getNode(bestID)->property["correspond"].toString() );
            Link* bestTLink = target->getEdge( tnode->id, bestTNode->id );

            // Add the link to source // 用价最大的那个node去链接需要链接进来的null—node
            Link* slink = addMissingLink(source, bestTLink);
            correspondTwoEdges(slink, bestTLink, false, source);

            /// Respect the groups:
            foreach(QVector<QString> group, target->groupsOf(bestTNode->id)){
                foreach(QString element, group){
                    Node * tn = target->getNode(element);
                    //Node * sn = source->getNode(tn->property["correspond"].toString());

                    if(element == bestTNode->id) continue;

                    // If there is an edge on the target between me and the element of the group
                    Link * tlink = target->getEdge(tnode->id, tn->id);
                    if( tlink )
                    {
                        Link* slink = addMissingLink(source, tlink);
                        correspondTwoEdges(slink, tlink, false, source);
                    }
                }
            }

            foreach(QVector<QString> group, source->groupsOf(bestID)){
                foreach(QString element, group){
                    Node * sn = source->getNode(element);
                    Node * tn = target->getNode(sn->property["correspond"].toString());
                    if(element == bestID) continue;
                    Link * tlink = target->getEdge(tnode->id, tn->id);
                    if( tlink )
                    {
                        Link* slink = addMissingLink(source, tlink);
                        correspondTwoEdges(slink, tlink, false, source);
                    }
                }
            }
        }
    }
}

void SuperGraph::correspondChangedEnds( Structure::Graph * source, Structure::Graph * target )
{
    bool isRunning = true;
    do{
        isRunning = false;

        foreach(Structure::Node * snode, source->nodes)
        {
            Structure::Node * tnode = target->getNode( snode->property["correspond"].toString() );

            QVector<Structure::Link*> sedges = edgesNotContain( source->getEdges(snode->id), "correspond" );
            QVector<Structure::Link*> tedges = edgesNotContain( target->getEdges(tnode->id), "correspond" );

            if(!sedges.size() || !tedges.size()) continue;

            // Only deal with cases that are for sure (equal number of edges)
            // This will leave some nodes flying that will be deal with by later case
            if( sedges.size() == tedges.size() && !sedges.isEmpty())
            {
                if( false )
                {
                    bool isPartOfGroup = !source->groupsOf(snode->id).front().isEmpty();
                    if( snode->id.contains("_null") && isPartOfGroup ) continue;
                }

                for( int i = 0; i < (int)sedges.size(); i++)
                {
                    Link* slink = sedges[i], *tlink = tedges[i];

                    bool sFromMe = ( slink->n1->id == snode->id );
                    bool tFromMe = ( tlink->n1->id == tnode->id );

                    bool isFlip = (sFromMe != tFromMe);

                    correspondTwoEdges(slink, tlink, isFlip, source);
                }

                isRunning = true;
            }
            // Source has been duplicated ?
            else if(tedges.size() == 1 && sedges.size() > 1)
            {
                Link * tlink = tedges.front();

                // A single edge from the target graph is ground truth
                if(target == super_tg)
                {
                    Structure::Node * n1 = source->getNode( tlink->getNode(tnode->id)->property["correspond"].toString() );
                    Structure::Node * n2 = source->getNode( tlink->otherNode(tnode->id)->property["correspond"].toString() );
                    Structure::Link * newEdge = source->addEdge(n1, n2, tlink->getCoord(tnode->id), tlink->getCoordOther(tnode->id), source->linkName(n1,n2));
                    sedges.push_front( newEdge );
                }

                Link* slink = sedges.front();
                bool sFromMe = ( slink->n1->id == snode->id );
                bool tFromMe = ( tlink->n1->id == tnode->id );
                bool isFlip = (sFromMe != tFromMe);

                correspondTwoEdges(slink, tlink, isFlip, source);
            }
        }
    } while (isRunning);
}

void SuperGraph::correspondRemainingOfNull( Structure::Graph * source, Structure::Graph * target )
{
    foreach(Structure::Node * snode, source->nodes)
    {
        if (!snode->id.contains("_null")) continue;
        Structure::Node * tnode = target->getNode( snode->property["correspond"].toString() );

        // Get un-corresponded of target //把null-node的对应node的边都拷贝过来
        QVector<Structure::Link*> tedges = edgesNotContain( target->getEdges(tnode->id), "correspond" );
        if(tedges.isEmpty()) continue;

        foreach(Link * tlink, tedges)
        {
            Link * slink = addMissingLink( source, tlink );

            correspondTwoEdges(slink, tlink, false, source);
        }
    }
}

void SuperGraph::connectFloatingRealNodes( Structure::Graph * source, Structure::Graph * target )
{
    foreach(Structure::Node * snode, source->nodes)
    {
        if (snode->id.contains("_null")) continue;

        QVector<Link*> alledges = source->getEdges(snode->id);
        QVector<Link*> sedges = edgesNotContain(alledges, "correspond");

        // No edges are corresponded yet
        if (sedges.size() == alledges.size())
        {
            Structure::Node * tnode = target->getNode( snode->property["correspond"].toString() );
            QVector<Structure::Link*> tedges = edgesNotContain( target->getEdges(tnode->id), "correspond" );

            int N = qMin(sedges.size(), tedges.size());
            for(int i = 0; i < N; i++)
            {
                Link* slink = sedges[i], *tlink = tedges[i];

                bool sFromMe = ( slink->n1->id == snode->id );
                bool tFromMe = ( tlink->n1->id == tnode->id );

                bool isFlip = (sFromMe != tFromMe);

                correspondTwoEdges(slink, tlink, isFlip, source);
            }
        }
    }
}

void SuperGraph::removeRedundantEdges( Structure::Graph * source )
{
    foreach(Structure::Node * snode, source->nodes)
    {
        if (snode->id.contains("_null")) continue;

        foreach(Link* link, edgesNotContain( source->getEdges(snode->id), "correspond" ))
            source->removeEdge(link->n1, link->n2);
    }
}

void SuperGraph::generateSuperGraphs()
{
    // Two super graphs have one-to-one correspondence between nodes and edges
    // Two corresponded edge don't have to link two corresponded nodes
    super_sg = new Structure::Graph(*sg);
    super_tg = new Structure::Graph(*tg);

    super_sg->property["sourceName"] = gcoor->sgName();
    super_sg->property["targetName"] = gcoor->tgName();

    super_sg->property["sourceGraphCenter"].setValue( Vector3(sg->bbox().center()) );
    super_sg->property["targetGraphCenter"].setValue( Vector3(tg->bbox().center()) );

    /// NODES:
    // Correspond nodes in super graphs
    correspondSuperNodes();

    // Equalize resolution for corresponded nodes
    equalizeSuperNodeResolutions();

    // Equalize type for corresponded nodes
    equalizeSuperNodeTypes();

    // Visualization - color similar items
    /*foreach(Structure::Node * n, super_sg->nodes)
    {
        QString corrId = n->property["correspond"].toString();
        Structure::Node * other = super_tg->getNode(corrId);
        if(other) n->vis_property["color"].setValue( other->vis_property["color"] );
    }*/

    /// EDGES:
    // Correspond edges in super graphs
    correspondSuperEdges();

    // Assign edge attributes
    postprocessSuperEdges();

    active = super_sg;
}

void SuperGraph::postprocessSuperEdges()
{
    // Initial edge radius for all nodes = using actual delta
    foreach(Link * l, super_sg->edges) l->property["delta"].setValue( l->delta() );
    foreach(Link * l, super_tg->edges) l->property["delta"].setValue( l->delta() );

    // Set delta for edges of null nodes, because now we can not determine the
    // Null-to-Null: use the corresponded real delta
    // Null-to-Real:
    // >> One real: use the corresponded real delta
    // >> More real: put null at the centoid of relative joint to all real neighbours
    //               and the delta is computed thus from there
    foreach (Node* n, super_sg->nodes){
        if (n->id.contains("_null")){
            foreach(Link* sl, super_sg->getEdges( n->id )) {
                //Link* tl = super_tg->getEdge(sl->property["correspond"].toInt());
                sl->property["delta"].setValue( Vector3d(0,0,0) );
            }
        }
    }
    foreach (Node* n, super_tg->nodes){
        if (n->id.contains("_null")){
            foreach(Link* tl, super_tg->getEdges(n->id)) {
                Link* sl = super_sg->getEdge(tl->property["correspond"].toInt());
                tl->property["delta"].setValue( sl->delta() );
            }
        }
    }

    // Set blended delta for first time relinking
    foreach(Link * l, super_sg->edges) l->property["blendedDelta"].setValue( l->property["delta"].value<Vector3d>() );

    // Encode mesh position relative to skeleton
    foreach(Structure::Graph * g, (QVector<Structure::Graph*>() << super_sg << super_tg) )
    {
        foreach(Node * n, g->nodes)
        {
            QSharedPointer<SurfaceMeshModel> nodeMesh = n->property["mesh"].value< QSharedPointer<SurfaceMeshModel> >();

            if(!nodeMesh) continue;

            Vector3 v0 = nodeMesh->get_vertex_property<Vector3>(VPOINT)[Vertex(0)];
            Vector3 c0 = n->controlPoints().front();
            Vector3 deltaMesh = v0 - c0;
            n->property["deltaMesh"].setValue( deltaMesh );
        }
    }
}

void SuperGraph::equalizeSuperNodeResolutions()
{
    //图内先保持一直
    foreach(QVector<QString> group, super_sg->groups){
        int maxNum = -1;
        //每个node用相同数目的控制点进行控制
        foreach(QString nid, group) maxNum = qMax(maxNum, super_sg->getNode(nid)->numCtrlPnts());
        foreach(QString nid, group) super_sg->getNode(nid)->refineControlPoints(maxNum, maxNum);
    }

    foreach(QVector<QString> group, super_tg->groups){
        int maxNum = -1;
        foreach(QString nid, group) maxNum = qMax(maxNum, super_tg->getNode(nid)->numCtrlPnts());
        foreach(QString nid, group) super_tg->getNode(nid)->refineControlPoints(maxNum, maxNum);
    }
    //图与图之间也要有相同的分辨率
    foreach(QString snodeID, superNodeCorr.keys())
    {
        QString tnodeID = superNodeCorr[snodeID];

        // skip non-corresponded nodes
        if (snodeID.contains("_null") || tnodeID.contains("_null"))
            continue;

        Structure::Node* snode = super_sg->getNode(snodeID);
        Structure::Node* tnode = super_tg->getNode(tnodeID);

        //节点类型相同时，匹配两者的控制点的数目
        if (snode->type() == tnode->type())
        {
            if (snode->numCtrlPnts() < tnode->numCtrlPnts())
                snode->equalizeControlPoints(tnode);
            else
                tnode->equalizeControlPoints(snode);
        }
        // One sheet and one curve
        // Sheet has squared resolution of curve
        else
        {
            int sN, tN, betterN;

            if (snode->type() == Structure::SHEET)
            {
                sN = qMax(((Structure::Sheet*) snode)->numUCtrlPnts(), ((Structure::Sheet*) snode)->numVCtrlPnts());
                tN = tnode->numCtrlPnts();
                betterN = qMax(sN, tN);

                snode->refineControlPoints(betterN, betterN);
                tnode->refineControlPoints(betterN);
            }
            else
            {
                sN = snode->numCtrlPnts();
                tN = qMax(((Structure::Sheet*) tnode)->numUCtrlPnts(), ((Structure::Sheet*) tnode)->numVCtrlPnts());
                betterN = qMax(sN, tN);

                snode->refineControlPoints(betterN);
                tnode->refineControlPoints(betterN, betterN);
            }
        }
    }

    // Final phase  //最后还来这下干嘛！？？？有可能是……一个节点会同时和不同类型的节点匹配
    foreach(Node * snode, super_sg->nodes){
        Node * tnode = super_tg->getNode(snode->property["correspond"].toString());

        if (snode->type() == tnode->type())
        {
            if (snode->numCtrlPnts() < tnode->numCtrlPnts())
                snode->equalizeControlPoints(tnode);
            else
                tnode->equalizeControlPoints(snode);
        }
    }
}

void SuperGraph::equalizeSuperNodeTypes()
{
    // initial tags and collect the node pairs that need to be converted
    QMap<QString, QString> diffPairs;
    foreach(QString snodeID, superNodeCorr.keys())
    {
        QString tnodeID = superNodeCorr[snodeID];

        Structure::Node* snode = super_sg->getNode(snodeID);
        Structure::Node* tnode = super_tg->getNode(tnodeID);

        bool equalType = true;
        if (snode->type() != tnode->type())
        {
            equalType = false;
            diffPairs[snodeID] = tnodeID;
        }

        snode->property["type_equalized"] = equalType;
        tnode->property["type_equalized"] = equalType;
    }

    // convert sheet to curve if corresponded to curve
    // iteratively determine the orientation and location of the new curves
    while(!diffPairs.isEmpty())
    {
        foreach(QString snodeID, diffPairs.keys())
        {
            QString tnodeID = diffPairs[snodeID];

            Structure::Node* snode = super_sg->getNode(snodeID);

            // try to convert
            bool converted;
            if (snode->type() == Structure::SHEET)
            {
                converted = convertSheetToCurve(snodeID, tnodeID, super_sg, super_tg);
            }
            else
            {
                converted = convertSheetToCurve(tnodeID, snodeID, super_tg, super_sg);
            }

            // check if success
            if (converted)  diffPairs.remove(snodeID);
        }

    }

    // remove tags
    foreach(Structure::Node* n, super_sg->nodes) n->property.remove("type_equalized");
    foreach(Structure::Node* n, super_tg->nodes) n->property.remove("type_equalized");
}

// The sheet and curve should serve the same functionality in each shape
// which implies the curve(s) lay within the BB of the sheet
// some of the curves maintain the same connections to others
// we can relink the new curves (converted sheet) in the same way as the curve
bool SuperGraph::convertSheetToCurve( QString nodeID1, QString nodeID2, Structure::Graph* superG1, Structure::Graph* superG2 )
{
    Structure::Node* node1 = superG1->getNode(nodeID1);
    Structure::Node* node2 = superG2->getNode(nodeID2);

    Structure::Sheet *oldSheet = (Structure::Sheet*)node1;
    Structure::Curve *curve2 = (Structure::Curve*)node2;

    // Find a for-sure link (links with corresponded ends) to determine the new curve skeleton
    // 找到所有以这个Node为端点的边
    QVector<Structure::Link*> edges = superG2->getEdges(nodeID2);

    bool converted = false;

    Structure::Curve * newCurve = NULL;

    // Convert the sheet into a curve
    foreach(Structure::Link* link2, edges){
        Structure::Node* other2 = link2->otherNode(nodeID2);

        // choose type-equalized neighbours
        if (other2->property["type_equalized"].toBool()){
            //curve的邻居的对应Node的索引
            QString otherID1 = other2->property["correspond"].toString();

            Structure::Node* other1 = superG1->getNode(otherID1);//sheet上和curve的邻居对应的节点
            Vector4d otherCoord = link2->getCoordOther(nodeID2).front();//curve Node的邻居节点中控制点的坐标
            Vector3d linkOtherPos1 = other1->position(otherCoord);//sheet节点上邻居坐标的相对位置？

            Vector3d direction2 = curve2->direction();
            NURBS::NURBSCurved new_curve = oldSheet->convertToNURBSCurve(linkOtherPos1, direction2);

            // create new curve node with the same id
            newCurve = new Structure::Curve(new_curve, nodeID1 + "TEMP");

            // copy properties
            newCurve->property = node1->property;
            newCurve->property["original_sheet"].setValue( oldSheet );

            // mark the tag
            newCurve->property["type_equalized"] = true;
            node2->property["type_equalized"] = true;

            converted = true;

            break;
        }
    }

    if(!newCurve) return false;

    // Add the new node
    superG1->addNode( newCurve );
    newCurve->property["correspond"] = curve2->id;

    // Copy the edges
    foreach(Structure::Link* l, superG1->getEdges(oldSheet->id))
    {
        Node * n1 = newCurve;
        Node * n2 = l->otherNode( oldSheet->id );

        Array1D_Vector4d coordOnNew = Array1D_Vector4d(1, newCurve->approxCoordinates( l->position(oldSheet->id) ));

        superG1->addEdge(n1, n2, coordOnNew, l->getCoord(n2->id), superG1->linkName(n1,n2));
    }

    // Remove old node
    superG1->removeNode( oldSheet->id );

    // Replace ID for new node
    QString tempID = newCurve->id;
    foreach(Structure::Link* l, superG1->getEdges(tempID)) l->id.replace(tempID, nodeID1);
    newCurve->id = nodeID1;

    return converted;
}

Structure::Link * SuperGraph::addMissingLink( Structure::Graph *g, Structure::Link * link )
{
    Structure::Node * bn1 = link->n1;
    Structure::Node * bn2 = link->n2;
    Structure::Node * an1 = g->getNode(bn1->property["correspond"].toString());
    Structure::Node * an2 = g->getNode(bn2->property["correspond"].toString());
    LinkCoords c1 = link->getCoord(bn1->id);
    LinkCoords c2 = link->getCoordOther(bn1->id);

    Structure::Link * existEdge = g->getEdge(an1->id,an2->id);

    if(!existEdge)
        return g->addEdge(an1, an2, c1, c2, g->linkName(an1, an2));
    else
        return existEdge;
}

QVector<Structure::Link*> SuperGraph::edgesContain( QVector<Structure::Link*> edges, QString property_name )
{
    QVector<Structure::Link*> result;
    foreach(Structure::Link * edge, edges)
        if(edge->property.contains(property_name))
            result.push_back(edge);
    return result;
}

QVector<Structure::Link*> SuperGraph::edgesNotContain( QVector<Structure::Link*> edges, QString property_name )
{
    QVector<Structure::Link*> result;
    foreach(Structure::Link * edge, edges)
        if(!edge->property.contains(property_name))
            result.push_back(edge);
    return result;
}

void SuperGraph::debugSuperGraphs( QString info )
{
    // Visualization: assign global unique ids
    int viz_uid = 0;
    foreach(Structure::Link * slink, super_sg->edges){
        if(!slink->property.contains("correspond")) continue;
        Link* tlink = super_tg->getEdge(slink->property["correspond"].toInt());
        if(!tlink) continue;
        slink->property["viz_uid"] = tlink->property["viz_uid"] = viz_uid++;
    }

    toGraphviz(super_sg, info + "_SourceSuper", true, info + " Super Source");
    toGraphviz(super_tg, info + "_TargetSuper", true, info + " Super Target");
}

void SuperGraph::saveMatchedGraph(QString fileName){
    //super_sg->saveToFile(QString("source.xml"));
    //super_tg->saveToFile(QString("target.xml"));
    if(tg->nodes.size() < 1) return;


    QFile file(fileName);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) return;
    QFileInfo fileInfo(file.fileName());
    QDir graphDir( fileInfo.absolutePath() );

    QTextStream out(&file);
    out << "<?xml version=\"1.0\" encoding=\"UTF-8\" ?>\n<document>\n";

    // Save nodes
    foreach(Node * node, super_sg->nodes)
    {
        QString corrID = node->property["correspond"].toString();
        Structure::Node *n = super_tg->getNode(corrID);

        out << "<node>\n";

        // Type and ID
        out << QString("\t<id>%1</id>\n").arg(n->id);
        out << QString("\t<type>%1</type>\n").arg(n->type());

        if(n->property.contains("mesh_filename"))
        {
            QString meshesFolder = "meshes";

            graphDir.mkdir( meshesFolder );

            QString mesh_filename = n->property["mesh_filename"].toString();
            QFileInfo meshFileInfo( mesh_filename );

            QString relativeFileName = meshesFolder + "/" + meshFileInfo.baseName() + ".obj";

            // Save mesh from memory
            /*QSharedPointer<SurfaceMeshModel> mesh = n->property["mesh"].value< QSharedPointer<SurfaceMeshModel> >();
            if(mesh_filename.size() && mesh)
            {
                if( sg->isOutParts )
                    saveOBJ( mesh.data(), graphDir.path() + "/" + relativeFileName );
            }*/

            out << QString("\t<mesh>%1</mesh>\n\n").arg( relativeFileName ); // relative
        }

        // Control count
        out << "\t<controls>\n";
        std::vector<int> controlCount = n->controlCount();
        foreach(int c, controlCount)
            out << QString("\t\t<c>%1</c>\n").arg(c);
        out << "\t</controls>\n\n";

        // Control Points
        foreach(Vector3d p, n->controlPoints())
            out << QString("\t<point>%1 %2 %3</point>\n").arg(p.x()).arg(p.y()).arg(p.z());

        // Weights
        out << "\n\t<weights>";
        foreach(Scalar w, n->controlWeights())
            out << QString::number(w) + " ";
        out << "</weights>\n";

        out << "</node>\n\n";
    }

    // Save edges
    foreach(Link * edge, super_sg->edges)
    {
        int edgeID = edge->property["correspond"].toInt();
        Link *e = super_tg->getEdge(edgeID);

        out << "<edge>\n";
        out << QString("\t<id>%1</id>\n").arg(e->id);
        out << QString("\t<type>%1</type>\n\n").arg(e->type);
        out << QString("\t<n>%1</n>\n").arg(e->n1->id);
        out << QString("\t<n>%1</n>\n").arg(e->n2->id);

        for(int k = 0; k < 2; k++)
        {
            out << "\t<coord>\n";
            foreach(Vector4d c, e->coord[k])
                out << "\t\t<uv>" << c[0] << " " << c[1] << " " << c[2] << " " << c[3] << "</uv>\n";
            out << "\t</coord>\n";
        }

        out << "</edge>\n\n";
    }

    // Save groups
    foreach(QVector<QString> group, super_tg->groups){
        out << "<group>";
        foreach(QString nid, group){
            out << QString("\n\t<n>%1</n>").arg(nid);
        }
        out << "\n</group>\n\n";
    }

    out << "\n</document>\n";
    file.close();
}


