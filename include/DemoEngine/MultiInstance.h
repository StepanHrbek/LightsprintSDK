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

template <class Class>
class MultiInstance
{
public:
	virtual Class* getInstance(unsigned instance) = 0;
	virtual unsigned getNumInstances() const = 0;
	virtual void setNumInstances(unsigned instances) = 0;
	virtual ~MultiInstance() {}
};


/////////////////////////////////////////////////////////////////////////////
//
// MultiInstanceWithParent

template <class Class>
class MultiInstanceWithParent : public MultiInstance<Class>
{
public:
	MultiInstanceWithParent()
	{
		parent = NULL;
		parentChanged = true;
	}
	virtual void attachTo(const Class* aparent)
	{
		if(aparent != parent)
		{
			parentChanged = true;
			parent = aparent;
		}
	}
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

template <class Class>
class MultiInstanceWithParentAndInstances : public MultiInstanceWithParent<Class>
{
public:
	MultiInstanceWithParentAndInstances()
	{
		numInstances = 1;
		numInstancesChanged = true;
	}
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
	virtual unsigned getNumInstances() const
	{
		return numInstances;
	}
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
