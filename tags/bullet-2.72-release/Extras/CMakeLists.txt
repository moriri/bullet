if (CMAKE_SIZEOF_VOID_P MATCHES "8")
	SUBDIRS( glui ConvexDecomposition BulletColladaConverter LibXML COLLADA_DOM GIMPACTUtils GIMPACT )
else (CMAKE_SIZEOF_VOID_P MATCHES "8")
	SUBDIRS( glui ConvexDecomposition BulletMultiThreaded BulletColladaConverter LibXML COLLADA_DOM GIMPACTUtils GIMPACT )
endif (CMAKE_SIZEOF_VOID_P MATCHES "8")
