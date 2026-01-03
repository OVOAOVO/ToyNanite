#pragma once
#include "matrix4.h"
class quaternion{
public:
	float w, x, y, z;//i,j,k
	quaternion(float inAngleX, float inAngleY, float inAngleZ);//eular -> quaternion
	quaternion(float inW, float inX, float inY,float inZ);//eular -> quaternion
	void operator=(const quaternion& inR);
	quaternion operator*(const quaternion& inR)const;
	matrix3 toMatrix3()const;
};

