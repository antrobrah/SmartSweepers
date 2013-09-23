#include "CMinesweeper.h"

//-----------------------------------constructor-------------------------
//
//-----------------------------------------------------------------------
CMinesweeper::CMinesweeper():
                             m_dRotation(RandFloat()*CParams::dTwoPi),
                             m_dMinesGathered(0),
							 m_dScale(CParams::iSweeperScale),
                             m_iClosestMine(0)
{
	//create a random start position
	m_vPosition = SVector2D((RandFloat() * CParams::WindowWidth), 
					                (RandFloat() * CParams::WindowHeight));
  
}

//-------------------------------------------Reset()--------------------
//
//	Resets the sweepers position, MinesGathered and rotation
//
//----------------------------------------------------------------------
void CMinesweeper::Reset()
{
	//reset the sweepers positions
	m_vPosition = SVector2D((RandFloat() * CParams::WindowWidth), 
					                (RandFloat() * CParams::WindowHeight));
	
	//and the MinesGathered
	m_dMinesGathered = 0;

	//and the rotation
	m_dRotation = RandFloat()*CParams::dTwoPi;

	return;
}
//---------------------WorldTransform--------------------------------
//
//	sets up a translation matrix for the sweeper according to its
//  scale, rotation and position. Returns the transformed vertices.
//-------------------------------------------------------------------
void CMinesweeper::WorldTransform(vector<SPoint> &sweeper)
{
	//create the world transformation matrix
	C2DMatrix matTransform;
	
	//scale
	matTransform.Scale(m_dScale, m_dScale);
	
	//rotate
	matTransform.Rotate(m_dRotation);
	
	//and translate
	matTransform.Translate(m_vPosition.x, m_vPosition.y);
	
	//now transform the ships vertices
	matTransform.TransformSPoints(sweeper);
}

//-------------------------------Update()--------------------------------
//
//	First we take sensor readings. These are then fed into the learning algorithm
//
//	The inputs are:
//	
//	A vector to the closest mine (x, y)
//	The sweepers 'look at' vector (x, y)
//	So given a force we calculate the resultant rotation 
//	and acceleration. This is then applied to current velocity vector.
//
//-----------------------------------------------------------------------
bool CMinesweeper::Update(vector<CCollisionObject> &objects)
{
	//TODO: update the sweeepers here
	/*
					 _       _         _                   
					| |     | |       | |                  
	 _   _ _ __   __| | __ _| |_ ___  | |__   ___ _ __ ___ 
	| | | | '_ \ / _` |/ _` | __/ _ \ | '_ \ / _ \ '__/ _ \
	| |_| | |_) | (_| | (_| | ||  __/ | | | |  __/ | |  __/
	 \__,_| .__/ \__,_|\__,_|\__\___| |_| |_|\___|_|  \___|
		  | |                                              
		  |_|   
	*/ 

	//get vector to closest mine
	SVector2D vClosestMine = GetClosestMine(objects);
	//normalise it
	Vec2DNormalize(vClosestMine);
	
	//TODO: calculate the steering forces here, it is set to 0 for now...
	double RotForce = 0;
	
	//clamp rotation
	Clamp(RotForce, -CParams::dMaxTurnRate, CParams::dMaxTurnRate);

	m_dRotation += RotForce;

	//TODO: calculate the speed of the sweeper here (it is set to 0.5 by default)
	m_dSpeed = 0.5;	

	//update Look At 
	m_vLookAt.x = -sin(m_dRotation);
	m_vLookAt.y = cos(m_dRotation);

	//update position
	m_vPosition += (m_vLookAt * m_dSpeed);

	
	//wrap around window limits
	if (m_vPosition.x > CParams::WindowWidth) m_vPosition.x = 0;
	if (m_vPosition.x < 0) m_vPosition.x = CParams::WindowWidth;
	if (m_vPosition.y > CParams::WindowHeight) m_vPosition.y = 0;
	if (m_vPosition.y < 0) m_vPosition.y = CParams::WindowHeight;

	return true;
}


//----------------------GetClosestObject()---------------------------------
//
//	returns the vector from the sweeper to the closest mine
//
//-----------------------------------------------------------------------
SVector2D CMinesweeper::GetClosestMine(vector<CCollisionObject> &objects)
{
	double			closest_so_far = 99999;

	SVector2D		vClosestObject(0, 0);

	//cycle through mines to find closest
	for (int i=0; i<objects.size(); i++)
	{
		double len_to_object = Vec2DLength(objects[i].getPosition() - m_vPosition);

		if (len_to_object < closest_so_far)
		{
			closest_so_far	= len_to_object;
			
			vClosestObject	= m_vPosition - objects[i].getPosition();

      m_iClosestMine = i;
		}
	}

	return vClosestObject;
}
//----------------------------- CheckForMine -----------------------------
//
//  this function checks for collision with its closest mine (calculated
//  earlier and stored in m_iClosestMine)
//-----------------------------------------------------------------------
int CMinesweeper::CheckForMine(vector<CCollisionObject> &objects, double size)
{
	SVector2D DistToObject = m_vPosition - objects[m_iClosestMine].getPosition();
		
	if (Vec2DLength(DistToObject) < (size + 5))
	{
			return m_iClosestMine;
	}

  return -1;
}

		
