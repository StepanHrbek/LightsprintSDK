#ifdef _MSC_VER // this is personal choice: I use Boost only under MSVC
//#define BOOST // support for load(filename) and save(filename)
#endif

#include "RRMeshCopy.h"

//////////////////////////////////////////////////////////////////////////////
//
// save/load

#ifdef BOOST

#ifdef _MSC_VER
#pragma warning(disable:4267)
#endif

#include <boost/archive/binary_oarchive.hpp>
#include <boost/archive/binary_iarchive.hpp>
#include <boost/serialization/base_object.hpp>
#include <boost/serialization/utility.hpp>
#include <boost/serialization/vector.hpp>
#include <fstream>
//#include <boost/iostreams/filter/zlib.hpp>
//#include <boost/iostreams/filter/bzip2.hpp>

namespace boost {
namespace serialization {

template<class Archive>
void serialize(Archive & ar, rr::RRVec3 & d, const unsigned int version)
{
	ar & d.x;
	ar & d.y;
	ar & d.z;
}

template<class Archive>
void serialize(Archive & ar, rr::RRMesh::Triangle & d, const unsigned int version)
{
	//ar & d.m; makes archive bigger and throws exception on loading
	ar & d.m[0];
	ar & d.m[1];
	ar & d.m[2];
}

template<class Archive>
void serialize(Archive & ar, rr::RRMeshCopy::PostImportTriangle & d, const unsigned int version)
{
	ar & d.preImportTriangle;
	ar & d.postImportTriangleVertices;
	ar & d.preImportTriangleVertices;
}

template<class Archive>
void serialize(Archive & ar, rr::RRMeshCopy & d, const unsigned int version)
{
	ar & d.postImportVertices;
	ar & d.postImportTriangles;
	ar & d.pre2postImportTriangles;
}

} // namespace serialization
} // namespace boost

namespace rr
{

bool RRMeshCopy::save(const char* filename) const
{
	try
	{
		std::ofstream ofs(filename);
		boost::archive::binary_oarchive oa(ofs);
		oa << *this;
	}
	catch (...)
	{
		return false;
	}
	return true;
}

bool RRMeshCopy::load(const char* filename)
{
	try
	{
		std::ifstream ifs(filename, std::ios::binary);
		boost::archive::binary_iarchive ia(ifs);
		ia >> *this;
	}
	catch (...)
	{
		return false;
	}
	return true;
}

#else // !BOOST

namespace rr
{

bool RRMeshCopy::save(const char* filename) const
{
	return false;
}

bool RRMeshCopy::load(const char* filename)
{
	return false;
}

#endif // !BOOST


//////////////////////////////////////////////////////////////////////////////
//
// all RRMeshCopy users
// concentrated here so that nobody else includes RRMeshCopy.h
// only this file has c++ exceptions on, no one else needs them


RRMesh* RRMesh::createCopy()
{
	RRMeshCopy* importer = new RRMeshCopy();
	if(importer->load(this)) return importer;
	delete importer;
	return NULL;
}

} // namespace rr
