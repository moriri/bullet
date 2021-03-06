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

#include "btConvexPlaneCollisionAlgorithm.h"

#include "BulletCollision/CollisionDispatch/btCollisionDispatcher.h"
#include "BulletCollision/CollisionDispatch/btCollisionObject.h"
#include "BulletCollision/CollisionShapes/btConvexShape.h"
#include "BulletCollision/CollisionShapes/btStaticPlaneShape.h"

//#include <stdio.h>

btConvexPlaneCollisionAlgorithm::btConvexPlaneCollisionAlgorithm(const btCollisionAlgorithmConstructionInfo& ci)
: btActivatingCollisionAlgorithm(ci),
m_numPerturbationIterations(1),
m_minimumPointsPerturbationThreshold(1),
m_isSwapped(ci.m_isSwapped)
{
}


btConvexPlaneCollisionAlgorithm::~btConvexPlaneCollisionAlgorithm()
{
}

void btConvexPlaneCollisionAlgorithm::collideSingleContact (const btQuaternion& perturbeRot, const btCollider& body0,const btCollider& body1,const btDispatcherInfo& dispatchInfo,btManifoldResult* resultOut)
{
    const btCollider& convexObj = m_isSwapped? body1 : body0;
	const btCollider& planeObj = m_isSwapped? body0: body1;

	const btConvexShape* convexShape = (const btConvexShape*) convexObj.getCollisionShape();
	const btStaticPlaneShape* planeShape = (const btStaticPlaneShape*) planeObj.getCollisionShape();

    bool hasCollision = false;
	const btVector3& planeNormal = planeShape->getPlaneNormal();
	const btScalar& planeConstant = planeShape->getPlaneConstant();
	
	btTransform convexWorldTransform = convexObj.getWorldTransform();
	btTransform convexInPlaneTrans;
	convexInPlaneTrans= planeObj.getWorldTransform().inverse() * convexWorldTransform;
	//now perturbe the convex-world transform
	convexWorldTransform.getBasis()*=btMatrix3x3(perturbeRot);
	btTransform planeInConvex;
	planeInConvex= convexWorldTransform.inverse() * planeObj.getWorldTransform();
	
	btVector3 vtx = convexShape->localGetSupportingVertex(planeInConvex.getBasis()*-planeNormal);

	btVector3 vtxInPlane = convexInPlaneTrans(vtx);
	btScalar distance = (planeNormal.dot(vtxInPlane) - planeConstant);

	btVector3 vtxInPlaneProjected = vtxInPlane - distance*planeNormal;
	btVector3 vtxInPlaneWorld = planeObj.getWorldTransform() * vtxInPlaneProjected;

	hasCollision = distance < m_manifoldPtr->getContactBreakingThreshold();
	resultOut->setPersistentManifold(m_manifoldPtr);
	if (hasCollision)
	{
		/// report a contact. internally this will be kept persistent, and contact reduction is done
		btVector3 normalOnSurfaceB = planeObj.getWorldTransform().getBasis() * planeNormal;
		btVector3 pOnB = vtxInPlaneWorld;
		resultOut->addContactPoint(normalOnSurfaceB,pOnB,distance);
	}
}


void btConvexPlaneCollisionAlgorithm::processCollision (const btCollisionProcessInfo& processInfo)
{
	if (!m_manifoldPtr)
		return;

    const btCollider& convexObj = m_isSwapped? processInfo.m_body1 : processInfo.m_body0;
	const btCollider& planeObj = m_isSwapped? processInfo.m_body0: processInfo.m_body1;

	const btConvexShape* convexShape = (const btConvexShape*) convexObj.getCollisionShape();
	const btStaticPlaneShape* planeShape = (const btStaticPlaneShape*) planeObj.getCollisionShape();

    
	const btVector3& planeNormal = planeShape->getPlaneNormal();
	//const btScalar& planeConstant = planeShape->getPlaneConstant();

	//first perform a collision query with the non-perturbated collision objects
	{
		btQuaternion rotq(0,0,0,1);
		collideSingleContact(
			rotq,
			processInfo.m_body0,
			processInfo.m_body1,
			processInfo.m_dispatchInfo,
			processInfo.m_result
		);
	}

	if (processInfo.m_result->getPersistentManifold()->getNumContacts()<m_minimumPointsPerturbationThreshold)
	{
		btVector3 v0,v1;
		btPlaneSpace1(planeNormal,v0,v1);
		//now perform 'm_numPerturbationIterations' collision queries with the perturbated collision objects

		const btScalar angleLimit = 0.125f * SIMD_PI;
		btScalar perturbeAngle;
		btScalar radius = convexShape->getAngularMotionDisc();
		perturbeAngle = gContactBreakingThreshold / radius;
		if ( perturbeAngle > angleLimit ) 
				perturbeAngle = angleLimit;

		btQuaternion perturbeRot(v0,perturbeAngle);
		for (int i=0;i<m_numPerturbationIterations;i++)
		{
			btScalar iterationAngle = i*(SIMD_2_PI/btScalar(m_numPerturbationIterations));
			btQuaternion rotq(planeNormal,iterationAngle);
			collideSingleContact(
				rotq.inverse() * perturbeRot * rotq,
				processInfo.m_body0,
				processInfo.m_body1,
				processInfo.m_dispatchInfo,
				processInfo.m_result
			);
		}
	}

	if (m_ownManifold)
	{
		if (m_manifoldPtr->getNumContacts())
		{
			processInfo.m_result->refreshContactPoints();
		}
	}
}

btScalar btConvexPlaneCollisionAlgorithm::calculateTimeOfImpact(btCollisionObject* col0,btCollisionObject* col1,const btDispatcherInfo& dispatchInfo,btManifoldResult* resultOut)
{
	(void)resultOut;
	(void)dispatchInfo;
	(void)col0;
	(void)col1;

	//not yet
	return btScalar(1.);
}
