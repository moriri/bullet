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

///btSoftBody implementation by Nathanael Presson


#include "btBulletDynamicsCommon.h"
#include "BulletSoftBody/btSoftRigidDynamicsWorld.h"

#include "BulletCollision/CollisionDispatch/btSphereSphereCollisionAlgorithm.h"
#include "BulletCollision/NarrowPhaseCollision/btGjkEpa2.h"
#include "LinearMath/btQuickprof.h"
#include "LinearMath/btIDebugDraw.h"

#include "../GimpactTestDemo/BunnyMesh.h"
#include "../GimpactTestDemo/TorusMesh.h"
#include <stdio.h> //printf debugging
#include "LinearMath/btConvexHull.h"
#include "BulletSoftBody/btSoftBodyRigidBodyCollisionConfiguration.h"
#include "BulletSoftBody/btSoftBodyHelpers.h"

#include "SoftDemo.h"
#include "GL_ShapeDrawer.h"

#include "GlutStuff.h"

extern float eye[3];
extern int glutScreenWidth;
extern int glutScreenHeight;

const int maxProxies = 32766;
const int maxOverlap = 65535;

static btVector3*	gGroundVertices=0;
static int*	gGroundIndices=0;
static btBvhTriangleMeshShape* trimeshShape =0;
static btRigidBody* staticBody = 0;
static float waveheight = 5.f;

const float TRIANGLE_SIZE=8.f;



#ifdef _DEBUG
const int gNumObjects = 1;
#else
const int gNumObjects = 1;//try this in release mode: 3000. never go above 16384, unless you increate maxNumObjects  value in DemoApplication.cp
#endif

const int maxNumObjects = 32760;

#define CUBE_HALF_EXTENTS 1.5
#define EXTRA_HEIGHT -10.f

//
void SoftDemo::createStack( btCollisionShape* boxShape, float halfCubeSize, int size, float zPos )
{
	btTransform trans;
	trans.setIdentity();

	for(int i=0; i<size; i++)
	{
		// This constructs a row, from left to right
		int rowSize = size - i;
		for(int j=0; j< rowSize; j++)
		{
			btVector3 pos;
			pos.setValue(
				-rowSize * halfCubeSize + halfCubeSize + j * 2.0f * halfCubeSize,
				halfCubeSize + i * halfCubeSize * 2.0f,
				zPos);

			trans.setOrigin(pos);
			btScalar mass = 1.f;

			btRigidBody* body = 0;
			body = localCreateRigidBody(mass,trans,boxShape);

		}
	}
}


////////////////////////////////////

extern int gNumManifold;
extern int gOverlappingPairs;


void SoftDemo::clientMoveAndDisplay()
{
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT|GL_STENCIL_BUFFER_BIT); 




	float dt = 1.0/60.;	

	if (m_dynamicsWorld)
	{
		if(m_drag)
		{
			const int				x=m_lastmousepos[0];
			const int				y=m_lastmousepos[1];
			const btVector3			rayFrom=m_cameraPosition;
			const btVector3			rayTo=getRayTo(x,y);
			const btVector3			rayDir=(rayTo-rayFrom).normalized();
			const btVector3			N=(m_cameraTargetPosition-m_cameraPosition).normalized();
			const btScalar			O=dot(m_impact,N);
			const btScalar			den=dot(N,rayDir);
			if((den*den)>0)
			{
				const btScalar			num=O-dot(N,rayFrom);
				const btScalar			hit=num/den;
				if((hit>0)&&(hit<1500))
				{				
					m_goal=rayFrom+rayDir*hit;
				}				
			}		
			btVector3				delta=m_goal-m_node->m_x;
			static const btScalar	maxdrag=10;
			if(delta.length2()>(maxdrag*maxdrag))
			{
				delta=delta.normalized()*maxdrag;
			}
			m_node->m_v+=delta/dt;
		}

#define FIXED_STEP
#ifdef FIXED_STEP
		m_dynamicsWorld->stepSimulation(dt=1.0f/60.f,0);

#else
		//during idle mode, just run 1 simulation step maximum, otherwise 4 at max
		int maxSimSubSteps = m_idle ? 1 : 4;
		//if (m_idle)
		//	dt = 1.0/420.f;

		int numSimSteps;
		numSimSteps = m_dynamicsWorld->stepSimulation(dt);

#ifdef VERBOSE_TIMESTEPPING_CONSOLEOUTPUT
		if (!numSimSteps)
			printf("Interpolated transforms\n");
		else
		{
			if (numSimSteps > maxSimSubSteps)
			{
				//detect dropping frames
				printf("Dropped (%i) simulation steps out of %i\n",numSimSteps - maxSimSubSteps,numSimSteps);
			} else
			{
				printf("Simulated (%i) steps\n",numSimSteps);
			}
		}
#endif //VERBOSE_TIMESTEPPING_CONSOLEOUTPUT

#endif		

		if(m_drag)
		{
			m_node->m_v*=0;
		}

		m_softBodyWorldInfo.m_sparsesdf.GarbageCollect();

		//optional but useful: debug drawing

	}

#ifdef USE_QUICKPROF 
	btProfiler::beginBlock("render"); 
#endif //USE_QUICKPROF 

	renderme(); 

	//render the graphics objects, with center of mass shift

	updateCamera();



#ifdef USE_QUICKPROF 
	btProfiler::endBlock("render"); 
#endif 
	glFlush();
	//some additional debugging info
#ifdef PRINT_CONTACT_STATISTICS
	printf("num manifolds: %i\n",gNumManifold);
	printf("num gOverlappingPairs: %i\n",gOverlappingPairs);
	
#endif //PRINT_CONTACT_STATISTICS


	glutSwapBuffers();

}



void SoftDemo::displayCallback(void) {

	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT); 


	renderme();

	glFlush();
	glutSwapBuffers();
}


//
// ImplicitShape
//

//
struct	ImplicitSphere : btSoftBody::ImplicitFn
{
	btVector3	center;
	btScalar	sqradius;
	ImplicitSphere() {}
	ImplicitSphere(const btVector3& c,btScalar r) : center(c),sqradius(r*r) {}
	btScalar	Eval(const btVector3& x)
	{
		return((x-center).length2()-sqradius);
	}
};

//
// Tetra meshes
//

struct	TetraBunny
{
#include "bunny.inl"
};

struct	TetraCube
{
#include "cube.inl"
};


//
// Random
//

static inline btScalar	UnitRand()
{
	return(rand()/(btScalar)RAND_MAX);
}

static inline btScalar	SignedUnitRand()
{
	return(UnitRand()*2-1);
}

static inline btVector3	Vector3Rand()
{
	const btVector3	p=btVector3(SignedUnitRand(),SignedUnitRand(),SignedUnitRand());
	return(p.normalized());
}

//
// Rb rain
//
static void	Ctor_RbUpStack(SoftDemo* pdemo,int count)
{
	float				mass=10;

	btCompoundShape* cylinderCompound = new btCompoundShape;
	btCollisionShape* cylinderShape = new btCylinderShapeX(btVector3(4,1,1));
	btCollisionShape* boxShape = new btBoxShape(btVector3(4,1,1));
	btTransform localTransform;
	localTransform.setIdentity();
	cylinderCompound->addChildShape(localTransform,boxShape);
	btQuaternion orn(SIMD_HALF_PI,0,0);
	localTransform.setRotation(orn);
	//	localTransform.setOrigin(btVector3(1,1,1));
	cylinderCompound->addChildShape(localTransform,cylinderShape);


	btCollisionShape*	shape[]={	cylinderCompound,
		new btSphereShape(1.5),
		new btBoxShape(btVector3(1,1,1))
	};
	static const int	nshapes=sizeof(shape)/sizeof(shape[0]);
	for(int i=0;i<count;++i)
	{
		btTransform startTransform;
		startTransform.setIdentity();
		startTransform.setOrigin(btVector3(0,2+6*i,0));
		pdemo->localCreateRigidBody(mass,startTransform,shape[i%nshapes]);
	}
}

//
// Big ball
//
static void	Ctor_BigBall(SoftDemo* pdemo,btScalar mass=10)
{
	btTransform startTransform;
	startTransform.setIdentity();
	startTransform.setOrigin(btVector3(0,13,0));
	pdemo->localCreateRigidBody(mass,startTransform,new btSphereShape(3));
}

//
// Big plate
//
static btRigidBody*	Ctor_BigPlate(SoftDemo* pdemo,btScalar mass=15,btScalar height=4)
{
	btTransform startTransform;
	startTransform.setIdentity();
	startTransform.setOrigin(btVector3(0,height,0.5));
	btRigidBody*		body=pdemo->localCreateRigidBody(mass,startTransform,new btBoxShape(btVector3(5,1,5)));
	body->setFriction(1);
	return(body);
}

//
// Linear stair
//
static void Ctor_LinearStair(SoftDemo* pdemo,const btVector3& org,const btVector3& sizes,btScalar angle,int count)
{
	btBoxShape*	shape=new btBoxShape(sizes);
	for(int i=0;i<count;++i)
	{
		btTransform startTransform;
		startTransform.setIdentity();
		startTransform.setOrigin(org+btVector3(sizes.x()*i*2,sizes.y()*i*2,0));
		btRigidBody* body=pdemo->localCreateRigidBody(0,startTransform,shape);
		body->setFriction(1);
	}
}

//
// Softbox
//
static btSoftBody* Ctor_SoftBox(SoftDemo* pdemo,const btVector3& p,const btVector3& s)
{
	const btVector3	h=s*0.5;
	const btVector3	c[]={	p+h*btVector3(-1,-1,-1),
		p+h*btVector3(+1,-1,-1),
		p+h*btVector3(-1,+1,-1),
		p+h*btVector3(+1,+1,-1),
		p+h*btVector3(-1,-1,+1),
		p+h*btVector3(+1,-1,+1),
		p+h*btVector3(-1,+1,+1),
		p+h*btVector3(+1,+1,+1)};
	btSoftBody*		psb=btSoftBodyHelpers::CreateFromConvexHull(pdemo->m_softBodyWorldInfo,c,8);
	psb->generateBendingConstraints(2);
	pdemo->getSoftDynamicsWorld()->addSoftBody(psb);

	return(psb);

}

//
// SoftBoulder
//
static btSoftBody* Ctor_SoftBoulder(SoftDemo* pdemo,const btVector3& p,const btVector3& s,int np,int id)
{
	btAlignedObjectArray<btVector3>	pts;
	if(id) srand(id);
	for(int i=0;i<np;++i)
	{
		pts.push_back(Vector3Rand()*s+p);
	}
	btSoftBody*		psb=btSoftBodyHelpers::CreateFromConvexHull(pdemo->m_softBodyWorldInfo,&pts[0],pts.size());
	psb->generateBendingConstraints(2);
	pdemo->getSoftDynamicsWorld()->addSoftBody(psb);

	return(psb);
}

//#define TRACEDEMO { pdemo->demoname=__FUNCTION__+5;printf("Launching demo: " __FUNCTION__ "\r\n"); }

//
// Basic ropes
//
static void	Init_Ropes(SoftDemo* pdemo)
{
	//TRACEDEMO
	const int n=15;
	for(int i=0;i<n;++i)
	{
		btSoftBody*	psb=btSoftBodyHelpers::CreateRope(pdemo->m_softBodyWorldInfo,	btVector3(-10,0,i*0.25),
			btVector3(10,0,i*0.25),
			16,
			1+2);
		psb->m_cfg.piterations		=	4;
		psb->m_materials[0]->m_kLST	=	0.1+(i/(btScalar)(n-1))*0.9;
		psb->setTotalMass(20);
		pdemo->getSoftDynamicsWorld()->addSoftBody(psb);

	}
}

//
// Rope attach
//
static void	Init_RopeAttach(SoftDemo* pdemo)
{
	//TRACEDEMO
	pdemo->m_softBodyWorldInfo.m_sparsesdf.RemoveReferences(0);
	struct	Functors
	{
		static btSoftBody* CtorRope(SoftDemo* pdemo,const btVector3& p)
		{
			btSoftBody*	psb=btSoftBodyHelpers::CreateRope(pdemo->m_softBodyWorldInfo,p,p+btVector3(10,0,0),8,1);
			psb->setTotalMass(50);
			pdemo->getSoftDynamicsWorld()->addSoftBody(psb);
			return(psb);
		}
	};
	btTransform startTransform;
	startTransform.setIdentity();
	startTransform.setOrigin(btVector3(12,8,0));
	btRigidBody*		body=pdemo->localCreateRigidBody(50,startTransform,new btBoxShape(btVector3(2,6,2)));
	btSoftBody*	psb0=Functors::CtorRope(pdemo,btVector3(0,8,-1));
	btSoftBody*	psb1=Functors::CtorRope(pdemo,btVector3(0,8,+1));
	psb0->appendAnchor(psb0->m_nodes.size()-1,body);
	psb1->appendAnchor(psb1->m_nodes.size()-1,body);
}

//
// Cloth attach
//
static void	Init_ClothAttach(SoftDemo* pdemo)
{
	//TRACEDEMO
	const btScalar	s=4;
	const btScalar	h=6;
	const int		r=9;
	btSoftBody*		psb=btSoftBodyHelpers::CreatePatch(pdemo->m_softBodyWorldInfo,btVector3(-s,h,-s),
		btVector3(+s,h,-s),
		btVector3(-s,h,+s),
		btVector3(+s,h,+s),r,r,4+8,true);
	pdemo->getSoftDynamicsWorld()->addSoftBody(psb);

	btTransform startTransform;
	startTransform.setIdentity();
	startTransform.setOrigin(btVector3(0,h,-(s+3.5)));
	btRigidBody*		body=pdemo->localCreateRigidBody(20,startTransform,new btBoxShape(btVector3(s,1,3)));
	psb->appendAnchor(0,body);
	psb->appendAnchor(r-1,body);
	pdemo->m_cutting=true;
}

//
// Impact
//
static void	Init_Impact(SoftDemo* pdemo)
{
	//TRACEDEMO
	btSoftBody*	psb=btSoftBodyHelpers::CreateRope(pdemo->m_softBodyWorldInfo,	btVector3(0,0,0),
		btVector3(0,-1,0),
		0,
		1);
	pdemo->getSoftDynamicsWorld()->addSoftBody(psb);
	psb->m_cfg.kCHR=0.5;
	btTransform startTransform;
	startTransform.setIdentity();
	startTransform.setOrigin(btVector3(0,20,0));
	pdemo->localCreateRigidBody(10,startTransform,new btBoxShape(btVector3(2,2,2)));
}

//
// Collide
//
static void	Init_Collide(SoftDemo* pdemo)
{
	//TRACEDEMO
	struct Functor
	{
		static btSoftBody* Create(SoftDemo* pdemo,const btVector3& x,const btVector3& a)
		{
			btSoftBody*	psb=btSoftBodyHelpers::CreateFromTriMesh(pdemo->m_softBodyWorldInfo,gVertices,
				&gIndices[0][0],
				NUM_TRIANGLES);
			psb->generateBendingConstraints(2);
			psb->m_cfg.piterations=2;
			psb->m_cfg.collisions|=btSoftBody::fCollision::VF_SS;
			psb->randomizeConstraints();
			btMatrix3x3	m;
			m.setEulerZYX(a.x(),a.y(),a.z());
			psb->transform(btTransform(m,x));
			psb->scale(btVector3(2,2,2));
			psb->setTotalMass(50,true);
			pdemo->getSoftDynamicsWorld()->addSoftBody(psb);
			return(psb);
		}
	};
	for(int i=0;i<3;++i)
	{
		Functor::Create(pdemo,btVector3(3*i,2,0),btVector3(SIMD_PI/2*(1-(i&1)),SIMD_PI/2*(i&1),0));
	}
	pdemo->m_cutting=true;
}

//
// Collide2
//
static void	Init_Collide2(SoftDemo* pdemo)
{
	//TRACEDEMO
	struct Functor
	{
		static btSoftBody* Create(SoftDemo* pdemo,const btVector3& x,const btVector3& a)
		{
			btSoftBody*	psb=btSoftBodyHelpers::CreateFromTriMesh(pdemo->m_softBodyWorldInfo,gVerticesBunny,
				&gIndicesBunny[0][0],
				BUNNY_NUM_TRIANGLES);
			btSoftBody::Material*	pm=psb->appendMaterial();
			pm->m_kLST				=	0.5;
			pm->m_flags				-=	btSoftBody::fMaterial::DebugDraw;
			psb->generateBendingConstraints(2,pm);
			psb->m_cfg.piterations	=	2;
			psb->m_cfg.kDF			=	0.5;
			psb->m_cfg.collisions	|=	btSoftBody::fCollision::VF_SS;
			psb->randomizeConstraints();
			btMatrix3x3	m;
			m.setEulerZYX(a.x(),a.y(),a.z());
			psb->transform(btTransform(m,x));
			psb->scale(btVector3(6,6,6));
			psb->setTotalMass(100,true);
			pdemo->getSoftDynamicsWorld()->addSoftBody(psb);
			return(psb);
		}
	};
	for(int i=0;i<3;++i)
	{
		Functor::Create(pdemo,btVector3(0,-1+5*i,0),btVector3(0,SIMD_PI/2*(i&1),0));
	}	
	pdemo->m_cutting=true;
}

//
// Collide3
//
static void	Init_Collide3(SoftDemo* pdemo)
{
	//TRACEDEMO
	{
		const btScalar	s=8;
		btSoftBody*		psb=btSoftBodyHelpers::CreatePatch(	pdemo->m_softBodyWorldInfo,btVector3(-s,0,-s),
			btVector3(+s,0,-s),
			btVector3(-s,0,+s),
			btVector3(+s,0,+s),
			15,15,1+2+4+8,true);
		psb->m_materials[0]->m_kLST	=	0.4;
		psb->m_cfg.collisions		|=	btSoftBody::fCollision::VF_SS;
		psb->setTotalMass(150);
		pdemo->getSoftDynamicsWorld()->addSoftBody(psb);
	}
	{		
		const btScalar	s=4;
		const btVector3	o=btVector3(5,10,0);
		btSoftBody*		psb=btSoftBodyHelpers::CreatePatch(	pdemo->m_softBodyWorldInfo,
			btVector3(-s,0,-s)+o,
			btVector3(+s,0,-s)+o,
			btVector3(-s,0,+s)+o,
			btVector3(+s,0,+s)+o,
			7,7,0,true);
		btSoftBody::Material*	pm=psb->appendMaterial();
		pm->m_kLST				=	0.1;
		pm->m_flags				-=	btSoftBody::fMaterial::DebugDraw;
		psb->generateBendingConstraints(2,pm);
		psb->m_materials[0]->m_kLST	=	0.5;
		psb->m_cfg.collisions		|=	btSoftBody::fCollision::VF_SS;
		psb->setTotalMass(150);
		pdemo->getSoftDynamicsWorld()->addSoftBody(psb);
		pdemo->m_cutting=true;
	}
}

//
// Aerodynamic forces, 50x1g flyers
//
static void	Init_Aero(SoftDemo* pdemo)
{
	//TRACEDEMO
	const btScalar	s=2;
	const btScalar	h=10;
	const int		segments=6;
	const int		count=50;
	for(int i=0;i<count;++i)
	{
		btSoftBody*		psb=btSoftBodyHelpers::CreatePatch(pdemo->m_softBodyWorldInfo,btVector3(-s,h,-s),
			btVector3(+s,h,-s),
			btVector3(-s,h,+s),
			btVector3(+s,h,+s),
			segments,segments,
			0,true);
		btSoftBody::Material*	pm=psb->appendMaterial();
		pm->m_flags				-=	btSoftBody::fMaterial::DebugDraw;
		psb->generateBendingConstraints(2,pm);
		psb->m_cfg.kLF			=	0.004;
		psb->m_cfg.kDG			=	0.0003;
		psb->m_cfg.aeromodel	=	btSoftBody::eAeroModel::V_TwoSided;
		btTransform		trs;
		btQuaternion	rot;
		btVector3		ra=Vector3Rand()*0.1;
		btVector3		rp=Vector3Rand()*15+btVector3(0,20,80);
		rot.setEuler(SIMD_PI/8+ra.x(),-SIMD_PI/7+ra.y(),ra.z());
		trs.setIdentity();
		trs.setOrigin(rp);
		trs.setRotation(rot);
		psb->transform(trs);
		psb->setTotalMass(0.1);
		psb->addForce(btVector3(0,2,0),0);
		pdemo->getSoftDynamicsWorld()->addSoftBody(psb);

	}
	pdemo->m_autocam=true;
}

//
// Friction
//
static void	Init_Friction(SoftDemo* pdemo)
{
	//TRACEDEMO
	const btScalar	bs=2;
	const btScalar	ts=bs+bs/4;
	for(int i=0,ni=20;i<ni;++i)
	{
		const btVector3	p(-ni*ts/2+i*ts,-10+bs,40);
		btSoftBody*		psb=Ctor_SoftBox(pdemo,p,btVector3(bs,bs,bs));
		psb->m_cfg.kDF	=	0.1 * ((i+1)/(btScalar)ni);
		psb->addVelocity(btVector3(0,0,-10));
	}
}

//
// Pressure
//
static void	Init_Pressure(SoftDemo* pdemo)
{
	//TRACEDEMO
	btSoftBody*	psb=btSoftBodyHelpers::CreateEllipsoid(pdemo->m_softBodyWorldInfo,btVector3(35,25,0),
		btVector3(1,1,1)*3,
		512);
	psb->m_materials[0]->m_kLST	=	0.1;
	psb->m_cfg.kDF				=	1;
	psb->m_cfg.kDP				=	0.001; // fun factor...
	psb->m_cfg.kPR				=	2500;
	psb->setTotalMass(30,true);
	pdemo->getSoftDynamicsWorld()->addSoftBody(psb);

	Ctor_BigPlate(pdemo);
	Ctor_LinearStair(pdemo,btVector3(0,0,0),btVector3(2,1,5),0,10);
	pdemo->m_autocam=true;
}

//
// Volume conservation
//
static void	Init_Volume(SoftDemo* pdemo)
{
	//TRACEDEMO
	btSoftBody*	psb=btSoftBodyHelpers::CreateEllipsoid(pdemo->m_softBodyWorldInfo,btVector3(35,25,0),
		btVector3(1,1,1)*3,
		512);
	psb->m_materials[0]->m_kLST	=	0.45;
	psb->m_cfg.kVC				=	20;
	psb->setTotalMass(50,true);
	psb->setPose(true,false);
	pdemo->getSoftDynamicsWorld()->addSoftBody(psb);

	Ctor_BigPlate(pdemo);
	Ctor_LinearStair(pdemo,btVector3(0,0,0),btVector3(2,1,5),0,10);
	pdemo->m_autocam=true;
}

//
// Stick+Bending+Rb's
//
static void	Init_Sticks(SoftDemo* pdemo)
{
	//TRACEDEMO
	const int		n=16;
	const int		sg=4;
	const btScalar	sz=5;
	const btScalar	hg=4;
	const btScalar	in=1/(btScalar)(n-1);
	for(int y=0;y<n;++y)
	{
		for(int x=0;x<n;++x)
		{
			const btVector3	org(-sz+sz*2*x*in,
				-10,
				-sz+sz*2*y*in);
			btSoftBody*		psb=btSoftBodyHelpers::CreateRope(	pdemo->m_softBodyWorldInfo,	org,
				org+btVector3(hg*0.001,hg,0),
				sg,
				1);
			psb->m_cfg.kDP		=	0.005;
			psb->m_cfg.kCHR		=	0.1;
			for(int i=0;i<3;++i)
			{
				psb->generateBendingConstraints(2+i);
			}
			psb->setMass(1,0);
			psb->setTotalMass(0.01);
			pdemo->getSoftDynamicsWorld()->addSoftBody(psb);

		}
	}
	Ctor_BigBall(pdemo);
}

//
// Bending
//
static void	Init_Bending(SoftDemo* pdemo)
{
	//TRACEDEMO
	const btScalar	s=4;
	const btVector3	x[]={	btVector3(-s,0,-s),
		btVector3(+s,0,-s),
		btVector3(+s,0,+s),
		btVector3(-s,0,+s)};
	const btScalar	m[]={	0,0,0,1};
	btSoftBody*		psb=new btSoftBody(&pdemo->m_softBodyWorldInfo,4,x,m);
	psb->appendLink(0,1);
	psb->appendLink(1,2);
	psb->appendLink(2,3);
	psb->appendLink(3,0);
	psb->appendLink(0,2);

	pdemo->getSoftDynamicsWorld()->addSoftBody(psb);
}

//
// 100kg cloth locked at corners, 10 falling 10kg rb's.
//
static void	Init_Cloth(SoftDemo* pdemo)
{
	//TRACEDEMO
	const btScalar	s=8;
	btSoftBody*		psb=btSoftBodyHelpers::CreatePatch(	pdemo->m_softBodyWorldInfo,btVector3(-s,0,-s),
		btVector3(+s,0,-s),
		btVector3(-s,0,+s),
		btVector3(+s,0,+s),
		31,31,

		//		31,31,
		1+2+4+8,true);
	btSoftBody::Material* pm=psb->appendMaterial();
	pm->m_kLST		=	0.4;
	pm->m_flags		-=	btSoftBody::fMaterial::DebugDraw;
	psb->generateBendingConstraints(2,pm);
	psb->setTotalMass(150);
	pdemo->getSoftDynamicsWorld()->addSoftBody(psb);

	Ctor_RbUpStack(pdemo,10);
	pdemo->m_cutting=true;
}

//
// 100kg Stanford's bunny
//
static void	Init_Bunny(SoftDemo* pdemo)
{
	//TRACEDEMO
	btSoftBody*	psb=btSoftBodyHelpers::CreateFromTriMesh(pdemo->m_softBodyWorldInfo,gVerticesBunny,
		&gIndicesBunny[0][0],
		BUNNY_NUM_TRIANGLES);
	btSoftBody::Material*	pm=psb->appendMaterial();
	pm->m_kLST				=	0.5;
	pm->m_flags				-=	btSoftBody::fMaterial::DebugDraw;
	psb->generateBendingConstraints(2,pm);
	psb->m_cfg.piterations	=	2;
	psb->m_cfg.kDF			=	0.5;
	psb->randomizeConstraints();
	psb->scale(btVector3(6,6,6));
	psb->setTotalMass(100,true);	
	pdemo->getSoftDynamicsWorld()->addSoftBody(psb);
	pdemo->m_cutting=true;

}

//
// 100kg Stanford's bunny with pose matching
//
static void	Init_BunnyMatch(SoftDemo* pdemo)
{
	//TRACEDEMO
	btSoftBody*	psb=btSoftBodyHelpers::CreateFromTriMesh(pdemo->m_softBodyWorldInfo,	gVerticesBunny,
		&gIndicesBunny[0][0],
		BUNNY_NUM_TRIANGLES);
	psb->m_cfg.kDF				=	0.5;
	psb->m_cfg.kMT				=	0.05;
	psb->m_cfg.piterations		=	5;
	psb->randomizeConstraints();
	psb->scale(btVector3(6,6,6));
	psb->setTotalMass(100,true);
	psb->setPose(false,true);
	pdemo->getSoftDynamicsWorld()->addSoftBody(psb);	

}

//
// 50Kg Torus
//
static void	Init_Torus(SoftDemo* pdemo)
{
	//TRACEDEMO
	btSoftBody*	psb=btSoftBodyHelpers::CreateFromTriMesh(	pdemo->m_softBodyWorldInfo,	gVertices,
		&gIndices[0][0],
		NUM_TRIANGLES);
	psb->generateBendingConstraints(2);
	psb->m_cfg.piterations=2;
	psb->randomizeConstraints();
	btMatrix3x3	m;
	m.setEulerZYX(SIMD_PI/2,0,0);
	psb->transform(btTransform(m,btVector3(0,4,0)));
	psb->scale(btVector3(2,2,2));
	psb->setTotalMass(50,true);
	pdemo->getSoftDynamicsWorld()->addSoftBody(psb);
	pdemo->m_cutting=true;

}

//
// 50Kg Torus with pose matching
//
static void	Init_TorusMatch(SoftDemo* pdemo)
{
	//TRACEDEMO
	btSoftBody*	psb=btSoftBodyHelpers::CreateFromTriMesh(pdemo->m_softBodyWorldInfo,	gVertices,
		&gIndices[0][0],
		NUM_TRIANGLES);
	psb->m_materials[0]->m_kLST	=	0.1;
	psb->m_cfg.kMT				=	0.05;
	psb->randomizeConstraints();
	btMatrix3x3	m;
	m.setEulerZYX(SIMD_PI/2,0,0);
	psb->transform(btTransform(m,btVector3(0,4,0)));
	psb->scale(btVector3(2,2,2));
	psb->setTotalMass(50,true);
	psb->setPose(false,true);
	pdemo->getSoftDynamicsWorld()->addSoftBody(psb);
}

//
// Cutting1
//
static void	Init_Cutting1(SoftDemo* pdemo)
{
	const btScalar	s=6;
	const btScalar	h=2;
	const int		r=16;	
	const btVector3	p[]={	btVector3(+s,h,-s),
		btVector3(-s,h,-s),
		btVector3(+s,h,+s),
		btVector3(-s,h,+s)};
	btSoftBody*	psb=btSoftBodyHelpers::CreatePatch(pdemo->m_softBodyWorldInfo,p[0],p[1],p[2],p[3],r,r,1+2+4+8,true);
	pdemo->getSoftDynamicsWorld()->addSoftBody(psb);
	psb->m_cfg.piterations=1;
	pdemo->m_cutting=true;	
}

//
// Clusters
//

//
static void			Ctor_Gear(SoftDemo* pdemo,const btVector3& pos,btScalar speed)
{
	btTransform startTransform;
	startTransform.setIdentity();
	startTransform.setOrigin(pos);
	btCompoundShape*	shape=new btCompoundShape();
#if 1
	shape->addChildShape(btTransform(btQuaternion(0,0,0)),new btBoxShape(btVector3(5,1,6)));
	shape->addChildShape(btTransform(btQuaternion(0,0,SIMD_HALF_PI)),new btBoxShape(btVector3(5,1,6)));
#else
	shape->addChildShape(btTransform(btQuaternion(0,0,0)),new btCylinderShapeZ(btVector3(5,1,7)));
	shape->addChildShape(btTransform(btQuaternion(0,0,SIMD_HALF_PI)),new btBoxShape(btVector3(4,1,8)));
#endif
	btRigidBody*		body=pdemo->localCreateRigidBody(10,startTransform,shape);
	body->setFriction(1);
	btDynamicsWorld*	world=pdemo->getDynamicsWorld();
	btHingeConstraint*	hinge=new btHingeConstraint(*body,btTransform::getIdentity());
	if(speed!=0) hinge->enableAngularMotor(true,speed,3);
	world->addConstraint(hinge);
}

//
static btSoftBody*	Ctor_ClusterBunny(SoftDemo* pdemo,const btVector3& x,const btVector3& a)
{
	btSoftBody*	psb=btSoftBodyHelpers::CreateFromTriMesh(pdemo->m_softBodyWorldInfo,gVerticesBunny,&gIndicesBunny[0][0],BUNNY_NUM_TRIANGLES);
	btSoftBody::Material*	pm=psb->appendMaterial();
	pm->m_kLST				=	1;
	pm->m_flags				-=	btSoftBody::fMaterial::DebugDraw;
	psb->generateBendingConstraints(2,pm);
	psb->m_cfg.piterations	=	2;
	psb->m_cfg.kDF			=	1;
	psb->m_cfg.collisions	=	btSoftBody::fCollision::CL_SS+
		btSoftBody::fCollision::CL_RS;
	psb->randomizeConstraints();
	btMatrix3x3	m;
	m.setEulerZYX(a.x(),a.y(),a.z());
	psb->transform(btTransform(m,x));
	psb->scale(btVector3(8,8,8));
	psb->setTotalMass(150,true);
	psb->generateClusters(1);
	pdemo->getSoftDynamicsWorld()->addSoftBody(psb);
	return(psb);
}

//
static btSoftBody*	Ctor_ClusterTorus(SoftDemo* pdemo,const btVector3& x,const btVector3& a,const btVector3& s=btVector3(2,2,2))
{
	btSoftBody*	psb=btSoftBodyHelpers::CreateFromTriMesh(pdemo->m_softBodyWorldInfo,gVertices,&gIndices[0][0],NUM_TRIANGLES);
	btSoftBody::Material*	pm=psb->appendMaterial();
	pm->m_kLST				=	1;
	pm->m_flags				-=	btSoftBody::fMaterial::DebugDraw;			
	psb->generateBendingConstraints(2,pm);
	psb->m_cfg.piterations	=	2;
	psb->m_cfg.collisions	=	btSoftBody::fCollision::CL_SS+
		btSoftBody::fCollision::CL_RS;
	psb->randomizeConstraints();
	psb->scale(s);
	psb->rotate(btQuaternion(a[0],a[1],a[2]));
	psb->translate(x);
	psb->setTotalMass(50,true);
	psb->generateClusters(64);			
	pdemo->getSoftDynamicsWorld()->addSoftBody(psb);
	return(psb);
}

//
static struct MotorControl : btSoftBody::AJoint::IControl
{
	MotorControl()
	{
		goal=0;
		maxtorque=0;
	}
	btScalar	Speed(btSoftBody::AJoint*,btScalar current)
	{
		return(current+btMin(maxtorque,btMax(-maxtorque,goal-current)));
	}
	btScalar	goal;
	btScalar	maxtorque;
}	motorcontrol;

//
struct SteerControl : btSoftBody::AJoint::IControl
{
	SteerControl(btScalar s)
	{
		angle=0;
		sign=s;
	}
	void		Prepare(btSoftBody::AJoint* joint)
	{
		joint->m_refs[0][0]=btCos(angle*sign);
		joint->m_refs[0][2]=btSin(angle*sign);
	}
	btScalar	Speed(btSoftBody::AJoint* joint,btScalar current)
	{
		return(motorcontrol.Speed(joint,current));
	}
	btScalar	angle;
	btScalar	sign;
};

static SteerControl	steercontrol_f(+1);
static SteerControl	steercontrol_r(-1);

//
static void	Init_ClusterDeform(SoftDemo* pdemo)
{
	btSoftBody*		psb=Ctor_ClusterTorus(pdemo,btVector3(0,0,0),btVector3(SIMD_PI/2,0,SIMD_HALF_PI));
	psb->generateClusters(8);
	psb->m_cfg.kDF=1;
}

//
static void	Init_ClusterCollide1(SoftDemo* pdemo)
{
	const btScalar	s=8;
	btSoftBody*		psb=btSoftBodyHelpers::CreatePatch(	pdemo->m_softBodyWorldInfo,btVector3(-s,0,-s),
		btVector3(+s,0,-s),
		btVector3(-s,0,+s),
		btVector3(+s,0,+s),
		31,31,
		1+2+4+8,true);
	btSoftBody::Material* pm=psb->appendMaterial();
	pm->m_kLST		=	0.4;
	pm->m_flags		-=	btSoftBody::fMaterial::DebugDraw;
	psb->m_cfg.kDF	=	1;
	psb->m_cfg.kSRHR_CL		=	1;
	psb->m_cfg.kSR_SPLT_CL	=	0;
	psb->m_cfg.collisions	=	btSoftBody::fCollision::CL_SS+
		btSoftBody::fCollision::CL_RS;
	psb->generateBendingConstraints(2,pm);
	psb->setTotalMass(50);
	psb->generateClusters(64);
	pdemo->getSoftDynamicsWorld()->addSoftBody(psb);

	Ctor_RbUpStack(pdemo,10);
}

//
static void	Init_ClusterCollide2(SoftDemo* pdemo)
{
	struct Functor
	{
		static btSoftBody* Create(SoftDemo* pdemo,const btVector3& x,const btVector3& a)
		{
			btSoftBody*	psb=btSoftBodyHelpers::CreateFromTriMesh(pdemo->m_softBodyWorldInfo,gVertices,
				&gIndices[0][0],
				NUM_TRIANGLES);
			btSoftBody::Material* pm=psb->appendMaterial();
			pm->m_flags		-=	btSoftBody::fMaterial::DebugDraw;
			psb->generateBendingConstraints(2,pm);
			psb->m_cfg.piterations=2;
			psb->m_cfg.kDF			=1;
			psb->m_cfg.kSSHR_CL		=1;
			psb->m_cfg.kSS_SPLT_CL	=0;
			psb->m_cfg.kSKHR_CL		=0.1f;
			psb->m_cfg.kSK_SPLT_CL	=1;
			psb->m_cfg.collisions=	btSoftBody::fCollision::CL_SS+
				btSoftBody::fCollision::CL_RS;		
			psb->randomizeConstraints();
			btMatrix3x3	m;
			m.setEulerZYX(a.x(),a.y(),a.z());
			psb->transform(btTransform(m,x));
			psb->scale(btVector3(2,2,2));
			psb->setTotalMass(50,true);
			psb->generateClusters(16);
			pdemo->getSoftDynamicsWorld()->addSoftBody(psb);
			return(psb);
		}
	};
	for(int i=0;i<3;++i)
	{
		Functor::Create(pdemo,btVector3(3*i,2,0),btVector3(SIMD_PI/2*(1-(i&1)),SIMD_PI/2*(i&1),0));
	}
}

//
static void	Init_ClusterSocket(SoftDemo* pdemo)
{
	btSoftBody*		psb=Ctor_ClusterTorus(pdemo,btVector3(0,0,0),btVector3(SIMD_PI/2,0,SIMD_HALF_PI));
	btRigidBody*	prb=Ctor_BigPlate(pdemo,50,8);
	psb->m_cfg.kDF=1;
	btSoftBody::LJoint::Specs	lj;
	lj.position	=	btVector3(0,5,0);
	psb->appendLinearJoint(lj,prb);
}

//
static void	Init_ClusterHinge(SoftDemo* pdemo)
{
	btSoftBody*		psb=Ctor_ClusterTorus(pdemo,btVector3(0,0,0),btVector3(SIMD_PI/2,0,SIMD_HALF_PI));
	btRigidBody*	prb=Ctor_BigPlate(pdemo,50,8);
	psb->m_cfg.kDF=1;
	btSoftBody::AJoint::Specs	aj;
	aj.axis	=	btVector3(0,0,1);
	psb->appendAngularJoint(aj,prb);
}

//
static void	Init_ClusterCombine(SoftDemo* pdemo)
{
	const btVector3	sz(2,4,2);		
	btSoftBody*	psb0=Ctor_ClusterTorus(pdemo,btVector3(0,8,0),btVector3(SIMD_PI/2,0,SIMD_HALF_PI),sz);
	btSoftBody*	psb1=Ctor_ClusterTorus(pdemo,btVector3(0,8,10),btVector3(SIMD_PI/2,0,SIMD_HALF_PI),sz);
	btSoftBody*	psbs[]={psb0,psb1};
	for(int j=0;j<2;++j)
	{
		psbs[j]->m_cfg.kDF=1;
		psbs[j]->m_cfg.kDP=0;
		psbs[j]->m_cfg.piterations=1;
		psbs[j]->m_clusters[0]->m_matching	=	0.05;
		psbs[j]->m_clusters[0]->m_ndamping	=	0.05;
	}
	btSoftBody::AJoint::Specs	aj;
	aj.axis		=	btVector3(0,0,1);
	aj.icontrol	=	&motorcontrol;
	psb0->appendAngularJoint(aj,psb1);

	btSoftBody::LJoint::Specs	lj;
	lj.position	=	btVector3(0,8,5);
	psb0->appendLinearJoint(lj,psb1);
}

//
static void	Init_ClusterCar(SoftDemo* pdemo)
{
	pdemo->setAzi(270);
	const btVector3		origin(100,80,0);
	const btQuaternion	orientation(-SIMD_PI/2,0,0);
	const btScalar	widthf=8;
	const btScalar	widthr=9;
	const btScalar	length=8;
	const btScalar	height=4;
	const btVector3	wheels[]=	{
		btVector3(+widthf,-height,+length),	// Front left
		btVector3(-widthf,-height,+length),	// Front right
		btVector3(+widthr,-height,-length),	// Rear left
		btVector3(-widthr,-height,-length),	// Rear right
	};
	btSoftBody*	pa=Ctor_ClusterBunny(pdemo,btVector3(0,0,0),btVector3(0,0,0));
	btSoftBody*	pfl=Ctor_ClusterTorus(pdemo,wheels[0],btVector3(0,0,SIMD_HALF_PI),btVector3(2,4,2));
	btSoftBody*	pfr=Ctor_ClusterTorus(pdemo,wheels[1],btVector3(0,0,SIMD_HALF_PI),btVector3(2,4,2));
	btSoftBody*	prl=Ctor_ClusterTorus(pdemo,wheels[2],btVector3(0,0,SIMD_HALF_PI),btVector3(2,5,2));
	btSoftBody*	prr=Ctor_ClusterTorus(pdemo,wheels[3],btVector3(0,0,SIMD_HALF_PI),btVector3(2,5,2));

	pfl->m_cfg.kDF	=
		pfr->m_cfg.kDF	=
		prl->m_cfg.kDF	=
		prr->m_cfg.kDF	=	1;

	btSoftBody::LJoint::Specs	lspecs;
	lspecs.cfm		=	1;
	lspecs.erp		=	1;
	lspecs.position	=	btVector3(0,0,0);

	lspecs.position=wheels[0];pa->appendLinearJoint(lspecs,pfl);
	lspecs.position=wheels[1];pa->appendLinearJoint(lspecs,pfr);
	lspecs.position=wheels[2];pa->appendLinearJoint(lspecs,prl);
	lspecs.position=wheels[3];pa->appendLinearJoint(lspecs,prr);

	btSoftBody::AJoint::Specs	aspecs;
	aspecs.cfm		=	1;
	aspecs.erp		=	1;
	aspecs.axis		=	btVector3(1,0,0);

	aspecs.icontrol	=	&steercontrol_f;
	pa->appendAngularJoint(aspecs,pfl);
	pa->appendAngularJoint(aspecs,pfr);

	aspecs.icontrol	=	&motorcontrol;
	pa->appendAngularJoint(aspecs,prl);
	pa->appendAngularJoint(aspecs,prr);

	pa->rotate(orientation);
	pfl->rotate(orientation);
	pfr->rotate(orientation);
	prl->rotate(orientation);
	prr->rotate(orientation);
	pa->translate(origin);
	pfl->translate(origin);
	pfr->translate(origin);
	prl->translate(origin);
	prr->translate(origin);
	pfl->m_cfg.piterations	=
		pfr->m_cfg.piterations	=
		prl->m_cfg.piterations	=
		prr->m_cfg.piterations	= 1;
	pfl->m_clusters[0]->m_matching	=
		pfr->m_clusters[0]->m_matching	=
		prl->m_clusters[0]->m_matching	=
		prr->m_clusters[0]->m_matching	= 0.05;
	pfl->m_clusters[0]->m_ndamping	=
		pfr->m_clusters[0]->m_ndamping	=
		prl->m_clusters[0]->m_ndamping	=
		prr->m_clusters[0]->m_ndamping	= 0.05;

	Ctor_LinearStair(pdemo,btVector3(0,-8,0),btVector3(3,2,40),0,20);
	Ctor_RbUpStack(pdemo,50);
	pdemo->m_autocam=true;
}

//
static void	Init_ClusterRobot(SoftDemo* pdemo)
{
	struct Functor
	{
		static btSoftBody*	CreateBall(SoftDemo* pdemo,const btVector3& pos)
		{
			btSoftBody*	psb=btSoftBodyHelpers::CreateEllipsoid(pdemo->m_softBodyWorldInfo,pos,btVector3(1,1,1)*3,512);
			psb->m_materials[0]->m_kLST	=	0.45;
			psb->m_cfg.kVC				=	20;
			psb->setTotalMass(50,true);
			psb->setPose(true,false);
			psb->generateClusters(1);
			pdemo->getSoftDynamicsWorld()->addSoftBody(psb);
			return(psb);
		}
	};
	const btVector3		base=btVector3(0,25,8);
	btSoftBody*			psb0=Functor::CreateBall(pdemo,base+btVector3(-8,0,0));
	btSoftBody*			psb1=Functor::CreateBall(pdemo,base+btVector3(+8,0,0));
	btSoftBody*			psb2=Functor::CreateBall(pdemo,base+btVector3(0,0,+8*btSqrt(2)));
	const btVector3		ctr=(psb0->clusterCom(0)+psb1->clusterCom(0)+psb2->clusterCom(0))/3;
	btCylinderShape*	pshp=new btCylinderShape(btVector3(8,1,8));
	btRigidBody*		prb=pdemo->localCreateRigidBody(50,btTransform(btQuaternion(0,0,0),ctr+btVector3(0,5,0)),pshp);
	btSoftBody::LJoint::Specs	ls;
	ls.erp=0.5f;
	ls.position=psb0->clusterCom(0);psb0->appendLinearJoint(ls,prb);
	ls.position=psb1->clusterCom(0);psb1->appendLinearJoint(ls,prb);
	ls.position=psb2->clusterCom(0);psb2->appendLinearJoint(ls,prb);

	btBoxShape*			pbox=new btBoxShape(btVector3(20,1,40));
	btRigidBody*		pgrn=pdemo->localCreateRigidBody(0,btTransform(btQuaternion(0,-SIMD_HALF_PI/2,0),btVector3(0,0,0)),pbox);

	pdemo->m_autocam=true;
}

//
static void	Init_ClusterStackSoft(SoftDemo* pdemo)
{
	for(int i=0;i<10;++i)
	{
		btSoftBody*	psb=Ctor_ClusterTorus(pdemo,btVector3(0,-9+8.25*i,0),btVector3(0,0,0));
		psb->m_cfg.kDF=1;
	}
}

//
static void	Init_ClusterStackMixed(SoftDemo* pdemo)
{
	for(int i=0;i<10;++i)
	{
		if((i+1)&1)
		{
			Ctor_BigPlate(pdemo,50,-9+4.25*i);
		}
		else
		{
			btSoftBody*	psb=Ctor_ClusterTorus(pdemo,btVector3(0,-9+4.25*i,0),btVector3(0,0,0));
			psb->m_cfg.kDF=1;
		}
	}
}

unsigned	current_demo=18;

void	SoftDemo::clientResetScene()
{
	DemoApplication::clientResetScene();
	/* Clean up	*/ 
	for(int i=m_dynamicsWorld->getNumCollisionObjects()-1;i>0;i--)
	{
		btCollisionObject*	obj=m_dynamicsWorld->getCollisionObjectArray()[i];
		btRigidBody*		body=btRigidBody::upcast(obj);
		if(body&&body->getMotionState())
		{
			delete body->getMotionState();
		}
		while(m_dynamicsWorld->getNumConstraints())
		{
			btTypedConstraint*	pc=m_dynamicsWorld->getConstraint(0);
			m_dynamicsWorld->removeConstraint(pc);
			delete pc;
		}
		btSoftBody* softBody = btSoftBody::upcast(obj);
		if (softBody)
		{
			getSoftDynamicsWorld()->removeSoftBody(softBody);
		} else
		{
			m_dynamicsWorld->removeCollisionObject(obj);
		}
		delete obj;
	}

	m_softBodyWorldInfo.m_sparsesdf.Reset();
	/* Init		*/ 
	void (*demofncs[])(SoftDemo*)=
	{
		Init_Cloth,
		Init_Pressure,
		Init_Volume,
		Init_Ropes,
		Init_RopeAttach,
		Init_ClothAttach,
		Init_Sticks,	
		Init_Collide,
		Init_Collide2,
		Init_Collide3,
		Init_Impact,
		Init_Aero,
		Init_Friction,			
		Init_Torus,
		Init_TorusMatch,
		Init_Bunny,
		Init_BunnyMatch,
		Init_Cutting1,
		Init_ClusterDeform,
		Init_ClusterCollide1,
		Init_ClusterCollide2,
		Init_ClusterSocket,
		Init_ClusterHinge,
		Init_ClusterCombine,
		Init_ClusterCar,
		Init_ClusterRobot,
		Init_ClusterStackSoft,
		Init_ClusterStackMixed,
	};
	current_demo=current_demo%(sizeof(demofncs)/sizeof(demofncs[0]));

	m_softBodyWorldInfo.air_density		=	(btScalar)1.2;
	m_softBodyWorldInfo.water_density	=	0;
	m_softBodyWorldInfo.water_offset		=	0;
	m_softBodyWorldInfo.water_normal		=	btVector3(0,0,0);
	m_softBodyWorldInfo.m_gravity.setValue(0,-10,0);


	m_autocam						=	false;
	m_raycast						=	false;
	m_cutting						=	false;
	m_results.fraction				=	1.f;
	demofncs[current_demo](this);
}

void	SoftDemo::renderme()
{
	btIDebugDraw*	idraw=m_dynamicsWorld->getDebugDrawer();

	m_dynamicsWorld->debugDrawWorld();

	/* Bodies		*/ 
	btVector3	ps(0,0,0);
	int			nps=0;

	btSoftBodyArray&	sbs=getSoftDynamicsWorld()->getSoftBodyArray();
	for(int ib=0;ib<sbs.size();++ib)
	{
		btSoftBody*	psb=sbs[ib];
		nps+=psb->m_nodes.size();
		for(int i=0;i<psb->m_nodes.size();++i)
		{
			ps+=psb->m_nodes[i].m_x;
		}		
	}
	ps/=nps;
	if(m_autocam)
		m_cameraTargetPosition+=(ps-m_cameraTargetPosition)*0.05;
	else
		m_cameraTargetPosition=btVector3(0,0,0);
	/* Anm			*/ 
	if(!isIdle())
		m_animtime=m_clock.getTimeMilliseconds()/1000.f;
	/* Ray cast		*/ 
	if(m_raycast)
	{		
		/* Prepare rays	*/ 
		const int		res=64;
		const btScalar	fres=res-1;
		const btScalar	size=8;
		const btScalar	dist=10;
		btTransform		trs;
		trs.setOrigin(ps);
		btScalar rayLength = 1000.f;

		const btScalar	angle=m_animtime*0.2;
		trs.setRotation(btQuaternion(angle,SIMD_PI/4,0));
		btVector3	dir=trs.getBasis()*btVector3(0,-1,0);
		trs.setOrigin(ps-dir*dist);
		btAlignedObjectArray<btVector3>	origins;
		btAlignedObjectArray<btScalar>	fractions;
		origins.resize(res*res);
		fractions.resize(res*res,1.f);
		for(int y=0;y<res;++y)
		{
			for(int x=0;x<res;++x)
			{
				const int	idx=y*res+x;
				origins[idx]=trs*btVector3(-size+size*2*x/fres,dist,-size+size*2*y/fres);
			}
		}
		/* Cast rays	*/ 		
		{
			m_clock.reset();
			btVector3*		org=&origins[0];
			btScalar*				fraction=&fractions[0];
			btSoftBody**			psbs=&sbs[0];
			btSoftBody::sRayCast	results;
			for(int i=0,ni=origins.size(),nb=sbs.size();i<ni;++i)
			{
				for(int ib=0;ib<nb;++ib)
				{
					btVector3 rayFrom = *org;
					btVector3 rayTo = rayFrom+dir*rayLength;
					if(psbs[ib]->rayTest(rayFrom,rayTo,results))
					{
						*fraction=results.fraction;
					}
				}
				++org;++fraction;
			}
			long	ms=btMax<long>(m_clock.getTimeMilliseconds(),1);
			long	rayperseconds=(1000*(origins.size()*sbs.size()))/ms;
			printf("%d ms (%d rays/s)\r\n",ms,rayperseconds);
		}
		/* Draw rays	*/ 
		const btVector3	c[]={	origins[0],
			origins[res-1],
			origins[res*(res-1)],
			origins[res*(res-1)+res-1]};
		idraw->drawLine(c[0],c[1],btVector3(0,0,0));
		idraw->drawLine(c[1],c[3],btVector3(0,0,0));
		idraw->drawLine(c[3],c[2],btVector3(0,0,0));
		idraw->drawLine(c[2],c[0],btVector3(0,0,0));
		for(int i=0,ni=origins.size();i<ni;++i)
		{
			const btScalar		fraction=fractions[i];
			const btVector3&	org=origins[i];
			if(fraction<1.f)
			{
				idraw->drawLine(org,org+dir*rayLength*fraction,btVector3(1,0,0));
			}
			else
			{
				idraw->drawLine(org,org-dir*rayLength*0.1,btVector3(0,0,0));
			}
		}
#undef RES
	}
	/* Water level	*/ 
	static const btVector3	axis[]={btVector3(1,0,0),
		btVector3(0,1,0),
		btVector3(0,0,1)};
	if(m_softBodyWorldInfo.water_density>0)
	{
		const btVector3	c=	btVector3((btScalar)0.25,(btScalar)0.25,1);
		const btScalar	a=	(btScalar)0.5;
		const btVector3	n=	m_softBodyWorldInfo.water_normal;
		const btVector3	o=	-n*m_softBodyWorldInfo.water_offset;
		const btVector3	x=	cross(n,axis[n.minAxis()]).normalized();
		const btVector3	y=	cross(x,n).normalized();
		const btScalar	s=	25;
		idraw->drawTriangle(o-x*s-y*s,o+x*s-y*s,o+x*s+y*s,c,a);
		idraw->drawTriangle(o-x*s-y*s,o+x*s+y*s,o-x*s+y*s,c,a);
	}
	//
	DemoApplication::renderme();

}

void	SoftDemo::setDrawClusters(bool drawClusters)
{
	if (drawClusters)
	{
		getSoftDynamicsWorld()->setDrawFlags(getSoftDynamicsWorld()->getDrawFlags()|fDrawFlags::Clusters);
	} else
	{
		getSoftDynamicsWorld()->setDrawFlags(getSoftDynamicsWorld()->getDrawFlags()& (~fDrawFlags::Clusters));
	}
}



void	SoftDemo::keyboardCallback(unsigned char key, int x, int y)
{
	switch(key)
	{
	case	'n':	motorcontrol.maxtorque=10;motorcontrol.goal+=1;break;
	case	'm':	motorcontrol.maxtorque=10;motorcontrol.goal-=1;break;
	case	'l':	steercontrol_f.angle+=0.1;steercontrol_r.angle+=0.1;break;
	case	'k':	steercontrol_f.angle-=0.1;steercontrol_r.angle-=0.1;break;
	case	']':	++current_demo;clientResetScene();break;
	case	'[':	--current_demo;clientResetScene();break;
	case	',':	m_raycast=!m_raycast;break;
	case	';':	m_autocam=!m_autocam;break;
	case	'c':	getSoftDynamicsWorld()->setDrawFlags(getSoftDynamicsWorld()->getDrawFlags()^fDrawFlags::Clusters);break;
	case	'`':
		{
			btSoftBodyArray&	sbs=getSoftDynamicsWorld()->getSoftBodyArray();
			for(int ib=0;ib<sbs.size();++ib)
			{
				btSoftBody*	psb=sbs[ib];
				psb->staticSolve(128);
			}
		}
		break;
	default:		DemoApplication::keyboardCallback(key,x,y);
	}
}

//
void	SoftDemo::mouseMotionFunc(int x,int y)
{
	if(m_node&&(m_results.fraction<1.f))
	{
		if(!m_drag)
		{
#define SQ(_x_) (_x_)*(_x_)
			if((SQ(x-m_lastmousepos[0])+SQ(y-m_lastmousepos[1]))>6)
			{
				m_drag=true;
			}
#undef SQ
		}
		if(m_drag)
		{
			m_lastmousepos[0]	=	x;
			m_lastmousepos[1]	=	y;		
		}
	}
	else
	{
		DemoApplication::mouseMotionFunc(x,y);
	}
}

//
void	SoftDemo::mouseFunc(int button, int state, int x, int y)
{
	if(button==0)
	{
		switch(state)
		{
		case	0:
			{
				m_results.fraction=1.f;
				DemoApplication::mouseFunc(button,state,x,y);
				if(!m_pickConstraint)
				{
					const btVector3			rayFrom=m_cameraPosition;
					const btVector3			rayTo=getRayTo(x,y);
					const btVector3			rayDir=(rayTo-rayFrom).normalized();
					btSoftBodyArray&		sbs=getSoftDynamicsWorld()->getSoftBodyArray();
					for(int ib=0;ib<sbs.size();++ib)
					{
						btSoftBody*				psb=sbs[ib];
						btSoftBody::sRayCast	res;
						if(psb->rayTest(rayFrom,rayTo,res))
						{
							m_results=res;
						}
					}
					if(m_results.fraction<1.f)
					{				
						m_impact			=	rayFrom+(rayTo-rayFrom)*m_results.fraction;
						m_drag				=	false;
						m_lastmousepos[0]	=	x;
						m_lastmousepos[1]	=	y;
						m_node				=	0;
						switch(m_results.feature)
						{
						case	btSoftBody::eFeature::Face:
							{
								btSoftBody::Face&	f=m_results.body->m_faces[m_results.index];
								m_node=f.m_n[0];
								for(int i=1;i<3;++i)
								{
									if(	(m_node->m_x-m_impact).length2()>
										(f.m_n[i]->m_x-m_impact).length2())
									{
										m_node=f.m_n[i];
									}
								}
							}
							break;
						}
						if(m_node) m_goal=m_node->m_x;
						return;
					}
				}
			}
			break;
		case	1:
			if((!m_drag)&&m_cutting&&(m_results.fraction<1.f))
			{
				ImplicitSphere	isphere(m_impact,1);
				printf("Mass before: %f\r\n",m_results.body->getTotalMass());
				m_results.body->refine(&isphere,0.0001,true);
				printf("Mass after: %f\r\n",m_results.body->getTotalMass());
			}
			m_results.fraction=1.f;
			m_drag=false;
			DemoApplication::mouseFunc(button,state,x,y);
			break;
		}
	}
	else
	{
		DemoApplication::mouseFunc(button,state,x,y);
	}
}


void	SoftDemo::initPhysics()
{
	///create concave ground mesh

	//reset and disable motorcontrol at the start
	motorcontrol.goal = 0;
	motorcontrol.maxtorque = 0;
	m_azi = 0;

	btCollisionShape* groundShape = 0;
	bool useConcaveMesh = false;//not ready yet true;

	if (useConcaveMesh)
	{
		int i;
		int j;

		const int NUM_VERTS_X = 30;
		const int NUM_VERTS_Y = 30;
		const int totalVerts = NUM_VERTS_X*NUM_VERTS_Y;
		const int totalTriangles = 2*(NUM_VERTS_X-1)*(NUM_VERTS_Y-1);

		gGroundVertices = new btVector3[totalVerts];
		gGroundIndices = new int[totalTriangles*3];

		btScalar offset(-50);

		for ( i=0;i<NUM_VERTS_X;i++)
		{
			for (j=0;j<NUM_VERTS_Y;j++)
			{
				gGroundVertices[i+j*NUM_VERTS_X].setValue((i-NUM_VERTS_X*0.5f)*TRIANGLE_SIZE,
					//0.f,
					waveheight*sinf((float)i)*cosf((float)j+offset),
					(j-NUM_VERTS_Y*0.5f)*TRIANGLE_SIZE);
			}
		}

		int vertStride = sizeof(btVector3);
		int indexStride = 3*sizeof(int);

		int index=0;
		for ( i=0;i<NUM_VERTS_X-1;i++)
		{
			for (int j=0;j<NUM_VERTS_Y-1;j++)
			{
				gGroundIndices[index++] = j*NUM_VERTS_X+i;
				gGroundIndices[index++] = j*NUM_VERTS_X+i+1;
				gGroundIndices[index++] = (j+1)*NUM_VERTS_X+i+1;

				gGroundIndices[index++] = j*NUM_VERTS_X+i;
				gGroundIndices[index++] = (j+1)*NUM_VERTS_X+i+1;
				gGroundIndices[index++] = (j+1)*NUM_VERTS_X+i;
			}
		}

		btTriangleIndexVertexArray* indexVertexArrays = new btTriangleIndexVertexArray(totalTriangles,
			gGroundIndices,
			indexStride,
			totalVerts,(btScalar*) &gGroundVertices[0].x(),vertStride);

		bool useQuantizedAabbCompression = true;

		groundShape = new btBvhTriangleMeshShape(indexVertexArrays,useQuantizedAabbCompression);
	} else
	{
		groundShape = new btBoxShape (btVector3(200,CUBE_HALF_EXTENTS,200));
	}


	m_collisionShapes.push_back(groundShape);

	btCompoundShape* cylinderCompound = new btCompoundShape;
	btCollisionShape* cylinderShape = new btCylinderShape (btVector3(CUBE_HALF_EXTENTS,CUBE_HALF_EXTENTS,CUBE_HALF_EXTENTS));
	btTransform localTransform;
	localTransform.setIdentity();
	cylinderCompound->addChildShape(localTransform,cylinderShape);
	btQuaternion orn(btVector3(0,1,0),SIMD_PI);
	localTransform.setRotation(orn);
	cylinderCompound->addChildShape(localTransform,cylinderShape);

	m_collisionShapes.push_back(cylinderCompound);


	m_dispatcher=0;

	///register some softbody collision algorithms on top of the default btDefaultCollisionConfiguration
	m_collisionConfiguration = new btSoftBodyRigidBodyCollisionConfiguration();


	m_dispatcher = new	btCollisionDispatcher(m_collisionConfiguration);
	m_softBodyWorldInfo.m_dispatcher = m_dispatcher;

	////////////////////////////
	///Register softbody versus softbody collision algorithm


	///Register softbody versus rigidbody collision algorithm


	////////////////////////////

	btVector3 worldAabbMin(-1000,-1000,-1000);
	btVector3 worldAabbMax(1000,1000,1000);

	m_broadphase = new btAxisSweep3(worldAabbMin,worldAabbMax,maxProxies);

	m_softBodyWorldInfo.m_broadphase = m_broadphase;

	btSequentialImpulseConstraintSolver* solver = new btSequentialImpulseConstraintSolver();

	m_solver = solver;

	btDiscreteDynamicsWorld* world = new btSoftRigidDynamicsWorld(m_dispatcher,m_broadphase,m_solver,m_collisionConfiguration);
	m_dynamicsWorld = world;


	m_dynamicsWorld->getDispatchInfo().m_enableSPU = true;
	m_dynamicsWorld->setGravity(btVector3(0,-10,0));
	m_softBodyWorldInfo.m_gravity.setValue(0,-10,0);





	btTransform tr;
	tr.setIdentity();
	tr.setOrigin(btVector3(0,-12,0));


#define USE_COLLISION_OBJECT 1
#ifdef USE_COLLISION_OBJECT
	btCollisionObject* newOb = new btCollisionObject();
	newOb->setWorldTransform(tr);
	newOb->setInterpolationWorldTransform( tr);
	newOb->setCollisionShape(m_collisionShapes[0]);
	m_dynamicsWorld->addCollisionObject(newOb);
#else
	this->localCreateRigidBody(0,tr,m_collisionShapes[0]);
#endif //USE_COLLISION_OBJECT

	//	clientResetScene();

	m_softBodyWorldInfo.m_sparsesdf.Initialize();
	clientResetScene();
}






void	SoftDemo::exitPhysics()
{

	//cleanup in the reverse order of creation/initialization

	//remove the rigidbodies from the dynamics world and delete them
	int i;
	for (i=m_dynamicsWorld->getNumCollisionObjects()-1; i>=0 ;i--)
	{
		btCollisionObject* obj = m_dynamicsWorld->getCollisionObjectArray()[i];
		btRigidBody* body = btRigidBody::upcast(obj);
		if (body && body->getMotionState())
		{
			delete body->getMotionState();
		}
		m_dynamicsWorld->removeCollisionObject( obj );
		delete obj;
	}

	//delete collision shapes
	for (int j=0;j<m_collisionShapes.size();j++)
	{
		btCollisionShape* shape = m_collisionShapes[j];
		m_collisionShapes[j] = 0;
		delete shape;
	}

	//delete dynamics world
	delete m_dynamicsWorld;

	//delete solver
	delete m_solver;

	//delete broadphase
	delete m_broadphase;

	//delete dispatcher
	delete m_dispatcher;



	delete m_collisionConfiguration;


}






