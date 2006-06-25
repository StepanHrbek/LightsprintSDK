/*=====================================================================
plymeshreader.cpp
-----------------
File created by ClassTemplate on Wed Nov 10 05:23:44 2004
Code By Nicholas Chapman.
=====================================================================*/
#include "plymeshreader.h"

//grab rply src from: http://www.cs.princeton.edu/~diego/professional/rply/

#include "rply.h"
#include <assert.h>

//some code taken from http://www.cs.princeton.edu/~diego/professional/rply/



static int vertex_callback(p_ply_argument argument) 
{

	//------------------------------------------------------------------------
	//get pointer to current mesh, current component of vertex pos.
	//------------------------------------------------------------------------
	long current_cmpnt = 0;
	void* pointer_usrdata = NULL;
	ply_get_argument_user_data(argument, &pointer_usrdata, &current_cmpnt);

	PlyMesh* current_mesh = static_cast<PlyMesh*>(pointer_usrdata);
	assert(current_mesh);


	//get vert index
	long vertindex;
	ply_get_argument_element(argument, NULL, &vertindex);

	current_mesh->verts[vertindex].pos[current_cmpnt] = (float)ply_get_argument_value(argument);


    return 1;
}

static int face_callback(p_ply_argument argument) 
{
	//------------------------------------------------------------------------
	//get pointer to current mesh
	//------------------------------------------------------------------------
	void* pointer_usrdata = NULL;
	ply_get_argument_user_data(argument, &pointer_usrdata, NULL);

	PlyMesh* current_mesh = static_cast<PlyMesh*>(pointer_usrdata);
	assert(current_mesh);


	//get current tri index
	long tri_index;
	ply_get_argument_element(argument, NULL, &tri_index);


	long length, value_index;
    ply_get_argument_property(argument, NULL, &length, &value_index);

	if(value_index >= 0 && value_index <= 2)
	{
		current_mesh->tris[tri_index].vert_indices[value_index] = (int)ply_get_argument_value(argument);
	}

    return 1;
}



PlyMeshReader::PlyMeshReader()
{
	
}


PlyMeshReader::~PlyMeshReader()
{
	
}



	//throws PlyMeshReaderExcep
void PlyMeshReader::readFile(const std::string& pathname, 
							 PlyMesh& mesh_out)
{
    p_ply ply = ply_open(pathname.c_str(), NULL);

    if(!ply) 
		throw new PlyMeshReaderExcep("could not open file '" + pathname + "' for reading.");
    
	if(!ply_read_header(ply))
		throw new PlyMeshReaderExcep("could not read header.");
		
    const long nvertices = ply_set_read_cb(ply, "vertex", "x", vertex_callback, &mesh_out, 0);

	mesh_out.verts.resize(nvertices);

    ply_set_read_cb(ply, "vertex", "y", vertex_callback, &mesh_out, 1);
    ply_set_read_cb(ply, "vertex", "z", vertex_callback, &mesh_out, 2);

    const long ntriangles = ply_set_read_cb(ply, "face", "vertex_indices", face_callback, &mesh_out, 0);

	mesh_out.tris.resize(ntriangles);
	
    if(!ply_read(ply)) 
		throw new PlyMeshReaderExcep("read of body failed.");

    ply_close(ply);
}



