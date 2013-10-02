#include "CMinesweeper.h"
#include "CController.h"

//-----------------------------------constructor-------------------------
//
//-----------------------------------------------------------------------
CMinesweeper::CMinesweeper(CController* _controller):
						     controller(_controller),
							 state_index(0),
							 action_index(0),
                             m_dRotation(RandFloat()*CParams::dTwoPi),
                             m_dMinesGathered(0),
							 m_dScale(CParams::iSweeperScale),
                             m_iClosestMine(0),
							 m_active(true)
{
	//create a random start position
	m_vPosition = SVector2D((RandFloat() * CParams::WindowWidth), 
					                (RandFloat() * CParams::WindowHeight));

	State s;
	s.angle = 0;
	s.mineType = true;
	state = s;
}

//-------------------------------------------Reset()--------------------
//
//	Resets the sweepers position, MinesGathered, rotation and activity
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

	//reset activity
	m_active = true;

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
	if(m_active)
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

		//find distance to closest object
		double distance = Vec2DLength(vClosestMine);
		//normalise vectors to find angle
		Vec2DNormalize(vClosestMine);
		
		// find angle between closest mine and sweeper
		double angle = acos(Vec2DDot(m_vLookAt, vClosestMine));
		
		// select an action a and execute it
		int sign = Vec2DSign(m_vLookAt, vClosestMine);

		double RotForce = sign * controller->actions[action_index] * 10;

		if (objects[m_iClosestMine].getType() == CCollisionObject::SuperMine)
			RotForce *= -1;
		//clamp rotation
		Clamp(RotForce, -CParams::dMaxTurnRate, CParams::dMaxTurnRate);

		m_dRotation += RotForce;

		//TODO: calculate the speed of the sweeper here (it is set to 0.5 by default)
		m_dSpeed = 0.9;	

		//update Look At 
		m_vLookAt.x = -sin(m_dRotation);
		m_vLookAt.y = cos(m_dRotation);

		//update position
		m_vPosition += (m_vLookAt * m_dSpeed);

		// receive immediate reward r
		// get updated vector to same mine, mine index given by m_iClosestMine
		SVector2D vUpdatedClosestMine;
		vUpdatedClosestMine = m_vPosition - objects[m_iClosestMine].getPosition();
		//find new distance to closest mine
		double new_distance = Vec2DLength(vUpdatedClosestMine);

		int reward = 0;
		// reward for ordinary mine
		if (controller->states[state_index].mineType && new_distance < distance)
		{
			reward = 10;
		}
		// reward for supermine
		if ((!controller->states[state_index].mineType) && new_distance < distance)
		{
			reward = -10; // getting too close
		}

		// observe the new state s'
		State new_state;
		
		vClosestMine = GetClosestMine(objects);
		Vec2DNormalize(m_vLookAt);
		Vec2DNormalize(vClosestMine);
		new_state.angle = acos(Vec2DDot(m_vLookAt, vClosestMine));

		if (objects[m_iClosestMine].getType() == CCollisionObject::Mine)
		{
			new_state.mineType = true;
		}
		else
		{
			new_state.mineType = false;
		}

		// find state index relating to new state
		int new_state_index = 0;

		if (new_state.mineType)
		{
			new_state_index = new_state.angle + CParams::iNumStates / 4;
		}
		else
		{
			new_state_index = ( new_state.angle + CParams::iNumStates / 4 ) * 2;
		}

		if (new_state_index < 0)
		{
			new_state_index = 0;
		}

		int new_action_index = 0;
		double largest = 0;

		// update Q(s,a):
		for (int i = 0; i < CParams::iNumActions; ++i)
		{
			double temp = controller->getQ(new_state_index, i);
			if (temp > largest)
			{
				largest = temp;
				new_action_index = i;
			}
		}

		// Q(s,a) <- r + y * max Q(s', a') (max over all actions taken over new state)
		// ( 0.9 = y = discount factor, 0 <= y < 1)
		controller->setQ(state_index, action_index, reward + 0.9 * largest);

		// s <- s'
		state = new_state;
		state_index = new_state_index;
		action_index = new_action_index;

		//wrap around window limits
		if (m_vPosition.x > CParams::WindowWidth) m_vPosition.x = 0;
		if (m_vPosition.x < 0) m_vPosition.x = CParams::WindowWidth;
		if (m_vPosition.y > CParams::WindowHeight) m_vPosition.y = 0;
		if (m_vPosition.y < 0) m_vPosition.y = CParams::WindowHeight;

	}

	return true;
}


//----------------------GetClosestMine()---------------------------------
//
//	returns the vector from the sweeper to the closest mine, within the
//  sweeper's field of view
//
//-----------------------------------------------------------------------
SVector2D CMinesweeper::GetClosestMine(vector<CCollisionObject> &objects)
{
	double			closest_so_far = 99999;

	SVector2D		vClosestObject(0, 0);

	//cycle through mines to find closest
	for (int i=0; i<objects.size(); i++)
	{
		if (objects[i].getActive())
		{
			double len_to_object = Vec2DLength(objects[i].getPosition() - m_vPosition);

			// find angle between forward direction and mine (in radians)
			SVector2D current_mine_vec = m_vPosition - objects[i].getPosition();
			Vec2DNormalize(m_vLookAt);
			Vec2DNormalize(current_mine_vec);
			double angle = acos(Vec2DDot(m_vLookAt, current_mine_vec));

			if(angle > CParams::dHalfPi) // out of field of view (behind sweeper)
				continue;
			else
			{
				if (len_to_object < closest_so_far)
				{
					closest_so_far	= len_to_object;
				
					vClosestObject	= m_vPosition - objects[i].getPosition();

					m_iClosestMine = i;
				}
			}
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

		
