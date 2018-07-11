#include "SeedRegion.h"

using namespace Structure;
using namespace NURBS;

SeedRegion::SeedRegion(Structure::Graph * activeGraph, Structure::Graph * targetGraph, QString node_ID)
{
    // Task properties
    this->active = activeGraph;
    this->target = targetGraph;

    property["nodeID"].setValue(node_ID);
    nodeID = node_ID;

}

void SeedRegion::ComputeSeedRegion(){
    Node * n = node();
    if(!n)return;

    QString type = node()->type();
    if (type == Structure::CURVE)
    {
        // Find edges to be used in growing
        QVector<Link*> tedges;
        // 在Curve中考虑所有的边
        QVector<Link*> edges = filteredFromTargetEdges();//active中 和target中的node相连的边  匹配的边的集合，并且是按照价排序过的
        // 怎么会出现edge数目为 0的情况呢
        assert(!edges.empty());
        foreach(Link* edge, edges){
            Structure::Link* t_edge = target->getEdge( edge->property["correspond"].toInt() );
            assert(t_edge);
            tedges.push_back(t_edge);//target中的node相连的边
        }

        // Save the edges used
        property["edges"].setValue( active->getEdgeIDs(edges) );

        if(tedges.size() == 1 )
            SeedCurveOneEdge(tedges.front());//这里有bug，tedges传进去是空的
        else
            SeedCurveTwoEdges(tedges);


    }else if(type == Structure::SHEET){

        //在sheet中最多只考虑两个边
        QVector<Structure::Link*> active_edges = active->getEdges(n->id);
        //猜测原因是sheet不能变形，只需要两个点就可以确定一个sheet
        QVector<Link*> edges = filterEdges(n, active_edges );//这里最多是是两个边（什么破函数……）

        property["edges"].setValue( active->getEdgeIDs(edges) );
        assert(!edges.empty());

        if (edges.size() == 1){
            SeedSheetOneEdge( edges.front() );
        }else{
            SeedSheetTwoEdges( edges.front(), edges.back() );
        }

    }


}

void SeedRegion::SeedCurveOneEdge(Structure::Link * tlink){
    //Jerry's debug
    assert(tlink != NULL);

    Node *n = node(), *tn = targetNode();
    Structure::Curve * curve = (Structure::Curve *)n;
    Structure::Curve * tcurve = (Structure::Curve *)tn;

    // Get base
    Node * tbase = tlink->otherNode(tn->id);
    QString baseID = tbase->property["correspond"].toString();
    Node* base = active->getNode(baseID);//准备和 null连接的node

    // Get coordinates on me and my base
    Vector4d coordBase = tlink->getCoord(tbase->id).front();
    Vector4d coordSelf = tlink->getCoord(tn->id).front();//为什么就是第一个了呢……

    /// Place curve:
    int cpIDX = curve->controlPointIndexFromCoord( coordSelf );

    // Make origin the position on me in which I will grow from
    //把target的curve转移到base的位置
    curve->foldTo(coordSelf, true);//直接折叠到自己的位置的那个点上
    Vector3 cntP = -curve->controlPoints()[cpIDX];
    Vector3 bP = base->position( coordBase );
    curve->moveBy(cntP + bP /*-tcurve.controlPoints()[cpIDX] + base->position( coordBase )*/ );

}

void SeedRegion::SeedCurveTwoEdges(QVector<Structure::Link*> tedges){
    Node *n = node(), *tn = targetNode();
    Structure::Curve * curve = (Structure::Curve *)n;
    Structure::Curve * tcurve = (Structure::Curve *)tn;
     //就是这里了！滴滴~学生卡

    // Make sure the edges are reasonable for curve encoding
         QMap< double, QPair<int,int> > edgePairs;
         for(int i = 0; i < tedges.size(); i++){
             Link *tlinkA = tedges[i];
             for(int j = i + 1; j < tedges.size(); j++){
                 Link *tlinkB = tedges[j];
                 double dist = (tlinkA->position(tn->id) - tlinkB->position(tn->id)).norm();//暂时认为是 两个 和选中的node相邻的node之间的最近点 的距离
                 edgePairs[dist] = qMakePair(i,j);
             }
         }

    // Prefer most distant edges
         double largestDist = edgePairs.keys().last();
         if( largestDist < 1e-6 ){ //如果几个相连的边都很近，那随便找一个相邻的node就行了
             SeedCurveOneEdge( tedges.front() );
             return;
         }

    // Links, nodes and positions on the TARGET
         Link *tlinkA = tedges[ edgePairs[largestDist].first ];//距离最大的第一个node对应的边
         Link *tlinkB = tedges[ edgePairs[largestDist].second ];//距离最大的第二个node对应的边
         Node *totherA = tlinkA->otherNode( tn->id );//距离最大的两个node
         Node *totherB = tlinkB->otherNode( tn->id );
         Vector4d othercoordA = tlinkA->getCoord(totherA->id).front();//这两个node和目标node的最近点
         Vector4d othercoordB = tlinkB->getCoord(totherB->id).front();

         // Corresponding stuff on ACTIVE
         Link *linkA = active->getEdge( tlinkA->property["correspond"].toInt() );//active上对应的边
         Link *linkB = active->getEdge( tlinkB->property["correspond"].toInt() );

         assert(linkA && linkB);

         Node *otherA = active->getNode( totherA->property["correspond"].toString() );//active上对应的node
         Node *otherB = active->getNode( totherB->property["correspond"].toString() );
         Vector3d pointA = otherA->position(othercoordA);//active上对应的那个最近点……哦！用corrd就可以找到相对坐标了！
         Vector3d pointB = otherB->position(othercoordB);

         // Geodesic distance between two link positions on the active graph excluding the running tasks
         QVector<QString> excludeNodes = active->property["activeTasks"].value< QVector<QString> >();

         GraphDistance gd( active, excludeNodes );
         gd.computeDistances( pointA, DIST_RESOLUTION );
         QVector< GraphDistance::PathPointPair > path;
         NodeCoord rpoint( otherB->id, othercoordB );
         gd.smoothPathCoordTo( rpoint, path );//pointA到pointB的最短路径

         // Very short paths
         if(path.size() < 4){
             SeedCurveOneEdge( tedges.front() );
             return;
         }

         // Use the center of the path as the start point//****************
         GraphDistance::PathPointPair startPointCoord = path[path.size() / 2];
         Vector3d startPoint = startPointCoord.position( active );

         // Initial position of curve node
         Vector4d midCoord(0.5,0.5,0.5,0.5);
         curve->foldTo(midCoord, true);
         curve->curve.translate(startPoint - curve->position(midCoord));

         return;

 }

void SeedRegion::SeedSheetOneEdge( Structure::Link * l )
{
    Node * n = node();//active上的目标n
    Node *tn = targetNode();
    Node * base = l->otherNode(n->id);//唯一一个和n相连的node
    Structure::Sheet* structure_sheet = ((Structure::Sheet*)n);
    Vector4d coord = l->getCoord(n->id).front();
    Vector4d otherCoord = l->getCoordOther(n->id).front();//coord 就是intersect point

    // Placement: 直接平移到和它相邻的那个node的接触点去
    structure_sheet->moveBy( base->position(otherCoord) - n->position(coord) );

}

void SeedRegion::SeedSheetTwoEdges( Structure::Link * linkA, Structure::Link * linkB )
{
    Structure::Node * n = node();
    Structure::Node * tn = targetNode();

    Structure::Link * tlinkA = target->getEdge( linkA->property["correspond"].toInt() );
    Structure::Link * tlinkB = target->getEdge( linkB->property["correspond"].toInt() );

    Vector3d pointA = linkA->positionOther(n->id);
    Vector3d pointB = linkB->positionOther(n->id);

    // Geodesic distance between two link positions on the active graph excluding the running tasks
    QVector<QString> exclude = active->property["activeTasks"].value< QVector<QString> >();
    GraphDistance gd( active, exclude );
    gd.computeDistances( pointA, DIST_RESOLUTION );
    QVector< GraphDistance::PathPointPair > path;
    gd.smoothPathCoordTo(pointB, path);

    // Otherwise, self expand / contract
    if(path.size() < 3)
    {
        QVector<Link*> twoEdges(2);
        twoEdges[0] = linkA; twoEdges[1] = linkB;
        Structure::Link * pickedLink = preferredEnd(n, twoEdges, active);
        SeedSheetOneEdge( pickedLink );

        QVector<Link*> edges; edges.push_back(pickedLink);
        property["edges"].setValue( active->getEdgeIDs(edges) );
        return;
    }

    // Use the center of the path as the start point
    GraphDistance::PathPointPair startPointCoord = path[path.size() / 2];
    Vector3d startPoint = startPointCoord.position( active );//取中点当作开始的点，为什么要有开始点……

    // Separate the path into two for linkA and linkB
    int N = path.size(), hN = N / 2;
    if (N %2 == 0) path.insert(hN, path[hN]);

    QVector<GraphDistance::PathPointPair> pathA, pathB;
    for (int i = 0; i < hN; i++)
    {
        pathA.push_back(path[hN+1+i]);
        pathB.push_back(path[hN-1-i]);
    }

    //选择和两个相邻node的邻接点，用来产生新的sheet的信息，但是为什么是curve……
    //咦~为什么是target来着……反正就是只要用一下size就可以啦嗯！
    SheetEncoding cpCoords = encodeSheetAsCurve((Structure::Sheet*)tn, tlinkA->position(tn->id), tlinkB->position(tn->id));

    // Initial position and geometry  产生n=cpCoords.size()大小的vector，其每个值为startPoint
    n->setControlPoints( Array1D_Vector3(cpCoords.size(), startPoint) );

}

QVector<Structure::Link*> SeedRegion::filterEdges( Structure::Node * n, QVector<Structure::Link*> allEdges )
{
    // Check edge's real ownership //保险起见检查一遍，看是不是都跟target匹配
    Node * tn = target->getNode(n->property["correspond"].toString());
    if( false/*tn*/ )
    {
        QVector<Link*> keepEdges;
        foreach(Link * sl, allEdges)
        {   // 还是要从target匹配过来
            Link * tl = target->getEdge( sl->property["correspond"].toInt() );

            // Skip target edges that are not related to current node
            if(!tl->hasNode(tn->id))
                continue;
            else
                keepEdges.push_back(sl);
        }
        allEdges = keepEdges;
    }

    QVector<Structure::Link*> edges = allEdges;
    //找到所有和他相连的边，删除没长大的边
    /*for(int i = 0; i < (int)allEdges.size(); i++){
        Structure::Node * otherI = allEdges[i]->otherNode(n->id);

        if( (!otherI) || ungrownNode(otherI->id) ) continue;

        if(otherI) edges.push_back(allEdges[i]);
    }*/

    // Bin edges by their coordinates into 4 locations (2 for curves)
    QMap< int, QVector<Structure::Link*> > bin;
    foreach(Structure::Link * l, edges){
        Vector4d coord = l->getCoord(n->id).front();
        int idx = coord[0] > 0.5 ? (coord[1] > 0.5 ? 3 : 1) : (coord[1] > 0.5 ? 2 : 0);
        bin[idx].push_back(l);//把边分成4个走向吧
    }

    edges.clear();

    foreach( int i, bin.keys() ){
        QVector<Structure::Link*> similarEdges = bin[i];//走向相同的边

        // Arbitrary choice for now..
        Structure::Link * furthest = similarEdges.front();

        if(edges.size() < 2) edges.push_back( furthest );//随……随便找方向相反的两个边？？？
    }

    return edges;
}
//算seed region的时候多的那个是target
QVector<Structure::Link*> SeedRegion::filteredFromTargetEdges(){
    Node *tn = targetNode();
    QVector<Link*> all_tedges = target->getEdges(tn->id);//链接targetnode的所有边

    // Do not consider edges with non-ready nodes
    QVector<Link*> tedges, edges;
    foreach(Link* edge, all_tedges)
    {
        Node * tother = edge->otherNode( tn->id );
        Node * other = active->getNode( tother->property["correspond"].toString() );

        // Skip not grown others
        // target在这里是一个集合，不需要grow
        //if( all_tedges.size() > 1 && ungrownNode(other->id) )
        //    continue;

        tedges.push_back(edge);	//链接targetnode的所有边，除去active这边还没长大的 node
    }

    // Skip all but one edge of splitting nodes
    /*foreach(Link * edge, tedges)
    {
        Node * other = active->getNode( edge->otherNode( tn->id )->property["correspond"].toString() );
        if(other->property["taskTypeReal"].toInt() == SPLIT && !other->property["taskIsReady"].toBool()){
            if(tedges.size() > 1)
                tedges.remove(tedges.indexOf(edge));
        }
    }*/

    // Find corresponding edges 把target中对应的edge连起来
    foreach(Link * edge, tedges)
    {
        Link * slink = active->getEdge( edge->property["correspond"].toInt() );//active中匹配的边

        // The edge has been removed, possibly by Task::postDone
        if(!slink)
        {
            Node * n1 = active->getNode(edge->n1->property["correspond"].toString());
            Node * n2 = active->getNode(edge->n2->property["correspond"].toString());
            Array1D_Vector4d c1 = edge->coord.front();
            Array1D_Vector4d c2 = edge->coord.back();

            // Bring it back
            slink = active->addEdge(n1, n2, c1, c2, active->linkName(n1,n2));

            // Correspond
            slink->property["correspond"] = edge->property["uid"].toInt();
            edge->property["correspond"] = slink->property["uid"].toInt();
        }

        if(slink && slink->hasNode(nodeID)) edges.push_back( slink );
    }

    // Sort by valence of other
    {
        QMap<Link*,int> linkValence;
        foreach(Link * l, edges){
            Node * tn = target->getNode( l->otherNode(node()->id)->property["correspond"].toString() );
            linkValence[l] = qMax(active->valence(l->otherNode(node()->id)), target->valence( tn ));
        }

        typedef QPair<int, Link*> ValenceLink;
        QList< ValenceLink > sorted = sortQMapByValue(linkValence);

        QVector<Link*> sortedEdges;
        foreach(ValenceLink vl, sorted) sortedEdges.push_back( vl.second );
        edges = sortedEdges;
    }

    // Surrounded by un-grown nodes 这样的node可能在target中没有对应的边
    if( edges.isEmpty() )
    {
        QVector<Structure::Link*> edgs = active->getEdges( nodeID );
        if(!edgs.isEmpty()) edges.push_back( edgs.front() );
    }

    if(tedges.size() > 4){
        int abc = 1;
        int bcd;
        bcd = abc - 1;
    }

    assert(!edges.empty());

    return edges;//active中 和target中的node相连的边  匹配的边的集合，并且是按照价排序过的
}


SheetEncoding SeedRegion::encodeSheetAsCurve( Structure::Sheet * sheet, Vector3 start, Vector3 end)
{
    Array1D_Vector3 controlPoints = sheet->controlPoints();
    return Structure::Curve::encodeCurve(controlPoints,start,end);
}

Structure::Node* SeedRegion::targetNode(){
    Node* n = node();
    if(!n->property.contains("correspond")) return NULL;
    return target->getNode( node()->property["correspond"].toString() );
}

Structure::Link * SeedRegion::preferredEnd(Structure::Node * n, QVector<Structure::Link*> edges, Structure::Graph * g)
{
    // Pick end with more valence
    int maxValence = 0;
    Structure::Link * preferredLink = edges.front();
    foreach(Structure::Link* edge, edges){
        Structure::Node * otherNode = edge->otherNode( n->id );
        int curValence = g->valence(otherNode) * (otherNode->type() == Structure::CURVE ? 1 : 100 );
        if(curValence > maxValence){
            maxValence = curValence;
            preferredLink = edge;
        }
    }
    return preferredLink;
}


bool SeedRegion::ungrownNode( QString nid )
{
    //Task* t = active->getNode(nid)->property["task"].value<Task*>();
    //return t->type == GROW && !t->isReady;
    //这里要获取其他的node需要进行的操作的的信息，需要进一步计算
    return nid.contains("_null") ;

}
