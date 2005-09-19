
#include "miniball_config.h"

#ifdef MINIBALL_NO_STD_NAMESPACE
#include <list.h>
#else
#include <list>
#endif

#include "wrapped_array.h"

namespace miniball 
{

template <int d> class Miniball;
template <int d> class Basis;

// Miniball
// --------

template <int d>
class Miniball {
public:
	// types
	typedef Wrapped_array<d>                            Point;
	typedef typename std::list<Point>::iterator         It;
	typedef typename std::list<Point>::const_iterator   Cit;

private:
	// data members
	std::list<Point> L;         // STL list keeping the points
	Basis<d>    B;              // basis keeping the current ball
	It          support_end;    // past-the-end iterator of support set

	// private methods
	void        mtf_mb (It k);
	void        pivot_mb (It k);
	void        move_to_front (It j);
	double      max_excess (It t, It i, It& pivot) const;
	double      abs (double r) const {return (r>0)? r: (-r);}
	double      sqr (double r) const {return r*r;}

public:
	// construction
	Miniball() {}
	void        check_in (const Point& p);
	void        build (bool pivoting = true);

	// access
	Point       center() const;
	double      squared_radius () const;
	int         nr_points () const;
	Cit         points_begin () const;
	Cit         points_end () const;
	int         nr_support_points () const;
	Cit         support_points_begin () const;
	Cit         support_points_end () const;

	// checking
	double      accuracy (double& slack) const;
	bool        is_valid (double tolerance = 1e-15) const;
};



// Basis
// -----

template <int d>
class Basis {
private:
	// types
	typedef Wrapped_array<d>            Point;

	// data members
	int                 m, s;   // size and number of support points
	double              q0[d];

	double              z[d+1];
	double              f[d+1];
	double              v[d+1][d];
	double              a[d+1][d];

	double              c[d+1][d];
	double              sqr_r[d+1];

	double*             current_c;      // points to some c[j]
	double              current_sqr_r;

	double              sqr (double r) const {return r*r;}

public:
	Basis();

	// access
	const double*       center() const;
	double              squared_radius() const;
	int                 size() const;
	int                 support_size() const;
	double              excess (const Point& p) const;

	// modification
	void                reset(); // generates empty sphere with m=s=0
	bool                push (const Point& p);
	void                pop ();

	// checking
	double              slack() const;
};




/////////////////////////////////////////////////////////////////////////////


#ifdef MINIBALL_NO_STD_NAMESPACE
#include<assert.h>
#else
#include<cassert>
#endif

// Miniball
// --------

template <int d>
void Miniball<d>::check_in (const Point& p)
{
	L.push_back(p);
}


template <int d>
void Miniball<d>::build (bool pivoting)
{
	B.reset();
	support_end = L.begin();
	if (pivoting)
		pivot_mb (L.end());
	else
		mtf_mb (L.end());
}


template <int d>
void Miniball<d>::mtf_mb (It i)
{
	support_end = L.begin();
	if ((B.size())==d+1) return;
	for (It k=L.begin(); k!=i;) {
		It j=k++;
		if (B.excess(*j) > 0) {
			if (B.push(*j)) {
				mtf_mb (j);
				B.pop();
				move_to_front(j);
			}
		}
	}
}

template <int d>
void Miniball<d>::move_to_front (It j)
{
	if (support_end == j)
		support_end++;
	L.splice (L.begin(), L, j);
}


template <int d>
void Miniball<d>::pivot_mb (It i)
{
	It t = ++L.begin();
	mtf_mb (t);
	double max_e, old_sqr_r;
	do {
		It pivot;
		max_e = max_excess (t, i, pivot);
		if (max_e > 0) {
			t = support_end;
			if (t==pivot) ++t;
			old_sqr_r = B.squared_radius();
			B.push (*pivot);
			mtf_mb (support_end);
			B.pop();
			move_to_front (pivot);
		}
	} while ((max_e > 0) && (B.squared_radius() > old_sqr_r));
}


template <int d>
double Miniball<d>::max_excess (It t, It i, It& pivot) const
{
	const double *c = B.center(), sqr_r = B.squared_radius();
	double e, max_e = 0;
	for (It k=t; k!=i; ++k) {
		const double *p = (*k).begin();
		e = -sqr_r;
		for (int j=0; j<d; ++j)
			e += sqr(p[j]-c[j]);
		if (e > max_e) {
			max_e = e;
			pivot = k;
		}
	}
	return max_e;
}



template <int d>
typename Miniball<d>::Point Miniball<d>::center () const
{
	return Point(B.center());
}

template <int d>
double Miniball<d>::squared_radius () const
{
	return B.squared_radius();
}


template <int d>
int Miniball<d>::nr_points () const
{
	return L.size();
}

template <int d>
typename Miniball<d>::Cit Miniball<d>::points_begin () const
{
	return L.begin();
}

template <int d>
typename Miniball<d>::Cit Miniball<d>::points_end () const
{
	return L.end();
}


template <int d>
int Miniball<d>::nr_support_points () const
{
	return B.support_size();
}

template <int d>
typename Miniball<d>::Cit Miniball<d>::support_points_begin () const
{
	return L.begin();
}

template <int d>
typename Miniball<d>::Cit Miniball<d>::support_points_end () const
{
	return support_end;
}



template <int d>
double Miniball<d>::accuracy (double& slack) const
{
	double e, max_e = 0;
	int n_supp=0;
	Cit i;
	for (i=L.begin(); i!=support_end; ++i,++n_supp)
		if ((e = abs (B.excess (*i))) > max_e)
			max_e = e;

	// you've found a non-numerical problem if the following ever fails
	assert (n_supp == nr_support_points());

	for (i=support_end; i!=L.end(); ++i)
		if ((e = B.excess (*i)) > max_e)
			max_e = e;

	slack = B.slack();
	return (max_e/squared_radius());
}


template <int d>
bool Miniball<d>::is_valid (double tolerance) const
{
	double slack;
	return ( (accuracy (slack) < tolerance) && (slack == 0) );
}



// Basis
// -----

template <int d>
const double* Basis<d>::center () const
{
	return current_c;
}

template <int d>
double Basis<d>::squared_radius() const
{
	return current_sqr_r;
}

template <int d>
int Basis<d>::size() const
{
	return m;
}

template <int d>
int Basis<d>::support_size() const
{
	return s;
}

template <int d>
double Basis<d>::excess (const Point& p) const
{
	double e = -current_sqr_r;
	for (int k=0; k<d; ++k)
		e += sqr(p[k]-current_c[k]);
	return e;
}



template <int d>
void Basis<d>::reset ()
{
	m = s = 0;
	// we misuse c[0] for the center of the empty sphere
	for (int j=0; j<d; ++j)
		c[0][j]=0;
	current_c = c[0];
	current_sqr_r = -1;
}


template <int d>
Basis<d>::Basis ()
{
	reset();
}


template <int d>
void Basis<d>::pop ()
{
	--m;
}


template <int d>
bool Basis<d>::push (const Point& p)
{
	int i, j;
	double eps = 1e-32;
	if (m==0) {
		for (i=0; i<d; ++i)
			q0[i] = p[i];
		for (i=0; i<d; ++i)
			c[0][i] = q0[i];
		sqr_r[0] = 0;
	} else {
		// set v_m to Q_m
		for (i=0; i<d; ++i)
			v[m][i] = p[i]-q0[i];

		// compute the a_{m,i}, i< m
		for (i=1; i<m; ++i) {
			a[m][i] = 0;
			for (j=0; j<d; ++j)
				a[m][i] += v[i][j] * v[m][j];
			a[m][i]*=(2/z[i]);
		}

		// update v_m to Q_m-\bar{Q}_m
		for (i=1; i<m; ++i) {
			for (j=0; j<d; ++j)
				v[m][j] -= a[m][i]*v[i][j];
		}

		// compute z_m
		z[m]=0;
		for (j=0; j<d; ++j)
			z[m] += sqr(v[m][j]);
		z[m]*=2;

		// reject push if z_m too small
		if (z[m]<eps*current_sqr_r) {
			return false;
		}

		// update c, sqr_r
		double e = -sqr_r[m-1];
		for (i=0; i<d; ++i)
			e += sqr(p[i]-c[m-1][i]);
		f[m]=e/z[m];

		for (i=0; i<d; ++i)
			c[m][i] = c[m-1][i]+f[m]*v[m][i];
		sqr_r[m] = sqr_r[m-1] + e*f[m]/2;
	}
	current_c = c[m];
	current_sqr_r = sqr_r[m];
	s = ++m;
	return true;
}



template <int d>
double Basis<d>::slack () const
{
	double l[d+1], min_l=0;
	l[0] = 1;
	for (int i=s-1; i>0; --i) {
		l[i] = f[i];
		for (int k=s-1; k>i; --k)
			l[i]-=a[k][i]*l[k];
		if (l[i] < min_l) min_l = l[i];
		l[0] -= l[i];
	}
	if (l[0] < min_l) min_l = l[0];
	return ( (min_l < 0) ? -min_l : 0);
}

} // namespace
