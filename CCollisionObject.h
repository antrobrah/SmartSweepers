#ifndef CCOLLISIONOBJECT_H
#define CCOLLISIONOBJECT_H
//#include "SVector2D.h"
class SVector2D;
class CCollisionObject
{
public:
	enum ObjectType { Mine, SuperMine, Rock };


	CCollisionObject();
	CCollisionObject(ObjectType objectType, SVector2D position);
	~CCollisionObject();

	//--- accessor and mutator functions
	void setPosition(SVector2D position);
	SVector2D getPosition();
	void setType(ObjectType objectType);
	ObjectType getType();	
	void setActive(bool b) {isActive = b;};
	bool getActive() {return isActive;};
		
private:
	
	SVector2D * m_vPosition;
	ObjectType m_ObjectType;
	bool isActive; // represents whether an object as been collided with (false) or not (true)
};

#endif