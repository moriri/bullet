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

#include "StridingMeshInterface.h"
#include <vector>

///IndexedMesh indexes into existing vertex and index arrays, in a similar way OpenGL glDrawElements
///instead of the number of indices, we pass the number of triangles
///todo: explain with pictures
struct	IndexedMesh
	{
		int			m_numTriangles;
		int*		m_triangleIndexBase;
		int			m_triangleIndexStride;
		int			m_numVertices;
		float*		m_vertexBase;
		int			m_vertexStride;
	};

///TriangleIndexVertexArray allows to use multiple meshes, by indexing into existing triangle/index arrays.
///Additional meshes can be added using AddIndexedMesh
///No duplcate is made of the vertex/index data, it only indexes into external vertex/index arrays.
///So keep those arrays around during the lifetime of this TriangleIndexVertexArray.
class TriangleIndexVertexArray : public StridingMeshInterface
{
	std::vector<IndexedMesh>	m_indexedMeshes;

		
public:

	

	TriangleIndexVertexArray()
	{
	}

	//just to be backwards compatible
	TriangleIndexVertexArray(int numTriangleIndices,int* triangleIndexBase,int triangleIndexStride,int numVertices,float* vertexBase,int vertexStride);
	
	void	AddIndexedMesh(const IndexedMesh& mesh)
	{
		m_indexedMeshes.push_back(mesh);
	}
	
	
	virtual void	getLockedVertexIndexBase(unsigned char **vertexbase, int& numverts,PHY_ScalarType& type, int& vertexStride,unsigned char **indexbase,int & indexstride,int& numfaces,PHY_ScalarType& indicestype,int subpart=0);

	virtual void	getLockedReadOnlyVertexIndexBase(const unsigned char **vertexbase, int& numverts,PHY_ScalarType& type, int& vertexStride,const unsigned char **indexbase,int & indexstride,int& numfaces,PHY_ScalarType& indicestype,int subpart=0) const;

	/// unLockVertexBase finishes the access to a subpart of the triangle mesh
	/// make a call to unLockVertexBase when the read and write access (using getLockedVertexIndexBase) is finished
	virtual void	unLockVertexBase(int subpart) {}

	virtual void	unLockReadOnlyVertexBase(int subpart) const {}

	/// getNumSubParts returns the number of seperate subparts
	/// each subpart has a continuous array of vertices and indices
	virtual int		getNumSubParts() const { 
		return (int)m_indexedMeshes.size();
	}
	
	virtual void	preallocateVertices(int numverts){}
	virtual void	preallocateIndices(int numindices){}

};

