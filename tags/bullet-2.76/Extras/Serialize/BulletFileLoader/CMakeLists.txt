INCLUDE_DIRECTORIES( 
	${BULLET_PHYSICS_SOURCE_DIR}/src  
)

ADD_LIBRARY(
BulletFileLoader 
bChunk.cpp 
bChunk.h 
bCommon.h 
bDefines.h 
bDNA.cpp 
bDNA.h 
bFile.cpp 
bFile.h
btBulletFile.cpp 
btBulletFile.h 
)
