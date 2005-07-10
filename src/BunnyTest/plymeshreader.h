/*=====================================================================
plymeshreader.h
---------------
File created by ClassTemplate on Wed Nov 10 05:23:44 2004
Code By Nicholas Chapman.
=====================================================================*/
#ifndef __PLYMESHREADER_H_666_
#define __PLYMESHREADER_H_666_



#include <string>
#include <vector>

class PlyMeshVert
{
public:
	float pos[3];
};

class PlyMeshTri
{
public:
	int vert_indices[3];
};

class PlyMesh
{
public:
	std::vector<PlyMeshVert> verts;
	std::vector<PlyMeshTri> tris;
};



class PlyMeshReaderExcep
{
public:
	PlyMeshReaderExcep(const std::string& s_) : s(s_) {}
	~PlyMeshReaderExcep(){}

	const std::string& what() const { return s; }
private:
	std::string s;
};

/*=====================================================================
PlyMeshReader
-------------
v1.0
=====================================================================*/
class PlyMeshReader
{
public:
	/*=====================================================================
	PlyMeshReader
	-------------
	
	=====================================================================*/
	PlyMeshReader();

	~PlyMeshReader();


	//throws PlyMeshReaderExcep
	void readFile(const std::string& pathname, PlyMesh& mesh_out);



};



#endif //__PLYMESHREADER_H_666_




