#include "GraphCorresponder.h"

#include <QFile>
#include <algorithm>
#include <fstream>

#include "GraphDistance.h"

#define INVALID_VALUE -1

GraphCorresponder::GraphCorresponder( Structure::Graph *source, Structure::Graph *target )
{
	this->sg = source;
	this->tg = target;

	spactialW = 0.5;
	structuralW = 1.0;
	sizeW = 0.0;
	orientationW = 0.0;

	scoreThreshold = 0.4f;

	// Useful for testing
	if( qMin(source->nodes.size(), target->nodes.size()) < 3 ) scoreThreshold = 1.0f;

	sIsLandmark.resize(sg->nodes.size(), false);
	tIsLandmark.resize(tg->nodes.size(), false);

    isReady = false;
}

// Helper functions
QString baseName(QString s){
	QString result;
	QFileInfo fname(s);
	result = fname.baseName();
	return result;
}

QString GraphCorresponder::sgName()
{
	return baseName(sg->property["name"].toString());
}

QString GraphCorresponder::tgName()
{
	return baseName(tg->property["name"].toString());
}

// Matrix operations
template <class Type>
void GraphCorresponder::initializeMatrix(std::vector< std::vector<Type> > & M, Type value)
{
	int sN = sg->nodes.size();
	int tN = tg->nodes.size();
	std::vector<Type> tmp(tN, value);
	M.clear();
	M.resize(sN, tmp);
}

void GraphCorresponder::normalizeMatrix(MATRIX & M)
{
	float maxDis = -1;

	// Find the maximum value
	int sN = sg->nodes.size();
	int tN = tg->nodes.size();
	for(int i = 0; i < sN; i++){
		for (int j = 0; j < tN; j++)
		{
			if (M[i][j] != INVALID_VALUE && M[i][j] > maxDis)
				maxDis = M[i][j];
		}
	}

	// Normalize
	for(int i = 0; i < sN; i++){
		for (int j = 0; j < tN; j++)
		{
			if (M[i][j] != INVALID_VALUE)
			{
				M[i][j] /= maxDis;
			}
		}
	}
}

bool GraphCorresponder::minElementInMatrix( MATRIX &M, int &row, int &column, float &minValue )
{
	if (M.size() == 0 || M[0].size() == 0)
	{
		qDebug()  << "Warning: minElementInMatrix: the input matrix cannot be empty.";
		return false;
	}

	minValue = FLT_MAX;
	for (int i = 0; i < (int)M.size(); i++){
		for (int j = 0; j < (int)M[0].size(); j++)
		{
            if (M[i][j] != INVALID_VALUE && M[i][j] < minValue)
			{
				minValue = M[i][j];
				row = i;
				column = j;
			}
		}
	}

	return minValue != FLT_MAX;
}

bool GraphCorresponder::minUnusedInMatrix( MATRIX &M, std::vector< std::vector<bool> > &U, int &row, int &column, float &minValue )
{
    if (M.size() == 0 || M[0].size() == 0)
    {
        qDebug()  << "Warning: minElementInMatrix: the input matrix cannot be empty.";
        return false;
    }

    minValue = FLT_MAX;
    for (int i = 0; i < (int)M.size(); i++){
        for (int j = 0; j < (int)M[0].size(); j++)
        {
            if (M[i][j] != INVALID_VALUE && U[i][j] != true && M[i][j] < minValue)
            {
                minValue = M[i][j];
                row = i;
                column = j;
            }
        }
    }

    return minValue != FLT_MAX;
}

// Point Landmarks
void GraphCorresponder::addPointLandmark()
{
	QVector<POINT_ID> sSelections = sg->selectedControlPointsByColor(Qt::green);
	QVector<POINT_ID> tSelections = tg->selectedControlPointsByColor(Qt::green);

	if (sSelections.empty() || tSelections.empty()) 
	{
		qDebug() << "You need select landmarks on both the source and target graphs.";
		return;
	}

	pointLandmarks.push_back(std::make_pair(sSelections, tSelections));

	// Clear the selections
	sg->clearSelections();
	tg->clearSelections();
}

void GraphCorresponder::prepareOneToOnePointLandmarks()
{
	// Clear
	sPointLandmarks.clear();
	tPointLandmarks.clear();

    //vector对vector变成点对点
	// Prepare
	foreach (POINT_LANDMARK pLandmark, pointLandmarks)
	{
		QVector<POINT_ID> sIDs = pLandmark.first;
		QVector<POINT_ID> tIDs = pLandmark.second;

		// One to many
		if (sIDs.size() == 1)
		{
			POINT_ID sID = sIDs.front();

			foreach( POINT_ID tID, tIDs)
			{
				sPointLandmarks.push_back(sID);
				tPointLandmarks.push_back(tID);
			}
		}
		// Many to one
		else if (tIDs.size() == 1)
		{
			POINT_ID tID = tIDs.front();

			foreach( POINT_ID sID, sIDs)
			{
				sPointLandmarks.push_back(sID);
				tPointLandmarks.push_back(tID);
			}
		}
	}
}

QVector< QVector<double> > GraphCorresponder::computeLandmarkFeatures( Structure::Graph *g, QVector<POINT_ID> &pointLandmarks )
{
	QVector< QVector<double> > result_features(g->nodes.size(), QVector<double>());
    //pointLandMark意思是某个编号对应的node对应的边界点
    //POINT_ID 第一个是nodeID， 第二个是这个node中的控制点的序号
	foreach (POINT_ID landmark, pointLandmarks)
    {   //每个点作为起始点，算一遍到每个node的距离贡献
		Vector3 startpoint = g->nodes[landmark.first]->controlPoint(landmark.second);

		GraphDistance gd(g);
		gd.computeDistances(startpoint, DIST_RESOLUTION);

		for (int nID = 0; nID < (int)g->nodes.size(); nID++)
        {   //每个node的中心到沿着control point 到某一个控制点【startpoint】之间的迪杰斯特拉距离距离
			double dis = gd.distance(g->nodes[nID]->center());

			result_features[nID].push_back(dis);
		}
	}

	return result_features;
}

double GraphCorresponder::distanceBetweenLandmarkFeatures( QVector<double> sFeature, QVector<double> tFeature )
{
	if (sFeature.size() != tFeature.size())
	{
		qDebug() << "Landmark features have different dimensions.";
		return -1;
	}

	// Euclidean distance
	double dis = 0;
	for (int i = 0; i < (int) sFeature.size(); i++)
	{
		double dif = sFeature[i] - tFeature[i];
		dis += dif * dif;
	}

	return sqrt(dis);
}

void GraphCorresponder::computeLandmarkFeatureMatrix( MATRIX & M )
{
	// One to one point landmarks
	prepareOneToOnePointLandmarks();

	// In case there are no point landmarks
	if(sPointLandmarks.empty())
	{
		initializeMatrix<float>(M, 0.0);
		return;
	}

	// Landmark features
	sLandmarkFeatures = computeLandmarkFeatures(sg, sPointLandmarks);
	tLandmarkFeatures = computeLandmarkFeatures(tg, tPointLandmarks);

	// Compute differences between features
	initializeMatrix<float>(M, INVALID_VALUE);

	int sN = sg->nodes.size();
	int tN = tg->nodes.size();
	for(int i = 0; i < sN; i++){
		for (int j = 0; j < tN; j++)
		{
			if (validM[i][j])
			{
				M[i][j] = distanceBetweenLandmarkFeatures(sLandmarkFeatures[i], tLandmarkFeatures[j]);
			}
		}
	}

	normalizeMatrix(M);
}

// Part landmarks
void GraphCorresponder::addLandmarks( QVector<QString> sParts, QVector<QString> tParts )
{
	// Mark those parts
	foreach(QString strID, sParts)
	{
		Structure::Node * snode = sg->getNode(strID);
		if(!snode) continue;

		int idx = snode->property["index"].toInt();
		sIsLandmark[idx] = true;

		// Make sure its not in list of non-corresponding
		nonCorresS.remove(idx);
		clearFor(strID, true);
	}

	foreach(QString strID, tParts)
	{
		Structure::Node * tnode = tg->getNode(strID);
		if(!tnode) continue;

		int idx = tnode->property["index"].toInt();
		tIsLandmark[idx] = true;

		nonCorresT.remove(idx);
		clearFor(strID, false);
	}

	// Store correspondence
	landmarks.push_back(std::make_pair(sParts, tParts));
}

void GraphCorresponder::addCorrespondences( QVector<QString> sParts, QVector<QString> tParts, float presetScore )
{
	if(!sParts.size() && !tParts.size()) return;

	// Check if those parts are available
	foreach(QString strID, sParts)
	{
		Structure::Node * node = sg->getNode(strID);
		if (!node)
		{
			qDebug() << "Add landmarks: " << strID << " doesn't exist in the source graph.";
			return;
		}
		int idx = node->property["index"].toInt();
		if (sIsCorresponded[idx])
		{
			qDebug() << "Add landmarks: " << strID << " in the source graph has already been corresponded.";
			return;
		}
	}

	foreach(QString strID, tParts)
	{
		Structure::Node * node = tg->getNode(strID);
		if (!node)
		{
			qDebug() << "Add landmarks: " << strID << " doesn't exist in the target graph.";
			return;
		}
		int idx = node->property["index"].toInt();
		if (tIsCorresponded[idx])
		{
			qDebug() << "Add landmarks: " << strID << " in the target graph has already been corresponded.";
			return;
		}
	}

	// Mark those parts
	foreach(QString strID, sParts)
	{
		int idx = sg->getNode(strID)->property["index"].toInt();
		sIsCorresponded[idx] = true;
	}

	foreach(QString strID, tParts)
	{
		int idx = tg->getNode(strID)->property["index"].toInt();
		tIsCorresponded[idx] = true;
	}

	// Store correspondence
	PART_LANDMARK vector2vector = std::make_pair(sParts, tParts);
	insertCorrespondence( vector2vector );

	// Set to specific score
	int N = qMax(sParts.size(), tParts.size());
	corrScores[vector2vector] = std::vector<float>(N, presetScore);
}

void GraphCorresponder::removeLandmark( int id )
{
	if (id > (int)landmarks.size()) return;


	PART_LANDMARK vector2vector = landmarks[id];

	// Update isLandmark
	foreach(QString strID, vector2vector.first)
	{
		int idx = sg->getNode(strID)->property["index"].toInt();
		sIsLandmark[idx] = false;
	}

	foreach(QString strID, vector2vector.second)
	{
		int idx = tg->getNode(strID)->property["index"].toInt();
		tIsLandmark[idx] = false;
	}


	// Erase
	landmarks.erase(landmarks.begin() + id);
}

void GraphCorresponder::computeValidationMatrix()
{
	initializeMatrix<bool>(validM, true);

	int sN = sg->nodes.size();
	int tN = tg->nodes.size();

	// Types
	for(int i = 0; i < sN; i++){
		for (int j = 0; j < tN; j++)
		{
			if (sg->nodes[i]->type() != tg->nodes[j]->type())
				validM[i][j] = false;
		}
	}

    // Landmarks //标记好的匹配点，landmark内部存储的是QString类型的nodeId
	foreach (PART_LANDMARK vector2vector, landmarks)
	{
		// Convert strID to int id
		std::set<int> sIDs, tIDs;
		foreach (QString strID, vector2vector.first)
			sIDs.insert(sg->getNode(strID)->property["index"].toInt());
		foreach (QString strID, vector2vector.second)
			tIDs.insert(tg->getNode(strID)->property["index"].toInt());

		// Set distances to invalid value
		foreach (int r, sIDs)
			for (int c = 0; c < (int)tg->nodes.size(); c++)
				validM[r][c] = false;

		foreach (int c, tIDs)
			for (int r = 0; r < (int)sg->nodes.size(); r++)
				validM[r][c] = false;
	}
}

void GraphCorresponder::saveLandmarks(QString filename)
{
	QFile file(filename);
	if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) return;
	QTextStream outF(&file);

	outF << landmarks.size() << "\n\n";

	foreach (PART_LANDMARK vector2vector, landmarks)
	{
		outF << vector2vector.first.size() << '\t';;
		foreach (QString strID, vector2vector.first)
			outF << strID << '\t';
		outF << '\n';

		outF << vector2vector.second.size() << '\t';
		foreach (QString strID, vector2vector.second)
			outF << strID << '\t';
		outF << "\n\n";
	}

	file.close();
}

void GraphCorresponder::clear()
{
	sIsLandmark.clear();
	tIsLandmark.clear();

	disM.clear();
	validM.clear();
	spatialM.clear();

	landmarks.clear();
	correspondences.clear();
	corrScores.clear();

	sIsCorresponded.clear();
	tIsCorresponded.clear();

	nonCorresS.clear();
	nonCorresT.clear();
	
	sIsCorresponded.clear();
	tIsCorresponded.clear();

	sIsCorresponded.resize(sg->nodes.size(), false);
	tIsCorresponded.resize(tg->nodes.size(), false);
	sIsLandmark.resize(sg->nodes.size(), false);
	tIsLandmark.resize(tg->nodes.size(), false);

	foreach(Structure::Node * n, sg->nodes) n->property.remove("correspond");
	foreach(Structure::Node * n, tg->nodes) n->property.remove("correspond");

	this->isReady = false;

	emit( cleared() );
}

void GraphCorresponder::loadLandmarks(QString filename)
{
	QFile file(filename);
	if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) return;
	QTextStream inF(&file);

	clear();

	int nbCorr;
	inF >> nbCorr;

	for (int i = 0; i < nbCorr; i++)
	{
		int n;
		QString strID;
		QVector<QString> sParts, tParts;

		inF >> n;
		for (int j = 0; j < n; j++)
		{
			inF >> strID;
			sParts.push_back(strID);
		}

		inF >> n;
		for (int j = 0; j < n; j++)
		{
			inF >> strID;
			tParts.push_back(strID);
		}

		this->addLandmarks(sParts, tParts);
	}

	file.close();
}

void GraphCorresponder::computeHausdorffDistanceMatrix( MATRIX & M )
{
	initializeMatrix<float>(M, INVALID_VALUE);

	// The control points of each node in the target graph
	Array2D_Vector3 targetControlPointsArray;
	foreach (Structure::Node *tNode, this->tg->nodes)
		targetControlPointsArray.push_back(tNode->controlPoints());

	// The Hausdorff distance
	int sN = sg->nodes.size();
	int tN = tg->nodes.size();
	for(int i = 0; i < sN; i++)
	{
		std::vector<Vector3> sPoints = sg->nodes[i]->controlPoints();
		for (int j = 0; j < tN; j++)
		{
			if (validM[i][j])
			{
				std::vector<Vector3> &tPoints = targetControlPointsArray[j];
				M[i][j] = HausdorffDistance(sPoints, tPoints);
			}
		}
	}

	normalizeMatrix(M);
}


// Size and orientation matrices
void GraphCorresponder::computeSizeDiffMatrix( MATRIX & M )
{
	initializeMatrix<float>(M, INVALID_VALUE);

	int sN = sg->nodes.size();
	int tN = tg->nodes.size();
	for(int i = 0; i < sN; i++){
		for (int j = 0; j < tN; j++)
		{
			if (validM[i][j])
			{
				float diff;
				if (sg->nodes[i]->type() == Structure::CURVE)
				{
					Structure::Curve *curve1 = (Structure::Curve *) sg->nodes[i];
					Structure::Curve *curve2 = (Structure::Curve *) tg->nodes[j];
					diff = curve1->curve.GetLength(0, 1) - curve2->curve.GetLength(0, 1);
				}
				else
				{
					Structure::Sheet *sheet1 = (Structure::Sheet *) sg->nodes[i];
					Structure::Sheet *sheet2 = (Structure::Sheet *) tg->nodes[j];
					diff = sheet1->area() - sheet2->area();
				}

				M[i][j] = fabsf(diff);
			}
		}
	}

	normalizeMatrix(M);
}

void GraphCorresponder::computeOrientationDiffMatrix( MATRIX & M )
{
	initializeMatrix<float>(M, INVALID_VALUE);

	int sN = sg->nodes.size();
	int tN = tg->nodes.size();
	for(int i = 0; i < sN; i++){
		for (int j = 0; j < tN; j++)
		{
			if (validM[i][j])
			{
				Vector3 vec1, vec2;
				if (sg->nodes[i]->type() == Structure::CURVE)
				{
					Structure::Curve *curve1 = (Structure::Curve *) sg->nodes[i];
					Structure::Curve *curve2 = (Structure::Curve *) tg->nodes[j];
					std::vector<Vector3> ctrlPnts1 = curve1->controlPoints();
					std::vector<Vector3> ctrlPnts2 = curve2->controlPoints();

					vec1 = ctrlPnts1.front() - ctrlPnts1.back();
					vec2 = ctrlPnts2.front() - ctrlPnts2.back();
				}
				else
				{
					Structure::Sheet *sSheet = (Structure::Sheet *) sg->nodes[i];
					Structure::Sheet *tSheet = (Structure::Sheet *) tg->nodes[j];
					Array2D_Vector3 sCtrlPoint = sSheet->surface.mCtrlPoint;
					Array2D_Vector3 tCtrlPoint = tSheet->surface.mCtrlPoint;
					Vector3 scenter = sSheet->center();
					Vector3 tcenter = tSheet->center();

					// Get the extreme points.
					Vector3 s00 = sCtrlPoint.front().front();
					Vector3 s01 = sCtrlPoint.front().back();
					Vector3 s10 = sCtrlPoint.back().front();
					Vector3 sU = s10 - s00;
					Vector3 sV = s01 - s00;

					Vector3 t00 = tCtrlPoint.front().front();
					Vector3 t01 = tCtrlPoint.front().back();
					Vector3 t10 = tCtrlPoint.back().front();
					Vector3 tU = t10 - t00;
					Vector3 tV = t01 - t00;

					// Normals
					vec1 = cross(sU, sV);
					vec2 = cross(tU, tV);
				}

				vec1.normalize();
				vec2.normalize();
				M[i][j] = 1.0 - abs(dot(vec1, vec2));
			}
		}
	}

	normalizeMatrix(M);
}

void GraphCorresponder::insertCorrespondence( PART_LANDMARK vector2vector )
{
	// Check for existing ones
	for(int i = 0; i < (int)correspondences.size(); i++)
	{
		if(correspondences[i] == vector2vector)
		{
			qDebug() << "Problem!";
		}
	}

	correspondences.push_back(vector2vector);
}

// Final disM
void GraphCorresponder::prepareAllMatrices()
{
	// Validation matrix
	computeValidationMatrix();

	// Distance matrices
	computeHausdorffDistanceMatrix(spatialM);
	//computeSizeDiffMatrix(sizeM);
	//computeOrientationDiffMatrix(orientationM);
	computeLandmarkFeatureMatrix(structuralM);
}

void GraphCorresponder::computeFinalDistanceMatrix()
{
	initializeMatrix<float>(disM, INVALID_VALUE);

	int sN = sg->nodes.size();
	int tN = tg->nodes.size();
	for(int i = 0; i < sN; i++) {
		for (int j = 0; j < tN; j++)
		{
			if (validM[i][j])
			{
				disM[i][j] = spactialW * spatialM[i][j] + structuralW * structuralM[i][j];
			}
		}
	}

	normalizeMatrix(disM);
}


// Part to Part
void GraphCorresponder::computePartToPartCorrespondences()
{
	// Clear
	correspondences.clear();
	corrScores.clear();

	// The final disM
	std::vector< std::vector<float> > disMatrix = disM;

	// Parameters
	int sN = sg->nodes.size();
	int tN = tg->nodes.size();
	float tolerance = 0.04f;

    // Force un-corresponded  已经确定不会被匹配的node
	foreach(int ri, nonCorresS)
		for (int ci = 0; ci < tN; ci++)
			disMatrix[ri][ci] = INVALID_VALUE;
	
	for (int ri = 0; ri < sN; ri++)
		foreach(int ci, nonCorresT)
			disMatrix[ri][ci] = INVALID_VALUE;

	int r, c;
	float minValue;
    while (minElementInMatrix(disMatrix, r, c, minValue)) //每次把距离最小的两个Node匹配起来
	{
		if (minValue > scoreThreshold) break;

		// source:r <-> target:c
		float upperBound = disMatrix[r][c] + tolerance;

		// Search for "many" in source
		std::vector<int> r_many;
		std::vector<float> r_scores;
		for (int ri = 0; ri < sN; ri++)
		{
			if (ri == r || disMatrix[ri][c] == INVALID_VALUE)
				continue;

            // ri is close to c 如果有多个的距离有小于一个阈值，就把它们都放到r_many
			if (disMatrix[ri][c] <= upperBound) 
			{
				r_scores.push_back(disMatrix[ri][c]);
				r_many.push_back(ri);
			}
		}

		// Search for "many" in target
		std::vector<int> c_many;
		std::vector<float> c_scores;
		for (int ci = 0; ci < tN; ci++)
		{
			if (ci == c || disMatrix[r][ci] == INVALID_VALUE)	
				continue;

			// ci is close to r
			if (disMatrix[r][ci] < upperBound) 
			{
				c_scores.push_back(disMatrix[r][ci]);
				c_many.push_back(ci);
			}
		}

		// Results
		QVector<QString> sVector, tVector;
		sVector.push_back(sg->nodes[r]->id);
		tVector.push_back(tg->nodes[c]->id);
		std::vector<float> scores;
		scores.push_back(minValue);
        //确定到底是一对多还是多对一
		if (c_many.size() > r_many.size()) // r <-> {c_i}
		{
			foreach(int ci, c_many)
			{
				tVector.push_back(tg->nodes[ci]->id);

				for (int i = 0; i < sN; i++) // Column c
					disMatrix[i][ci] = INVALID_VALUE;
			}

			scores.insert(scores.end(), c_scores.begin(), c_scores.end());
		}
		else // {r_i} <-> c
		{
			foreach(int ri, r_many)
			{
				sVector.push_back(sg->nodes[ri]->id);

				for (int j = 0; j < tN; j++) // Row r
					disMatrix[ri][j] = INVALID_VALUE;
			}

			scores.insert(scores.end(), r_scores.begin(), r_scores.end());
		}

		// Save results
		PART_LANDMARK vector2vector = std::make_pair(sVector, tVector);
		this->insertCorrespondence( vector2vector );
		this->corrScores[vector2vector] = scores;
		
        // Remove r and c in the disMatrix
        // 匹配过的Node不再考虑
		for (int i = 0; i < sN; i++) // Column c
			disMatrix[i][c] = INVALID_VALUE;
		for (int j = 0; j < tN; j++) // Row r
			disMatrix[r][j] = INVALID_VALUE;
	}

	// Add the part landmarks as correspondences too
	foreach(PART_LANDMARK landmark, landmarks)
	{
		insertCorrespondence( landmark );
		int n = qMax(landmark.first.size(),landmark.second.size());
		std::vector<float> fake_score(n, -1);
		corrScores[landmark] = fake_score;
	}
}


// Point to Point
void GraphCorresponder::correspondAllNodes()
{
	// Adjust the frames for corresponded parts
	// Then Point-to-Point correspondences can be easily retrieved via parameterized NURBS. 
	for (int i = 0; i < (int)correspondences.size(); i++)
	{
		QVector<QString> &sVector = correspondences[i].first;
		QVector<QString> &tVector = correspondences[i].second;

		// One to many
		if (sVector.size() == 1)
		{
			Structure::Node *sNode = sg->getNode(sVector.front());
			foreach(QString tID, tVector)
			{
				Structure::Node *tNode = tg->getNode(tID);
				correspondTwoNodes(sNode, tNode);
			}
		}
		// Many to one
		else if (tVector.size() == 1)
		{
			Structure::Node *tNode = tg->getNode(tVector.front());
			foreach(QString sID, sVector)
			{
				Structure::Node *sNode = sg->getNode(sID);
				correspondTwoNodes(sNode, tNode);
			}
		}
		// Many to many
		else
		{
			qDebug() << "Many to Many correspondence happened. ";
		}
	}
}

void GraphCorresponder::correspondTwoNodes( Structure::Node *sNode, Structure::Node *tNode )
{
	if (sNode->type() != tNode->type())
	{
		qDebug() << "Two nodes with different types cannot be corresponded. ";
		return;
	}

	if (sNode->type() == Structure::SHEET)
		correspondTwoSheets((Structure::Sheet*) sNode, (Structure::Sheet*) tNode, tg);
	else
		correspondTwoCurves((Structure::Curve*) sNode, (Structure::Curve*) tNode, tg);
}

void GraphCorresponder::correspondTwoCurves( Structure::Curve *sCurve, Structure::Curve *tCurve, Structure::Graph * tgt )
{
	std::vector<Vector3> sCtrlPoint = sCurve->controlPoints();
	std::vector<Vector3> tCtrlPoint = tCurve->controlPoints();

	// Euclidean for now, can use Geodesic distance instead if need
	Vector3 scenter = sCurve->center();
	Vector3 sfront = sCtrlPoint.front() - scenter;
	Vector3 tcenter = tCurve->center();
	Vector3 tfront = tCtrlPoint.front() - tcenter;
	Vector3 tback = tCtrlPoint.back() - tcenter;

	float f2f = (sfront - tfront).norm();
	float f2b = (sfront - tback).norm();

	float diff = std::abs(f2f-f2b);
    float threshold = 0.1f;

	if (f2f > f2b && diff > threshold)
	{
		// Flip the target
		std::vector<Scalar> tCtrlWeight = tCurve->controlWeights();
		std::reverse(tCtrlPoint.begin(), tCtrlPoint.end());
		std::reverse(tCtrlWeight.begin(), tCtrlWeight.end());

        NURBS::NURBSCurved newCurve(tCtrlPoint, tCtrlWeight);
		tCurve->curve = newCurve;

		// Update the coordinates of links
		foreach( Structure::Link * l, tgt->getEdges(tCurve->id) )
		{
			l->setCoord(tCurve->id, inverseCoords( l->getCoord(tCurve->id) ));
		}
	}
}

void GraphCorresponder::correspondTwoSheets( Structure::Sheet *sSheet, Structure::Sheet *tSheet, Structure::Graph * tgt)
{
	// Old properties
    NURBS::NURBSRectangled &oldRect = tSheet->surface;
	int uDegree = oldRect.GetDegree(0);
	int vDegree = oldRect.GetDegree(1);
	bool uLoop = oldRect.IsLoop(0);
	bool vLoop = oldRect.IsLoop(1);
	bool uOpen = oldRect.IsOpen(0);
	bool vOpen = oldRect.IsOpen(1);
	bool isModified = false;
	bool isUVFlipped = false;

	// Control points and weights
	Array2D_Vector3 sCtrlPoint = sSheet->surface.mCtrlPoint;
	Array2D_Real sCtrlWeight = sSheet->surface.mCtrlWeight;

	Array2D_Vector3 tCtrlPoint = tSheet->surface.mCtrlPoint;
	Array2D_Real tCtrlWeight = tSheet->surface.mCtrlWeight;

	Array2D_Vector3 tCtrlPointNew;
	Array2D_Real tCtrlWeightNew;

	Vector3 scenter = sSheet->center();
	Vector3 tcenter = tSheet->center();

	// Get the extreme points.
	Vector3 s00 = sCtrlPoint.front().front();
	Vector3 s01 = sCtrlPoint.front().back();
	Vector3 s10 = sCtrlPoint.back().front();
	Vector3 sU = s10 - s00;
	Vector3 sV = s01 - s00;

	Vector3 t00 = tCtrlPoint.front().front();
	Vector3 t01 = tCtrlPoint.front().back();
	Vector3 t10 = tCtrlPoint.back().front();
	Vector3 tU = t10 - t00;
	Vector3 tV = t01 - t00;

	// Flip if need
	Vector3 sUV = cross(sU, sV);
	Vector3 tUV = cross(tU, tV);
	if (dot(sUV, tUV) < 0)
	{
		// Reverse the target along u direction
		std::reverse(tCtrlPoint.begin(), tCtrlPoint.end());
		std::reverse(tCtrlWeight.begin(), tCtrlWeight.end());

		// Update tU
		tU = -tU;
		tUV = -tUV;
		isModified = true;

		// Update the coordinates of links
		foreach( Structure::Link * l, tgt->getEdges(tSheet->id) ){
			Array1D_Vector4d oldCoord = l->getCoord(tSheet->id), newCoord;
			foreach(Vector4d c, oldCoord) newCoord.push_back(Vector4d(1-c[0], c[1], c[2], c[3]));
			l->setCoord(tSheet->id, newCoord);
		}
	}

	// Rotate if need
	Scalar cosAngle = dot(sU.normalized(), tU.normalized());
	Scalar cos45 = sqrtf(2.0) / 2;

	// Do Nothing
	if (cosAngle > cos45)
	{
		tCtrlPointNew = tCtrlPoint;
		tCtrlWeightNew = tCtrlWeight;
	}
	// Rotate 180 degrees
	else if (cosAngle < - cos45)
	{
		//  --> sV				tU
		// |					|
		// sU             tV <--
		// By flipping along both directions
		std::reverse(tCtrlPoint.begin(), tCtrlPoint.end());
		std::reverse(tCtrlWeight.begin(), tCtrlWeight.end());

		for (int i = 0; i < (int)tCtrlPoint.size(); i++)
		{
			std::reverse(tCtrlPoint[i].begin(), tCtrlPoint[i].end());
			std::reverse(tCtrlWeight[i].begin(), tCtrlWeight[i].end());
		}

		// The new control points and weights
		tCtrlPointNew = tCtrlPoint;
		tCtrlWeightNew = tCtrlWeight;
		isModified = true;

		// Update the coordinates of links
		foreach( Structure::Link * l, tgt->getEdges(tSheet->id) ){
			Array1D_Vector4d oldCoord = l->getCoord(tSheet->id), newCoord;
			foreach(Vector4d c, oldCoord) newCoord.push_back(Vector4d(1-c[0], 1-c[1], c[2], c[3]));
			l->setCoord(tSheet->id, newCoord);
		}
	}
	// Rotate 90 degrees 
	else
	{
		Vector3 stU = cross(sU, tU);
		if (dot(stU, sUV) >= 0)
		{
			//  --> sV		tV
			// |			|
			// sU           --> tU
			// Transpose and reverse along U
			tCtrlPointNew = transpose<Vector3>(tCtrlPoint);
			tCtrlWeightNew = transpose<Scalar>(tCtrlWeight);

			std::reverse(tCtrlPointNew.begin(), tCtrlPointNew.end());
			std::reverse(tCtrlWeightNew.begin(), tCtrlWeightNew.end());

			// Update the coordinates of links
			foreach( Structure::Link * l, tgt->getEdges(tSheet->id) ){
				Array1D_Vector4d oldCoord = l->getCoord(tSheet->id), newCoord;
				foreach(Vector4d c, oldCoord) newCoord.push_back(Vector4d(1- c[1], c[0], c[2], c[3]));
				l->setCoord(tSheet->id, newCoord);
			}
		}
		else
		{
			//  --> sV	tU<--
			// |			 |
			// sU			tV
			// Reverse along U and Transpose
			std::reverse(tCtrlPoint.begin(), tCtrlPoint.end());
			std::reverse(tCtrlWeight.begin(), tCtrlWeight.end());

			tCtrlPointNew = transpose<Vector3>(tCtrlPoint);
			tCtrlWeightNew = transpose<Scalar>(tCtrlWeight);

			// Update the coordinates of links
			foreach( Structure::Link * l, tgt->getEdges(tSheet->id) ){
				Array1D_Vector4d oldCoord = l->getCoord(tSheet->id), newCoord;
				foreach(Vector4d c, oldCoord) newCoord.push_back(Vector4d(c[1], 1- c[0], c[2], c[3]));
				l->setCoord(tSheet->id, newCoord);
			}
		}

		isModified = true;
		isUVFlipped = true;
	}

	// Create a new sheet if need
	if (isModified)
	{
        NURBS::NURBSRectangled newRect;
		if (isUVFlipped)
            newRect = NURBS::NURBSRectangled(tCtrlPointNew, tCtrlWeightNew, vDegree, uDegree, vLoop, uLoop, vOpen, uOpen);
		else
            newRect = NURBS::NURBSRectangled(tCtrlPointNew, tCtrlWeightNew, uDegree, vDegree, uLoop, vLoop, uOpen, vOpen);

		tSheet->surface = newRect;
	}
}


void GraphCorresponder::doHopelessCorrespondence()
{
	double maxVolS = -1, maxVolT = -1;
	QString maxS, maxT;

	// Get parts with maximum volume on both graphs
	foreach(Structure::Node* n, sg->nodes){
		double volume = n->bbox().volume();
		if(volume > maxVolS){
			maxVolS = volume;
			maxS = n->id;
		}
	}
	foreach(Structure::Node* n, tg->nodes){
		double volume = n->bbox().volume();
		if(volume > maxVolT){
			maxVolT = volume;
			maxT = n->id;
		}
	}

	// Force a correspondence
	this->addCorrespondences(QVector<QString>() << maxS, QVector<QString>() << maxT, -1);

	qDebug() << "WARNING: forced correspondence between [" << maxS << "] and [" << maxT << "].";
}

// Others
std::vector<QString> GraphCorresponder::nonCorresSource()
{
	std::vector<QString> nodes;
    int l;
    l = sg->nodes.size();
    for (int i = 0; i < l/*(int)sIsCorresponded.size()*/; i++)
	{
		if (!sIsCorresponded[i])
			nodes.push_back(sg->nodes[i]->id);
	}

	return nodes;
}

std::vector<QString> GraphCorresponder::nonCorresTarget()
{
	std::vector<QString> nodes;

	for (int i = 0; i < (int) tIsCorresponded.size(); i++)
	{
		if (!tIsCorresponded[i])
			nodes.push_back(tg->nodes[i]->id);
	}

	return nodes;
}

void GraphCorresponder::clearFor( QString sID, bool isSource )
{
	// Only keep landmarks that don't have sID
	QVector<PART_LANDMARK> lkeep, lremove;
	for(int i = 0; i < landmarks.size(); i++)
	{
		bool isKeep = (isSource && !landmarks[i].first.contains(sID)) || (!isSource && !landmarks[i].second.contains(sID));
		if( isKeep ) 
			lkeep.push_back(landmarks[i]);
		else
			lremove.push_back(landmarks[i]);
	}

	landmarks = lkeep;

	foreach(PART_LANDMARK landmark, lremove)
	{
		foreach(QString sid, landmark.first){
			sIsLandmark[sg->getNode(sid)->property["index"].toInt()] = false;
		}

		foreach(QString tid, landmark.second){
			tIsLandmark[tg->getNode(tid)->property["index"].toInt()] = false;
		}
	}
}

void GraphCorresponder::setNonCorresSource(QString sID)
{
	clearFor(sID, true);

	nonCorresS.insert( sg->getNode(sID)->property["index"].toInt() );
}

void GraphCorresponder::setNonCorresTarget(QString tID)
{
	clearFor(tID, false);

	nonCorresT.insert( tg->getNode(tID)->property["index"].toInt() );
}

void GraphCorresponder::saveCorrespondences( QString filename, bool isWithScores )
{
	QFile file(filename);
	if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) return;
	QTextStream outF(&file);

	// Header
	outF << (isWithScores ? "hasScore" : "" ) << " " << correspondences.size() << " " << nonCorresS.size() << " " << nonCorresT.size() << "\n\n";

	// Add items
	int nCoor = correspondences.size();
	for (int i = 0; i < nCoor; i++)
	{
		// Correspondences and scores
		PART_LANDMARK vec2vec = correspondences[i];
		std::vector<float> scores = corrScores[vec2vec];

		outF << vec2vec.first.size() << '\t';;
		foreach (QString strID, vec2vec.first)
			outF << strID << '\t';
		outF << '\n';

		outF << vec2vec.second.size() << '\t';
		foreach (QString strID, vec2vec.second)
			outF << strID << '\t';

		if(isWithScores) outF << "\n" << scores.front() << "\n";

		outF << "\n\n";
	}

	// Non-corresponded
	{
		foreach(int i, nonCorresS) outF << i << '\t';
		outF << "\n";

		foreach(int i, nonCorresT) outF << i << '\t';
		outF << "\n";
	}

	file.close();
}

void GraphCorresponder::loadCorrespondences( QString filename, bool isReversed )
{
	QFile file(filename);
	if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) return;
	QTextStream inF(&file);

	clear();

	int nbCorr;
	//inF >> nbCorr; // old way
	QString header = inF.readLine();
	bool isWithScores = header.contains("Score", Qt::CaseInsensitive);
	
	QVector<int> counts;
	QRegExp re("\\d*");
	foreach(QString token, header.split(" ", QString::SkipEmptyParts)){
		if(re.exactMatch(token))
			counts.push_back(token.toInt());
	}

	nbCorr = counts.front();

	for (int i = 0; i < nbCorr; i++)
	{
		int n;
		QString strID;
		QVector<QString> sParts, tParts;

		inF >> n;
		for (int j = 0; j < n; j++)
		{
			inF >> strID;
			sParts.push_back(strID);
		}

		inF >> n;
		for (int j = 0; j < n; j++)
		{
			inF >> strID;
			tParts.push_back(strID);
		}

		float score = -1;
		if(isWithScores) inF >> score;

		// This is useful when file is saved as g1_g2 is loaded for g2_g1
		if( isReversed ) qSwap(sParts, tParts);

		// Store correspondence
		addCorrespondences(sParts, tParts, score);
	}

	if(counts.size() == 3)
	{
		for(int i = 0; i < counts[1]; i++){
			int nid = -1;
			inF >> nid;
			nonCorresS.insert(nid);
		}

		for(int i = 0; i < counts[2]; i++){
			int nid = -1;
			inF >> nid;
			nonCorresT.insert(nid);
		}

		if( isReversed ) qSwap(nonCorresS, nonCorresT);
	}

	isReady = true;  // block automatic computing correspondences

	file.close();
}

void GraphCorresponder::visualizePart2PartDistance( int sourceID )
{
	if (disM.empty()) 
		computeFinalDistanceMatrix();

	// The source
	sourceID %= sg->nodes.size();
	for (int i = 0; i < sg->nodes.size(); i++)
	{
		Structure::Node *sNode = sg->nodes[i];
		float color = (i == sourceID)? 1.0 : 0.0;
		sNode->vis_property["color"] = qtJetColorMap(color);
		sNode->vis_property["showControl"] = false;
	}

	// Visualize by jet color
	for (int j = 0; j < tg->nodes.size(); j++)
	{
		Structure::Node *tNode = tg->nodes[j];

		float value;
		if (disM[sourceID][j] == INVALID_VALUE)
			value = 0;
		else
			value = 1 - disM[sourceID][j];
		tNode->vis_property["color"] = qtJetColorMap(value);
		tNode->vis_property["showControl"] = false;
	}
}



// Main access
void GraphCorresponder::computeCorrespondences()
{
    corrScores.clear();

    // Prepare
    prepareAllMatrices();

    // Distance matrix
    computeFinalDistanceMatrix();

    // Part to Part correspondence
    computePartToPartCorrespondences();

    // Point to Point correspondence
    correspondAllNodes();

    // Reset correspondence states
    sIsCorresponded.clear();
    tIsCorresponded.clear();
    sIsCorresponded.resize(sg->nodes.size(), false);
    tIsCorresponded.resize(tg->nodes.size(), false);

    if(correspondences.empty()) doHopelessCorrespondence();

    // Mark the corresponded nodes
    foreach (PART_LANDMARK vector2vector, correspondences)
    {
        foreach (QString sID, vector2vector.first)
        {
            int sid = sg->getNode(sID)->property["index"].toInt();
            sIsCorresponded[sid] = true;
        }

        foreach (QString tID, vector2vector.second)
        {
            int tid = tg->getNode(tID)->property["index"].toInt();
            tIsCorresponded[tid] = true;
        }
    }
}

//自己随便改一改匹配算法……
void GraphCorresponder::computeCorrespondencesNew(bool fromTarget){
    corrScores.clear();

    // Prepare
    prepareAllMatrices();

    // Distance matrix //直接计算control points之间的距离，所以要求模型全部归一化
    computeFinalDistanceMatrix();

    // Part to Part correspondence
    if(fromTarget){
        computePartToPartCorrTarget();
    }else{
        computePartToPartCorrNew();
    }
    computePartToPartCorrOther();

    // Point to Point correspondence
    correspondAllNodes();

    // Reset correspondence states
    sIsCorresponded.clear();
    tIsCorresponded.clear();
    sIsCorresponded.resize(sg->nodes.size(), false);
    tIsCorresponded.resize(tg->nodes.size(), false);

    if(correspondences.empty()) doHopelessCorrespondence();

    // Mark the corresponded nodes
    foreach (PART_LANDMARK vector2vector, correspondences)
    {
        foreach (QString sID, vector2vector.first)
        {
            int sid = sg->getNode(sID)->property["index"].toInt();
            sIsCorresponded[sid] = true;
        }

        foreach (QString tID, vector2vector.second)
        {
            int tid = tg->getNode(tID)->property["index"].toInt();
            tIsCorresponded[tid] = true;
        }
    }
}
//在第二次匹配中应该不再改变target,也不应该出现1个target对应多个source的情况
//第二次应该每一个source都给匹配至少一个node
void GraphCorresponder::computePartToPartCorrTarget(){
    // Clear
    correspondences.clear();
    corrScores.clear();

    // The final disM
    std::vector< std::vector<float> > disMatrix = disM;

    // Parameters
    int sN = sg->nodes.size();
    int tN = tg->nodes.size();
    float tolerance = 0.04f;

    // Force un-corresponded  已经确定不会被匹配的node
    foreach(int ri, nonCorresS)
        for (int ci = 0; ci < tN; ci++)
            disMatrix[ri][ci] = INVALID_VALUE;

    for (int ri = 0; ri < sN; ri++)
        foreach(int ci, nonCorresT)
            disMatrix[ri][ci] = INVALID_VALUE;

    //第一次先给每个source中的node都分配一个corre
    //每个node都应该有至少一个匹配，每个node匹配的内容应该不重叠
    //优先级：1. 距离最近且没有被匹配的node
    //       2. 距离不是最近但是没有被匹配的node
    //       3. 其他距离最近的node
    QMap<int, QVector<int>> mp;
    std::vector< std::vector<float> > tmpMatrix(sN, std::vector<float>(tN));
    for(int i = 0; i < sN; i++){
        tmpMatrix[i].assign(disMatrix[i].begin(), disMatrix[i].end());
    }
    int r, c;
    float minValue;
    while(minElementInMatrix(tmpMatrix, r, c, minValue)){
        // Remove r and c in the disMatrix
        // 匹配过的Node不再考虑
        for (int i = 0; i < sN; i++) // Column c
            tmpMatrix[i][c] = INVALID_VALUE;
        for (int j = 0; j < tN; j++) // Row r
            tmpMatrix[r][j] = INVALID_VALUE;
        for (int j = 0; j < tN; j++) // Row r
            disMatrix[r][j] = INVALID_VALUE;

        // Save results
        QVector<int> singleT;
        singleT.push_back(c);
        mp[r] = singleT;
    }

    while (minElementInMatrix(disMatrix, r, c, minValue)) //每次把距离最小的两个Node匹配起来
    {
        if (minValue > scoreThreshold) break;

        mp[r].push_back(c);
        std::vector<float> scores;
        scores.push_back(minValue);

        // source:r <-> target:c
        float upperBound = disMatrix[r][c] + tolerance;

        // Search for "many" in source
        for (int ci = 0; ci < tN; ci++)
        {
            if (ci == c || disMatrix[r][ci] == INVALID_VALUE)
                continue;

            // ri is close to c 如果有多个的距离有小于一个阈值，就把它们都放到r_many
            if (disMatrix[r][ci] <= upperBound)
            {
                QString cID = tg->nodes[c]->id;
                foreach(QVector<QString> group, tg->groupsOf(cID)){
                    if(group.contains(tg->nodes[ci]->id)){
                        mp[r].push_back(c);

                        // Remove r and c in the disMatrix
                        // 匹配过的Node不再考虑, 一个source node可以和多个target匹配
                        for (int i = 0; i < sN; i++)
                            disMatrix[i][ci] = INVALID_VALUE;
                    }
                }

            }
        }

        // Remove r and c in the disMatrix
        // 匹配过的Node不再考虑
        for (int i = 0; i < sN; i++) // Column c
            disMatrix[i][c] = INVALID_VALUE;
        for (int j = 0; j < tN; j++) // Row r
            disMatrix[r][j] = INVALID_VALUE;

    }
    // 应该每个source 的node都匹配到了才对
    for(int i = 0; i < sN; i++){
        bool corred = true;
        for(int j = 0; j < tN; j++){
            if(disMatrix[i][j] != INVALID_VALUE){
                assert(corred);
            }
        }
    }
    QMap<int, QVector<int>>::iterator iter = mp.begin();
    while(iter!=mp.end()){
        QVector<QString> sVector, tVector;
        std::vector<float> scores;
        sVector.push_back(sg->nodes[iter.key()]->id);
        foreach(int i, iter.value()){
            tVector.push_back(tg->nodes[i]->id);
            scores.push_back(disMatrix[iter.key()][i]);
        }

        // Save results
        PART_LANDMARK vector2vector = std::make_pair(sVector, tVector);
        this->insertCorrespondence( vector2vector );
        this->corrScores[vector2vector] = scores;
        iter++;
    }

    // Add the part landmarks as correspondences too
    foreach(PART_LANDMARK landmark, landmarks)
    {
        insertCorrespondence( landmark );
        int n = qMax(landmark.first.size(),landmark.second.size());
        std::vector<float> fake_score(n, -1);
        corrScores[landmark] = fake_score;
    }

}

void GraphCorresponder::computePartToPartCorrNew(){
    // Clear
    correspondences.clear();
    corrScores.clear();

    // The final disM
    std::vector< std::vector<float> > disMatrix = disM;

    // Parameters
    int sN = sg->nodes.size();
    int tN = tg->nodes.size();
    float tolerance = 0.04f;

    // Force un-corresponded  已经确定不会被匹配的node
    foreach(int ri, nonCorresS)
        for (int ci = 0; ci < tN; ci++)
            disMatrix[ri][ci] = INVALID_VALUE;

    for (int ri = 0; ri < sN; ri++)
        foreach(int ci, nonCorresT)
            disMatrix[ri][ci] = INVALID_VALUE;

    int r, c;
    float minValue;
    while (minElementInMatrix(disMatrix, r, c, minValue)) //每次把距离最小的两个Node匹配起来
    {
        if (minValue > scoreThreshold) break;

        // source:r <-> target:c
        float upperBound = disMatrix[r][c] + tolerance;

        // Search for "many" in source
        std::vector<int> r_many;
        std::vector<float> r_scores;
        for (int ri = 0; ri < sN; ri++)
        {
            if (ri == r || disMatrix[ri][c] == INVALID_VALUE)
                continue;

            // ri is close to c 如果有多个的距离有小于一个阈值，就把它们都放到r_many
            if (disMatrix[ri][c] <= upperBound)
            {
                QString rID = sg->nodes[r]->id;
                foreach(QVector<QString> group, sg->groupsOf(rID)){
                    if(group.contains(sg->nodes[ri]->id)){
                        r_scores.push_back(disMatrix[ri][c]);
                        r_many.push_back(ri);
                    }
                }

            }
        }

        // Search for "many" in target
        std::vector<int> c_many;
        std::vector<float> c_scores;
        for (int ci = 0; ci < tN; ci++)
        {
            if (ci == c || disMatrix[r][ci] == INVALID_VALUE)
                continue;

            // ci is close to r
            if (disMatrix[r][ci] < upperBound)
            {
                QString cID = tg->nodes[c]->id;
                foreach(QVector<QString> group, tg->groupsOf(cID)){
                    if(group.contains(tg->nodes[ci]->id)){
                        c_scores.push_back(disMatrix[r][ci]);
                        c_many.push_back(ci);

                    }
                }


            }
        }

        // Results
        QVector<QString> sVector, tVector;
        sVector.push_back(sg->nodes[r]->id);
        tVector.push_back(tg->nodes[c]->id);
        std::vector<float> scores;
        scores.push_back(minValue);

        //尽量保持一对一，然后才是一对多和多对一
        //和r和c匹配的数据离，如果有能相互匹配的，就把他们也踢出去
        if(c_many.size() > 1 && r_many.size() > 1){
            for(int i = 1; i < r_many.size(); i++){
                for(int j = 1; j < c_many.size(); j++){
                    int ri = r_many[i], cj = c_many[j];
                    if(ri > -1 && cj > -1 && disMatrix[ri][cj] < upperBound){
                        r_many[i] = -1;
                        c_many[j] = -1;
                        break;
                    }
                }
            }
        }
        std::vector<int>r_tmp, c_tmp;
        std::vector<float> rscore_tmp, cscore_tmp;
        for(int i = 0; i < r_many.size(); i++){
            int r = r_many[i];
            if(r > -1)r_tmp.push_back(r);
            rscore_tmp.push_back(r_scores[i]);
        }
        for(int i; i < c_many.size(); i++){
            int c = c_many[i];
            if(c > -1)c_tmp.push_back(c);
            cscore_tmp.push_back(c_scores[i]);
        }
        r_many.swap(r_tmp);
        c_many.swap(c_tmp);
        r_scores.swap(rscore_tmp);
        c_scores.swap(cscore_tmp);

        //确定到底是一对多还是多对一
        if (c_many.size() > r_many.size()) // r <-> {c_i}
        {
            foreach(int ci, c_many)
            {
                tVector.push_back(tg->nodes[ci]->id);

                for (int i = 0; i < sN; i++) // Column c
                    disMatrix[i][ci] = INVALID_VALUE;
            }

            scores.insert(scores.end(), c_scores.begin(), c_scores.end());
        }
        else // {r_i} <-> c
        {
            foreach(int ri, r_many)
            {
                sVector.push_back(sg->nodes[ri]->id);

                for (int j = 0; j < tN; j++) // Row r
                    disMatrix[ri][j] = INVALID_VALUE;
            }

            scores.insert(scores.end(), r_scores.begin(), r_scores.end());
        }

        // Save results
        PART_LANDMARK vector2vector = std::make_pair(sVector, tVector);
        this->insertCorrespondence( vector2vector );
        this->corrScores[vector2vector] = scores;

        // Remove r and c in the disMatrix
        // 匹配过的Node不再考虑
        for (int i = 0; i < sN; i++) // Column c
            disMatrix[i][c] = INVALID_VALUE;
        for (int j = 0; j < tN; j++) // Row r
            disMatrix[r][j] = INVALID_VALUE;
    }

    // Add the part landmarks as correspondences too
    foreach(PART_LANDMARK landmark, landmarks)
    {
        insertCorrespondence( landmark );
        int n = qMax(landmark.first.size(),landmark.second.size());
        std::vector<float> fake_score(n, -1);
        corrScores[landmark] = fake_score;
    }

}

void GraphCorresponder::computePartToPartCorrOther(){
    QVector<Structure::Node*> sNullNodes, tNullNodes;
    foreach(Structure::Node* snode, sg->nodes){
        if(snode->property.contains("correspond")) continue;
        sNullNodes.push_back(snode);
    }
    foreach(Structure::Node* tnode, tg->nodes){
        if(tnode->property.contains("correspond")) continue;
        tNullNodes.push_back(tnode);
    }

    foreach(Structure::Node* snode, sNullNodes){
        foreach(Structure::Node* tnode, tNullNodes){
            if(tnode->property.contains("correspond"))continue;

            //把所有的邻居node都放到set里面
            QVector<Structure::Link*> sedges = sg->getEdges(snode->id);
            QSet<QString> sNeighborNodes;

            foreach(Structure::Link* edge, sedges){
                Structure::Node* sother = edge->otherNode(snode->id);
                sNeighborNodes.insert(sother->id);
            }

            QVector<Structure::Link*> tedges = tg->getEdges(tnode->id);
            QSet<QString> tNeighborNodes;

            foreach(Structure::Link* edge, tedges){
                Structure::Node* tother = edge->otherNode(tnode->id);
                tNeighborNodes.insert(tother->id);
            }

            bool isCorr = true;
            //看两边的邻居是否一模一样
            foreach(QString sotherID, sNeighborNodes){
                Structure::Node* sother = sg->getNode(sotherID);
                if(sother->property.contains("correspond")){
                    QString totherID = sother->property["correspond"].toString();

                    if(!tNeighborNodes.contains(totherID)){
                        isCorr = false;
                        break;
                    }
                }else{
                    isCorr = false;
                    break;
                }
            }

            if(isCorr){
                // Save results
                QVector<QString> sVector, tVector;
                sVector.push_back(snode->id);
                tVector.push_back(tnode->id);
                PART_LANDMARK vector2vector = std::make_pair(sVector, tVector);
                this->insertCorrespondence( vector2vector );
                //this->corrScores[vector2vector] =
            }
        }
    }
}
