#include "CController.h"


//these hold the geometry of the sweepers and the mines
const int	 NumSweeperVerts = 16;
const SPoint sweeper[NumSweeperVerts] = {SPoint(-1, -1),
                                         SPoint(-1, 1),
                                         SPoint(-0.5, 1),
                                         SPoint(-0.5, -1),

                                         SPoint(0.5, -1),
                                         SPoint(1, -1),
                                         SPoint(1, 1),
                                         SPoint(0.5, 1),
                                         
                                         SPoint(-0.5, -0.5),
                                         SPoint(0.5, -0.5),

                                         SPoint(-0.5, 0.5),
                                         SPoint(-0.25, 0.5),
                                         SPoint(-0.25, 1.75),
                                         SPoint(0.25, 1.75),
                                         SPoint(0.25, 0.5),
                                         SPoint(0.5, 0.5)};



const int NumMineVerts = 4;
const SPoint mine[NumMineVerts] = {SPoint(-1, -1),
                                   SPoint(-1, 1),
                                   SPoint(1, 1),
                                   SPoint(1, -1)};



//---------------------------------------constructor---------------------
//
//	initilaize the sweepers and their brains
//
//-----------------------------------------------------------------------
CController::CController(HWND hwndMain): m_NumSweepers(CParams::iNumSweepers),
										 m_NumInactiveSweepers(0),
										                     m_bFastRender(false),
										                     m_iTicks(0),
										                     m_NumMines(CParams::iNumMines),
															 m_NumSuperMines(CParams::iNumSuperMines),
															 m_NumRocks(CParams::iNumRocks),
										                     m_hwndMain(hwndMain),
										                     m_iIterations(0),
                                         cxClient(CParams::WindowWidth),
                                         cyClient(CParams::WindowHeight)
{
	//let's create the mine sweepers
	for (int i=0; i<m_NumSweepers; ++i)
	{
		m_vecSweepers.push_back(CMinesweeper(this));
	}
	
	m_vecAvMinesGathered.push_back(0);
	m_vecMostMinesGathered.push_back(0);
	m_vecDeadSweepers.push_back(0);
	//m_vecSweepersRemaining.push_back(0);

	//TODO: initialse the learning algorithm here
	// _       _ _   _       _ _           _                   
	//(_)     (_) | (_)     | (_)         | |                  
	// _ _ __  _| |_ _  __ _| |_ _______  | |__   ___ _ __ ___ 
	//| | '_ \| | __| |/ _` | | |_  / _ \ | '_ \ / _ \ '__/ _ \
	//| | | | | | |_| | (_| | | |/ /  __/ | | | |  __/ | |  __/
	//|_|_| |_|_|\__|_|\__,_|_|_/___\___| |_| |_|\___|_|  \___|

	//initialize mines in random positions within the application window
	for (int i=0; i<m_NumMines; ++i)
	{
		m_vecObjects.push_back(CCollisionObject(CCollisionObject::Mine, SVector2D(RandFloat() * cxClient,
                                   RandFloat() * cyClient)));
	}
	for (int i=0; i<m_NumSuperMines; ++i)
	{
		m_vecObjects.push_back(CCollisionObject(CCollisionObject::SuperMine, SVector2D(RandFloat() * cxClient,
                                   RandFloat() * cyClient)));
	}
	for (int i=0; i<m_NumRocks; ++i)
	{
		m_vecObjects.push_back(CCollisionObject(CCollisionObject::Rock, SVector2D(RandFloat() * cxClient,
                                   RandFloat() * cyClient)));
	}

	// create policy
	initializeQ(CParams::iNumStates, CParams::iNumActions);

	// for each s,a
	for (int s = 0; s < CParams::iNumStates; ++s)
	{
		for (int a = 0; a < CParams::iNumActions; ++a)
		{
			// initialize table entry to 0
			setQ(s, a, 0);
		}
	}

	// initialize possible states (angle to closest mine and type (mine TRUE or supermine FALSE))
	// divide by 4 to create field of view (i.e. 240 / 4 = 60 indicates 60 degree vision on either side from normal)
	// for ordinary mines
	for (int i = CParams::iNumStates / (-4); i < CParams::iNumStates / 4; ++i)
	{
		State s;
		s.angle = i;
		s.mineType = true;
		states.push_back(s);
	}

	// for supermines
	for (int i = CParams::iNumStates / (-4); i < CParams::iNumStates / 4; ++i)
	{
		State s;
		s.angle = i;
		s.mineType = false;
		states.push_back(s);
	}

	// initialize possible actions
	actions.push_back(1);
	actions.push_back(-1);

	//create a pen for the graph drawing
	m_BluePen  = CreatePen(PS_SOLID, 1, RGB(0, 0, 255));
	m_RedPen   = CreatePen(PS_SOLID, 1, RGB(255, 0, 0));
	m_GreenPen = CreatePen(PS_SOLID, 1, RGB(0, 150, 0));

	m_OldPen	= NULL;

	//fill the vertex buffers
	for (int i=0; i<NumSweeperVerts; ++i)
	{
		m_SweeperVB.push_back(sweeper[i]);
	}

	for (int i=0; i<NumMineVerts; ++i)
	{
		m_MineVB.push_back(mine[i]);
	}

}


//--------------------------------------destructor-------------------------------------
//
//--------------------------------------------------------------------------------------
CController::~CController()
{
	DeleteObject(m_BluePen);
	DeleteObject(m_RedPen);
	DeleteObject(m_GreenPen);
	DeleteObject(m_OldPen);
	delete []Q;
}


//---------------------WorldTransform--------------------------------
//
//	sets up the translation matrices for the mines and applies the
//	world transform to each vertex in the vertex buffer passed to this
//	method.
//-------------------------------------------------------------------
void CController::WorldTransform(vector<SPoint> &VBuffer, SVector2D vPos)
{
	//create the world transformation matrix
	C2DMatrix matTransform;
	
	//scale
	matTransform.Scale(CParams::dMineScale, CParams::dMineScale);
	
	//translate
	matTransform.Translate(vPos.x, vPos.y);

	//transform the ships vertices
	matTransform.TransformSPoints(VBuffer);
}


//-------------------------------------Update-----------------------------
//
//	This is the main workhorse. The entire simulation is controlled from here.
//
//	The comments should explain what is going on adequately.
//-------------------------------------------------------------------------
bool CController::Update()
{
	//run the sweepers through CParams::iNumTicks amount of cycles. During
	//this loop each sweeper is constantly updated with the appropriate
	//information from its surroundings. The output from the learning algorithm is obtained
	//and the sweeper is moved. If it encounters a mine its MinesGathered is
	//updated appropriately,
	if (m_iTicks++ < CParams::iNumTicks)
	{
		for (int i=0; i<m_NumSweepers; ++i)
		{
			//update the position
			if (!m_vecSweepers[i].Update(m_vecObjects))
			{
				//error in processing the learning algorithm
				MessageBox(m_hwndMain, "An error occured while processing!", "Error", MB_OK);

				return false;
			}
				
			//see if it's found a mine
			int GrabHit = m_vecSweepers[i].CheckForMine(m_vecObjects,
													CParams::dMineScale);

			if (GrabHit >= 0)
			{
				// --- collided with mine --- 
				if(m_vecObjects[GrabHit].getType()==CCollisionObject::Mine && m_vecObjects[GrabHit].getActive())
				{
					//we have discovered a mine so increase MinesGathered
					m_vecSweepers[i].IncrementMinesGathered();

					//mine found so replace the mine with another at a random position
					m_vecObjects[GrabHit].setActive(false);
				}

				// --- collided with supermine --- 
				if(m_vecObjects[GrabHit].getType()==CCollisionObject::SuperMine && m_vecObjects[GrabHit].getActive())
				{
					// set inactive for this iteration
					m_vecSweepers[i].setActive(false);

					// decrease number of sweepers
					++m_NumInactiveSweepers;

					// hide supermine for rest of iteration
					m_vecObjects[GrabHit].setActive(false);


				}
			}
		}
	}
	//Time to update the sweepers for the next iteration
	else
	{
		//update the stats to be used in our stat window
		//TODO: at the moment this is set to 0 for all sweepers by default.
		//		You should apply meaningful stats from your sweepers here.
		
		// Find data to add to stats vectors
		double tot_mines_collected = 0.0;
		double max_mines = 0.0;
		for(int i=0; i<m_NumSweepers ; ++i)
		{
			tot_mines_collected += m_vecSweepers[i].MinesGathered();
			if(m_vecSweepers[i].MinesGathered() > max_mines)
					max_mines = m_vecSweepers[i].MinesGathered();
		}
		double average = tot_mines_collected/m_NumSweepers;

		// add data for this iteration to stats vectors
		m_vecAvMinesGathered.push_back(average);
		m_vecMostMinesGathered.push_back(max_mines);
		m_vecDeadSweepers.push_back(m_NumInactiveSweepers);

		/*string op = itos(m_iIterations)+": "+ftos(average)+" "+ftos(max_mines)+" "+itos(m_NumInactiveSweepers)+"\n";
		OutputDebugString(op.c_str());*/

		//increment the iteration counter
		++m_iIterations;

		//reset cycles
		m_iTicks = 0;
	
		//reset the sweepers positions etc (and sets activity to true)
		for (int i=0; i<m_NumSweepers; ++i)
		{
			m_vecSweepers[i].Reset();
		}

		//reset number of active sweepers
		m_NumInactiveSweepers = 0;

		// reset all mines (and supermines) in random locations (and sets activity to true)
		for (int i=0; i< m_vecObjects.size(); ++i)
		{
			m_vecObjects[i] = CCollisionObject(m_vecObjects[i].getType(),SVector2D(RandFloat() * cxClient,RandFloat() * cyClient));		
		}
	}
	return true;
}
//------------------------------------Render()--------------------------------------
//
//----------------------------------------------------------------------------------
void CController::Render(HDC surface)
{
	//render the stats
	string s = "Iteration:          " + itos(m_iIterations);
	TextOut(surface, 5, 0, s.c_str(), s.size());

	//do not render if running at accelerated speed
	if (!m_bFastRender)
	{
		//keep a record of the old pen
		m_OldPen = (HPEN)SelectObject(surface, m_GreenPen);
		
		//render the mines
		for (int i=0; i<m_NumMines+m_NumSuperMines+m_NumRocks; ++i)
		{
			if ( m_vecObjects[i].getType() == CCollisionObject::Mine)
			{
				SelectObject(surface, m_GreenPen);
			}
			else if ( m_vecObjects[i].getType() == CCollisionObject::Rock)
			{
				SelectObject(surface, m_BluePen );
			}
			else if ( m_vecObjects[i].getType() == CCollisionObject::SuperMine)
			{
				SelectObject(surface, m_RedPen);
			}
			//grab the vertices for the mine shape
			vector<SPoint> mineVB = m_MineVB;

			WorldTransform(mineVB, m_vecObjects[i].getPosition());

			//draw the mines (only if active)
			if(m_vecObjects[i].getActive())
			{
				MoveToEx(surface, (int)mineVB[0].x, (int)mineVB[0].y, NULL);

				for (int vert=1; vert<mineVB.size(); ++vert)
				{
					LineTo(surface, (int)mineVB[vert].x, (int)mineVB[vert].y);
				}

				LineTo(surface, (int)mineVB[0].x, (int)mineVB[0].y);
			}
			
		}
       		
		//we want some sweepers displayed in red
		SelectObject(surface, m_RedPen);

		//render the sweepers
		for (int i=0; i<m_NumSweepers; i++)
		{
			if (i == CParams::iNumElite)
			{
				SelectObject(surface, m_OldPen);
			}
      
			//grab the sweeper vertices
			vector<SPoint> sweeperVB = m_SweeperVB;

			//transform the vertex buffer
			m_vecSweepers[i].WorldTransform(sweeperVB);
			
			if(m_vecSweepers[i].getActive())
			{
				//draw the sweeper left track
				MoveToEx(surface, (int)sweeperVB[0].x, (int)sweeperVB[0].y, NULL);

				for (int vert=1; vert<4; ++vert)
				{
					LineTo(surface, (int)sweeperVB[vert].x, (int)sweeperVB[vert].y);
				}

				LineTo(surface, (int)sweeperVB[0].x, (int)sweeperVB[0].y);

				//draw the sweeper right track	
				MoveToEx(surface, (int)sweeperVB[4].x, (int)sweeperVB[4].y, NULL);

				for (int vert=5; vert<8; ++vert)
				{
					LineTo(surface, (int)sweeperVB[vert].x, (int)sweeperVB[vert].y);
				}

				LineTo(surface, (int)sweeperVB[4].x, (int)sweeperVB[4].y);

				MoveToEx(surface, (int)sweeperVB[8].x, (int)sweeperVB[8].y, NULL);
				LineTo(surface, (int)sweeperVB[9].x, (int)sweeperVB[9].y);

				MoveToEx(surface, (int)sweeperVB[10].x, (int)sweeperVB[10].y, NULL);

				for (int vert=11; vert<16; ++vert)
				{
					LineTo(surface, (int)sweeperVB[vert].x, (int)sweeperVB[vert].y);
				}
			}

		}

		//put the old pen back
		SelectObject(surface, m_OldPen);

	}//end if

  else
  {
    PlotStats(surface);
  }

}
//--------------------------PlotStats-------------------------------------
//
//  Given a surface to draw on this function displays stats and a crude
//  graph showing best and average MinesGathered
//------------------------------------------------------------------------
void CController::PlotStats(HDC surface)
{
	//TODO: at the moment this is set to 0 by default.
	//		You should plot meaningful stats from your sweepers here.
    string s = "Most MinesGathered:       " + ftos(m_vecMostMinesGathered.back());
	TextOut(surface, 5, 20, s.c_str(), s.size());

     s = "Average MinesGathered: " + ftos(m_vecAvMinesGathered.back());
	TextOut(surface, 5, 40, s.c_str(), s.size());

	s = "Number of dead sweepers: " + ftos(m_vecDeadSweepers.back());
	TextOut(surface, 5, 60, s.c_str(), s.size());
    
    //render the graph
    float HSlice = (float)cxClient/(m_iIterations+1);
	float VSlice = (float)cyClient/(10);

    //plot the graph for the best MinesGathered
    float x = 0;
    
    m_OldPen = (HPEN)SelectObject(surface, m_RedPen);

    MoveToEx(surface, 0, cyClient, NULL);
    
    for (int i=0; i<m_vecMostMinesGathered.size(); ++i)
    {
       LineTo(surface, x, cyClient - VSlice*m_vecMostMinesGathered[i]);

       x += HSlice;
    }

    //plot the graph for the average MinesGathered
    x = 0;

    SelectObject(surface, m_BluePen);

    MoveToEx(surface, 0, cyClient, NULL);
    
    for (int i=0; i<m_vecAvMinesGathered.size(); ++i)
    {
       LineTo(surface, (int)x, (int)(cyClient - VSlice*m_vecAvMinesGathered[i]));

       x += HSlice;
    }

	//plot the graph for the sweepers remaining
    x = 0;

    SelectObject(surface, m_GreenPen);

    MoveToEx(surface, 0, cyClient, NULL);
    
    for (int i=0; i<m_vecDeadSweepers.size(); ++i)
    {
       LineTo(surface, (int)x, (int)(cyClient - VSlice*m_vecDeadSweepers[i]));

       x += HSlice;
    }

    //replace the old pen
    SelectObject(surface, m_OldPen);
}

