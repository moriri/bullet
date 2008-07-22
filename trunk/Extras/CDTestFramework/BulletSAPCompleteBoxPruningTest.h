/*
BulletSAPCompleteBoxPruningTest, Copyright (c) 2008 Erwin Coumans
Part of:
CDTestFramework http://codercorner.com
Copyright (c) 2007-2008 Pierre Terdiman,  pierre@codercorner.com

This software is provided 'as-is', without any express or implied warranty.
In no event will the authors be held liable for any damages arising from the use of this software.
Permission is granted to anyone to use this software for any purpose, 
including commercial applications, and to alter it and redistribute it freely, 
subject to the following restrictions:

1. The origin of this software must not be misrepresented; you must not claim that you wrote the original software. If you use this software in a product, an acknowledgment in the product documentation would be appreciated but is not required.
2. Altered source versions must be plainly marked as such, and must not be misrepresented as being the original software.
3. This notice may not be removed or altered from any source distribution.
*/

#ifndef BULLET_SAP_COMPLETEBOXPRUNING_H
#define BULLET_SAP_COMPLETEBOXPRUNING_H

#include "LinearMath/btAlignedObjectArray.h"

#include "CollisionTest.h"
#include "Profiling.h"

	class BulletSAPCompleteBoxPruningTest : public CollisionTest
	{
		public:
								BulletSAPCompleteBoxPruningTest(int numBoxes,int method);
		virtual					~BulletSAPCompleteBoxPruningTest();

		virtual	void			Init();
		virtual	void			Release();
		virtual	void			PerformTest();
		void					RenderAll();
		void					Render();
		virtual	void			Select();
		virtual	void			Deselect();
		virtual	void			KeyboardCallback(unsigned char key, int x, int y);
		virtual	void			MouseCallback(int button, int state, int x, int y);
		virtual	void			MotionCallback(int x, int y);

				TwBar*			mBar;		//!< AntTweakBar
				Profiler		mProfiler;

				class btBroadphaseInterface*	m_broadphase;
				bool							m_isdbvt;
				btAlignedObjectArray<struct  btBroadphaseProxy*> m_proxies;

				udword			mNbBoxes;
				AABB*			mBoxes;
				bool*			mFlags;
				const char*		methodname;
				const AABB**	mBoxPtrs;
				Pairs			mPairs;
				float*			mBoxTime;
				float			mSpeed;
				float			mAmplitude;
				bool			m_firstTime;

		private:
				bool			UpdateBoxes(int numBoxes);
	};

#endif	// BULLET_SAP_COMPLETEBOXPRUNING_H