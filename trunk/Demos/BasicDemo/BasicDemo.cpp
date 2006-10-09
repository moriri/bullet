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

//#define USE_GROUND_BOX 1
#define PRINT_CONTACT_STATISTICS 1
int gNumObjects = 120;
#define HALF_EXTENTS 1.f
#include "btBulletDynamicsCommon.h"
#include "LinearMath/btIDebugDraw.h"
#include "GLDebugDrawer.h"
#include "BMF_Api.h"
#include <stdio.h> //printf debugging
float deltaTime = 1.f/60.f;
float	gCollisionMargin = 0.05f;
#include "BasicDemo.h"
#include "GL_ShapeDrawer.h"
#include "GlutStuff.h"
////////////////////////////////////

GLDebugDrawer debugDrawer;

int main(int argc,char** argv)
{

	BasicDemo* ccdDemo = new BasicDemo();
	ccdDemo->initPhysics();
	ccdDemo->setCameraDistance(50.f);
	return glutmain(argc, argv,640,480,"Bullet Physics Demo. http://bullet.sf.net",ccdDemo);
}



extern int gNumManifold;
extern int gOverlappingPairs;
extern int gTotalContactPoints;

void BasicDemo::clientMoveAndDisplay()
{
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT); 

	if (m_dynamicsWorld)
		m_dynamicsWorld->stepSimulation(deltaTime);
		
	renderme(); 

	glFlush();
	//some additional debugging info
#ifdef PRINT_CONTACT_STATISTICS
	printf("num manifolds: %i\n",gNumManifold);
	printf("num gOverlappingPairs: %i\n",gOverlappingPairs);
	printf("num gTotalContactPoints : %i\n",gTotalContactPoints );
#endif //PRINT_CONTACT_STATISTICS

	gTotalContactPoints = 0;
	glutSwapBuffers();

}



void BasicDemo::displayCallback(void) {

	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT); 

	m_dynamicsWorld->updateAabbs();
	
	renderme();

	glFlush();
	glutSwapBuffers();
}



///make this positive to show stack falling from a distance
///this shows the penalty tresholds in action, springy/spungy look

void BasicDemo::clientResetScene()
{
}


void	BasicDemo::initPhysics()
{

	btCollisionDispatcher* dispatcher = new	btCollisionDispatcher(true);
	
#ifdef USE_SWEEP_AND_PRUNE
	btVector3 worldAabbMin(-10000,-10000,-10000);
	btVector3 worldAabbMax(10000,10000,10000);
	btOverlappingPairCache* broadphase = new btAxisSweep3(worldAabbMin,worldAabbMax,maxProxies);
#else
	btOverlappingPairCache* broadphase = new btSimpleBroadphase;
#endif //USE_SWEEP_AND_PRUNE
	
	dispatcher->registerCollisionCreateFunc(SPHERE_SHAPE_PROXYTYPE,SPHERE_SHAPE_PROXYTYPE,new btSphereSphereCollisionAlgorithm::CreateFunc);
#ifdef USE_GROUND_BOX
	dispatcher->registerCollisionCreateFunc(SPHERE_SHAPE_PROXYTYPE,BOX_SHAPE_PROXYTYPE,new btSphereBoxCollisionAlgorithm::CreateFunc);
	btCollisionAlgorithmCreateFunc* swappedSphereBox = new btSphereBoxCollisionAlgorithm::CreateFunc;
	swappedSphereBox->m_swapped = true;
	dispatcher->registerCollisionCreateFunc(BOX_SHAPE_PROXYTYPE,SPHERE_SHAPE_PROXYTYPE,swappedSphereBox);
#endif //USE_GROUND_BOX

	btSequentialImpulseConstraintSolver* solver = new btSequentialImpulseConstraintSolver;

	m_dynamicsWorld = new btSimpleDynamicsWorld(dispatcher,broadphase,solver);
	m_dynamicsWorld->setGravity(btVector3(0,-10,0));

	m_dynamicsWorld->setDebugDrawer(&debugDrawer);

	///create a few basic rigid bodies


	//static ground
#ifdef USE_GROUND_BOX
	btCollisionShape* groundShape = new btBoxShape(btVector3(50.f,50.f,50.f));
#else
	btCollisionShape* groundShape = new btSphereShape(50.f);
#endif//USE_GROUND_BOX

	btTransform groundTransform;
	groundTransform.setIdentity();
	groundTransform.setOrigin(btVector3(0,-50,0));
	localCreateRigidBody(0.f,groundTransform,groundShape);

	//create a few dynamic sphere rigidbodies (re-using the same sphere shape)
	btCollisionShape* sphereShape = new btSphereShape(1.f);
	int i;
	for (i=0;i<gNumObjects;i++)
	{
		
		sphereShape->setMargin(gCollisionMargin);
		btTransform trans;
		trans.setIdentity();
		//stack them
		int colsize = 10;
		int row = (i*HALF_EXTENTS*2)/(colsize*2*HALF_EXTENTS);
		int row2 = row;
		int col = (i)%(colsize)-colsize/2;
		btVector3 pos(col*2*HALF_EXTENTS + (row2%2)*HALF_EXTENTS,
			row*2*HALF_EXTENTS+HALF_EXTENTS,0);

		trans.setOrigin(pos);
		btRigidBody* body = localCreateRigidBody(1.f,trans,sphereShape);
	}

	clientResetScene();
}
	



