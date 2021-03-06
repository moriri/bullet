/*
Bullet Continuous Collision Detection and Physics Library
Copyright (c) 2003-2006 Erwin Coumans  http://continuousphysics.com/Bullet/

This software is provided 'as-is', without any express or implied warranty.
In no event will the authors be held liable for any damages arising from the use of this software.
Permission is granted to anyone to use this software for any purpose, 
including commercial applications, and to alter it and redistribute it freely, 
subject to the following restrictions:

1. The origin of this software must not be misrepresented; you must not claim that you wrote the original software. If you use this software in a product, an acknowledgment in the product documentation would be appreciated but is not required.
2. Altered source versions must be plainly marked as such, and must not be misrepresented as being the original software.
3. This notice may not be removed or altered from any source distribution.
*/

#ifndef CONVEX_CONVEX_ALGORITHM_H
#define CONVEX_CONVEX_ALGORITHM_H

#include "BroadphaseCollision/CollisionAlgorithm.h"
#include "NarrowPhaseCollision/GjkPairDetector.h"
#include "NarrowPhaseCollision/PersistentManifold.h"
#include "BroadphaseCollision/BroadphaseProxy.h"
#include "NarrowPhaseCollision/VoronoiSimplexSolver.h"
#include "CollisionCreateFunc.h"

class ConvexPenetrationDepthSolver;

///ConvexConvexAlgorithm collision algorithm implements time of impact, convex closest points and penetration depth calculations.
class ConvexConvexAlgorithm : public CollisionAlgorithm
{
	//ConvexPenetrationDepthSolver*	m_penetrationDepthSolver;
	VoronoiSimplexSolver	m_simplexSolver;
	GjkPairDetector m_gjkPairDetector;
	bool	m_useEpa;
public:
	BroadphaseProxy	m_box0;
	BroadphaseProxy	m_box1;

	bool	m_ownManifold;
	PersistentManifold*	m_manifoldPtr;
	bool			m_lowLevelOfDetail;

	void	CheckPenetrationDepthSolver();

	

public:

	ConvexConvexAlgorithm(PersistentManifold* mf,const CollisionAlgorithmConstructionInfo& ci,BroadphaseProxy* proxy0,BroadphaseProxy* proxy1);

	virtual ~ConvexConvexAlgorithm();

	virtual void ProcessCollision (BroadphaseProxy* proxy0,BroadphaseProxy* proxy1,const DispatcherInfo& dispatchInfo);

	virtual float CalculateTimeOfImpact(BroadphaseProxy* proxy0,BroadphaseProxy* proxy1,const DispatcherInfo& dispatchInfo);

	void	SetLowLevelOfDetail(bool useLowLevel);

	virtual void SetShapeIdentifiers(int partId0,int index0,	int partId1,int index1)
	{
			m_gjkPairDetector.m_partId0=partId0;
			m_gjkPairDetector.m_partId1=partId1;
			m_gjkPairDetector.m_index0=index0;
			m_gjkPairDetector.m_index1=index1;		
	}

	const PersistentManifold*	GetManifold()
	{
		return m_manifoldPtr;
	}

	struct CreateFunc :public 	CollisionAlgorithmCreateFunc
	{
		virtual	CollisionAlgorithm* CreateCollisionAlgorithm(CollisionAlgorithmConstructionInfo& ci, BroadphaseProxy* proxy0,BroadphaseProxy* proxy1)
		{
			return new ConvexConvexAlgorithm(0,ci,proxy0,proxy1);
		}
	};


};

#endif //CONVEX_CONVEX_ALGORITHM_H
