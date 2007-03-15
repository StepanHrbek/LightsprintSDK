/*
    Copyright (C) 2005-2007 Feeling Software Inc.
    MIT License: http://www.opensource.org/licenses/mit-license.php
*/

/**
	@file FMMatrix44.h
	The file containing the class and global functions for 4x4 matrices.
*/

#ifndef _FM_MATRIX44_H_
#define _FM_MATRIX44_H_

/**
	A 4x4 Row Major matrix: use to represent 3D transformations.

	@ingroup FMath
*/
class FCOLLADA_EXPORT FMMatrix44
{
public:
	float m[4][4];	/**< The matrix elements stored in a 2D array. */

	/**
	 * Creates a FMMatrix44 from the \c float array.
	 *
	 * The float array stores the elements in the following order: m[0][0], 
	 * m[1][0], m[2][0], m[3][0], m[0][1], m[1][1], m[2][1], m[3][1], m[0][2],
	 * m[1][2], m[2][2], m[3][2], m[0][3], m[1][3], m[2][3], m[3][3].
	 *
	 * @param _m The \c float array to create the matrix from.
	 */
	FMMatrix44(const float* _m);
	FMMatrix44(const double* _m); /**< See above. */

	/**
	 * Creates an empty FMMatrix44.
	 *
	 * The default values are non deterministic.
	 */
	#ifndef _DEBUG
	FMMatrix44() {}
	#else
	FMMatrix44() { memset(m, 55, 16 * sizeof(float)); }
	#endif 

	/** Get this FMMatrix44 as an array of \c floats.
		The array contains the elements in the following order: m[0][0], 
		m[0][1], m[0][2], m[0][3], m[1][0], m[1][1], m[1][2], m[1][3], m[2][0],
		m[2][1], m[2][2], m[0][3], m[3][0], m[3][1], m[3][2], m[3][3].
		@return The \c float array. */
	inline operator float*() { return &m[0][0]; }
	inline operator const float*() const { return &m[0][0]; } /**< See above. */

	/** Get a specified row of FMMatrix44 as an array of \c floats.
		@param a The row index, starting at 0, of the row to get.
		@return The \c float array of the elements in the specified row. */
	template <class Integer> float* operator[](Integer a) { return m[a]; }
	template <class Integer> const float* operator[](Integer a) const { return m[a]; } /**< See above. */

	/** Assign this FMMatrix44's elements to be the same as that of the given 
		matrix.
		@param copy The FMMatrix to copy elements from.
		@return This FMMatrix. */
	FMMatrix44& operator=(const FMMatrix44& copy);

	/** Sets a FMMatrix44 from the \c float array.
		The float array stores the elements in the following order: m[0][0],
		m[1][0], m[2][0], m[3][0], m[0][1], m[1][1], m[2][1], m[3][1], m[0][2], 
		m[1][2], m[2][2], m[3][2], m[0][3], m[1][3], m[2][3], m[3][3].
		@param _m The \c float array to create the matrix from. */
	void Set(const float* _m);
	void Set(const double* _m);	/** See above. */

	/** Gets the transposed of this FMMatrix44.
		@return The transposed of this FMMatrix. */
	FMMatrix44 Transposed() const;

	/** Gets the inverse of this matrix.
		@return The inverse of this matrix. */
	FMMatrix44 Inverted() const;

	/** Gets the determinant of this matrix.
		@return The determinant of this matrix. */
	float Determinant() const;
	
	/** Decompose this matrix into it's scale, rotation, and translation
		components; it also tells whether it is inverted.
		@param Scale The FMVector to place the scale components to.
		@param Rotation The FMVector to place the rotation components to.
		@param Translation The FMVector to place the translation components to.
		@param inverted The value corresponding to if it was inverted (-1.0f or 1.0f) */
	void Decompose(FMVector3& Scale, FMVector3& Rotation, FMVector3& Translation, float& inverted) const;

	/** Transforms the given point by this matrix.
		@param coordinate The point to transform.
		@return The vector representation of the transformed point. */
	FMVector3 TransformCoordinate(const FMVector3& coordinate) const;
	FMVector4 TransformCoordinate(const FMVector4& coordinate) const; /**< See above. */

	/** Transforms the given vector by this FMMatrix44.
		@param v The vector to transform.
		@return The FMVector3 representation of the transformed vector. */
	FMVector3 TransformVector(const FMVector3& v) const;

	/** Gets the translation component of this matrix.
		@return The FMVector3 representation of the translation. */
	FMVector3 GetTranslation() const;

	/** Sets the translation component of this matrix.
		@return The new translation component. */
	void SetTranslation(const FMVector3& translation);

public:
	static FMMatrix44 Identity;	/**< The identity matrix. */

	/** Gets the FMMatrix44 representation of a 3D translation.
		The translation in the x, y and z directions correspond to the \a x, 
		\a y, and \a z components of the FMVector3.
		@param translation The FMVector3 to get the translation components from.
		@return The translation FMMatrix44.  */
	static FMMatrix44 TranslationMatrix(const FMVector3& translation);

	/** Gets the FMMatrix44 representation of a 3D rotation about a given axis
		by an angle.
		@param axis The axis of rotation.
		@param angle The angle of rotation in radians.
		@return The rotation FMMatrix44. */
	static FMMatrix44 AxisRotationMatrix(const FMVector3& axis, float angle);

	/** Gets the matrix representation of a 3D euler rotation.
		The angles are considered in the order: x, y, z,
		which represents heading, banking and roll.
		@param rotation The euler rotation angles.
		@return The rotation matrix. */
	static FMMatrix44 EulerRotationMatrix(const FMVector3& rotation);

	/** Gets the FMMatrix44 representation of a 3D axis-bound scale.
		@param scale The scaling components.
		@return The scale transform. */
	static FMMatrix44 ScaleMatrix(const FMVector3& scale);
};

/**
 * Matrix multiplication with two FMMatrix44.
 *
 * @param m1 The first matrix.
 * @param m2 The second matrix.
 * @return The FMMatrix44 representation of the resulting matrix.
 */
FMMatrix44 FCOLLADA_EXPORT operator*(const FMMatrix44& m1, const FMMatrix44& m2);

/**
 * Matrix multiplication with FMMatrix44 and FMVector4.
 *
 * @param m The matrix.
 * @param v The vector.
 * @return The FMVector4 representation of the resulting vector.
 */
FMVector4 FCOLLADA_EXPORT operator*(const FMMatrix44& m, const FMVector4& v);

/**
 * Scalar multiplication with float and FMMatrix44.
 *
 * @param a The scalar.
 * @param m The matrix.
 * @return The FMMatrix44 representation of the resulting matrix.
 */
FMMatrix44 FCOLLADA_EXPORT operator*(float a, const FMMatrix44& m);

/**
	Matrix equality comparison function.
	@param m1 The first matrix.
	@param m2 The second matrix.
	@return Whether the given matrices are equal.
*/
bool FCOLLADA_EXPORT IsEquivalent(const FMMatrix44& m1, const FMMatrix44& m2);

/** Left-multiplies a given matrix, in-place.
	@param m1 The in-place multiplied matrix.
	@param m2 The matrix transformation.
	@return The in-place multiplied matrix. */
inline FMMatrix44& operator*=(FMMatrix44& m1, const FMMatrix44& m2) { return m1 = m1 * m2; }

/**
	Matrix equality operator override.
	@param m1 The first matrix.
	@param m2 The second matrix.
	@return Whether the given matrices are equal.
*/
inline bool operator==(const FMMatrix44& m1, const FMMatrix44& m2) { return IsEquivalent(m1, m2); }

/** A dynamically-sized array of 4x4 matrices. */
typedef fm::vector<FMMatrix44> FMMatrix44List;

#endif // _FM_MATRIX44_H_
