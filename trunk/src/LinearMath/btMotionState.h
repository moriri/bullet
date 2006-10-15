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

#ifndef BT_MOTIONSTATE_H
#define BT_MOTIONSTATE_H

#include "LinearMath/btVector3.h"
#include "LinearMath/btQuaternion.h"


///btMotionState allows the dynamics world to synchronize the updated world transforms with graphics
///For optimizations, potentially only moving objects get synchronized (using setWorldPosition/setWorldOrientation)
class	btMotionState
{
	public:
		
		virtual ~btMotionState()
		{
			
		}
		
		virtual void	getWorldPosition(btVector3& worldPos )=0;
		virtual void	getWorldScaling(btVector3& scaling)=0;
		virtual void	getWorldOrientation(btQuaternion& orn)=0;
		
		virtual void	setWorldPosition(const btVector3& worldPos)=0;
		virtual	void	setWorldOrientation(const btQuaternion& orn)=0;

		virtual	void	calculateWorldTransformations()=0;

};

#endif //BT_MOTIONSTATE_H
