// --------------------------------------------------------------------------
// DemoEngine
// MultiInstance, provides multiple (possibly generated) instances of Class.
// Copyright (C) Stepan Hrbek, Lightsprint, 2005-2007
// --------------------------------------------------------------------------

#ifndef MULTIINSTANCE_H
#define MULTIINSTANCE_H

#include "DemoEngine.h"


/////////////////////////////////////////////////////////////////////////////
//
// MultiInstance

//! Interface of object with variable number of Class instances indexed by 0..n.
template <class Class>
class MultiInstance
{
public:
	//! Returns requested Class instance. To be deleted by caller.
	virtual Class* getInstance(unsigned instance) = 0;
	//! Returns number of Class instances available.
	virtual unsigned getNumInstances() const = 0;
	//! Sets number of Class instances.
	virtual void setNumInstances(unsigned instances) = 0;
	virtual ~MultiInstance() {}
};


/////////////////////////////////////////////////////////////////////////////
//
// MultiInstanceWithParent

//! MultiInstance with additional Class instance called parent.
template <class Class>
class MultiInstanceWithParent : public MultiInstance<Class>
{
public:
	MultiInstanceWithParent()
	{
		parent = NULL;
		parentChanged = true;
	}
	//! Sets parent.
	virtual void attachTo(const Class* aparent)
	{
		if(aparent != parent)
		{
			parentChanged = true;
			parent = aparent;
		}
	}
	//! Returns parent.
	virtual const Class* getParent()
	{
		return parent;
	}
protected:
	const Class* parent;
	bool parentChanged;
};


/////////////////////////////////////////////////////////////////////////////
//
// MultiInstanceWithParentAndInstances

//! MultiInstanceWithParent that internally stores number of instances.
template <class Class>
class MultiInstanceWithParentAndInstances : public MultiInstanceWithParent<Class>
{
public:
	MultiInstanceWithParentAndInstances()
	{
		numInstances = 1;
		numInstancesChanged = true;
	}
	//! Returns requested Class instance. To be deleted by caller.
	virtual Class* getInstance(unsigned instance)
	{
		if(!MultiInstanceWithParent<Class>::parent || instance>numInstances)
			return NULL;
		Class* c = new Class(*MultiInstanceWithParent<Class>::parent);
		if(!c)
			return NULL;
		instanceMakeup(*c,instance);
		return c;
	}
	//! Returns number of Class instances available.
	virtual unsigned getNumInstances() const
	{
		return numInstances;
	}
	//! Sets number of Class instances.
	virtual void setNumInstances(unsigned instances)
	{
		numInstances = instances;
	}
protected:
	virtual void instanceMakeup(Class& c, unsigned instance) = 0;
	unsigned numInstances;
	bool numInstancesChanged;
};

#endif
