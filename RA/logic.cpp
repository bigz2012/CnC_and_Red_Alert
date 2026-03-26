/*
**	Command & Conquer Red Alert(tm)
**	Copyright 2025 Electronic Arts Inc.
**
**	This program is free software: you can redistribute it and/or modify
**	it under the terms of the GNU General Public License as published by
**	the Free Software Foundation, either version 3 of the License, or
**	(at your option) any later version.
**
**	This program is distributed in the hope that it will be useful,
**	but WITHOUT ANY WARRANTY; without even the implied warranty of
**	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
**	GNU General Public License for more details.
**
**	You should have received a copy of the GNU General Public License
**	along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

/* $Header: /CounterStrike/LOGIC.CPP 1     3/03/97 10:25a Joe_bostic $ */
/***********************************************************************************************
 ***              C O N F I D E N T I A L  ---  W E S T W O O D  S T U D I O S               ***
 ***********************************************************************************************
 *                                                                                             *
 *                 Project Name : Command & Conquer                                            *
 *                                                                                             *
 *                    File Name : LOGIC.CPP                                                    *
 *                                                                                             *
 *                   Programmer : Joe L. Bostic                                                *
 *                                                                                             *
 *                   Start Date : September 27, 1993                                           *
 *                                                                                             *
 *                  Last Update : July 30, 1996 [JLB]                                          *
 *                                                                                             *
 *---------------------------------------------------------------------------------------------*
 * Functions:                                                                                  *
 *   LogicClass::AI -- Handles AI logic processing for game objects.                           *
 *   LogicClass::Debug_Dump -- Displays logic class status to the mono screen.                 *
 *   LogicClass::Detach -- Detatch the specified target from the logic system.                 *
 * - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

#include	"function.h"
#include	"logic.h"
#include 	"vortex.h"
#include	<SDL.h>

/*
**	Random skirmish events system. Triggers hostile events at random
**	intervals to keep skirmish games interesting.
*/
static unsigned int NextRandomEventTime = 0;
static const int RANDOM_EVENT_MIN_DELAY = 3 * 60;  /* 3 minutes in seconds */
static const int RANDOM_EVENT_MAX_DELAY = 7 * 60;  /* 7 minutes in seconds */

static void Schedule_Next_Random_Event(void)
{
	int delay = RANDOM_EVENT_MIN_DELAY + (Random_Pick(0, RANDOM_EVENT_MAX_DELAY - RANDOM_EVENT_MIN_DELAY));
	NextRandomEventTime = SDL_GetTicks() + (delay * 1000);
}

static void Do_Random_Skirmish_Event(void)
{
	/*
	**	Find or create a hostile house for the event forces.
	**	Use HOUSE_BAD (Soviet/Nod) as a generic enemy.
	*/
	/*
	**	Find a hostile house. Use any active non-human AI opponent
	**	from skirmish — they already have proper enemy relationships.
	**	Fall back to HOUSE_BAD/HOUSE_GOOD if no AI found.
	*/
	HouseClass * hostile = NULL;
	for (int h = 0; h < Houses.Count(); h++) {
		HouseClass * hh = Houses.Ptr(h);
		if (hh && hh->IsActive && !hh->IsHuman && !hh->IsDefeated) {
			hostile = hh;
			break;
		}
	}
	if (!hostile) {
		hostile = HouseClass::As_Pointer(HOUSE_BAD);
	}
	if (!hostile) return;

	/*
	**	Ensure the hostile house is enemy to all other active houses.
	*/
	for (int h2 = 0; h2 < Houses.Count(); h2++) {
		HouseClass * hh = Houses.Ptr(h2);
		if (hh && hh->IsActive && hh != hostile) {
			hostile->Make_Enemy(hh->Class->House);
		}
	}

	/*
	**	Pick a random drop zone across the map (not near any player base).
	**	Choose a random cell somewhere in the middle portion of the map.
	*/
	int map_x = Map.MapCellX + Random_Pick(Map.MapCellWidth / 4, (Map.MapCellWidth * 3) / 4);
	int map_y = Map.MapCellY + Random_Pick(Map.MapCellHeight / 4, (Map.MapCellHeight * 3) / 4);
	CELL drop_cell = XY_Cell(map_x, map_y);
	drop_cell = Map.Nearby_Location(drop_cell, SPEED_FOOT);

	/*
	**	Pick a random infantry type for the drop.
	*/
	static const InfantryType drop_types[] = {
		INFANTRY_E1, INFANTRY_E1, INFANTRY_E2, INFANTRY_E3, INFANTRY_E4
	};
	int pick = Random_Pick(0, 4);
	InfantryType inf_type = drop_types[pick];

	Speak(VOX_BASE_UNDER_ATTACK);

	/*
	**	Reveal the full drop line for the human player so they can watch.
	*/
	if (PlayerPtr) {
		for (int r = -15; r <= 15; r += 5) {
			int rx = map_x + r;
			if (rx >= Map.MapCellX && rx < Map.MapCellX + Map.MapCellWidth) {
				Map.Sight_From(XY_Cell(rx, map_y), 6, PlayerPtr, false);
			}
		}
	}

	/*
	**	All planes come from the same direction (north edge).
	**	Create 10 Badger transports, each with a full infantry cargo,
	**	targeting cells spread across a horizontal line.
	*/
	int max_pass = AircraftTypeClass::As_Reference(AIRCRAFT_BADGER).Max_Passengers();
	SourceType source = SOURCE_NORTH;
	CELL spawn_cell = Map.Calculated_Cell(source, -1, -1, SPEED_WINGED);

	for (int plane = 0; plane < 10; plane++) {
		int offset_x = map_x + (plane - 5) * 3;
		if (offset_x < Map.MapCellX) offset_x = Map.MapCellX;
		if (offset_x >= Map.MapCellX + Map.MapCellWidth) offset_x = Map.MapCellX + Map.MapCellWidth - 1;
		CELL target_cell = XY_Cell(offset_x, map_y);

		/*
		**	Create the Badger transport aircraft.
		*/
		ScenarioInit++;
		AircraftClass * aircraft = (AircraftClass *)AircraftTypeClass::As_Reference(AIRCRAFT_BADGER).Create_One_Of(hostile);
		ScenarioInit--;
		if (!aircraft) continue;

		aircraft->IsALoaner = true;
		aircraft->Passenger = true;

		/*
		**	Load infantry into the aircraft cargo hold.
		*/
		for (int p = 0; p < max_pass; p++) {
			ScenarioInit++;
			InfantryClass * inf = (InfantryClass *)InfantryTypeClass::As_Reference(inf_type).Create_One_Of(hostile);
			ScenarioInit--;
			if (inf) {
				inf->IsALoaner = true;
				inf->Assign_Mission(MISSION_HUNT);
				aircraft->Attach(inf);
			}
		}

		/*
		**	Place the aircraft at the north edge of the map and send it to the drop zone.
		*/
		ScenarioInit++;
		if (aircraft->Unlimbo(Cell_Coord(spawn_cell), DIR_S)) {
			aircraft->Assign_Mission(MISSION_MOVE);
			aircraft->Assign_Target(::As_Target(target_cell));
			aircraft->Assign_Destination(::As_Target(target_cell));
			aircraft->Commence();
		} else {
			delete aircraft;
		}
		ScenarioInit--;
	}
}

static unsigned FramesPerSecond=0;


#ifdef CHEAT_KEYS
/***********************************************************************************************
 * LogicClass::Debug_Dump -- Displays logic class status to the mono screen.                   *
 *                                                                                             *
 *    This is a debugging support routine. It displays the current state of the logic class    *
 *    to the monochrome monitor. It assumes that it is being called once per second.           *
 *                                                                                             *
 * INPUT:   none                                                                               *
 *                                                                                             *
 * OUTPUT:  none                                                                               *
 *                                                                                             *
 * WARNINGS:   Call this routine only once per second.                                         *
 *                                                                                             *
 * HISTORY:                                                                                    *
 *   05/31/1994 JLB : Created.                                                                 *
 *   01/26/1996 JLB : Prints game time value.                                                  *
 *=============================================================================================*/
void LogicClass::Debug_Dump(MonoClass * mono) const
{
	#define RECORDCOUNT	40
	#define RECORDHEIGHT 21
	static int _framecounter = 0;

	static bool first = true;
	if (first) {
		first = false;
	mono->Set_Cursor(0, 0);
	mono->Print(Text_String(TXT_DEBUG_STRESS));
	}

//mono->Set_Cursor(0,0);mono->Printf("%d", AllowVoice);


	_framecounter++;
	mono->Set_Cursor(1, 1);mono->Printf("%ld", (long)Scen.Timer);
	mono->Set_Cursor(10, 1);mono->Printf("%3d", FramesPerSecond);
	mono->Set_Cursor(1, 3);mono->Printf("%02d:%02d:%02d", Scen.Timer / TICKS_PER_HOUR, (Scen.Timer % TICKS_PER_HOUR)/TICKS_PER_MINUTE, (Scen.Timer % TICKS_PER_MINUTE)/TICKS_PER_SECOND);

	mono->Set_Cursor(1, 11);mono->Printf("%3d", Units.Count());
	mono->Set_Cursor(1, 12);mono->Printf("%3d", Infantry.Count());
	mono->Set_Cursor(1, 13);mono->Printf("%3d", Aircraft.Count());
	mono->Set_Cursor(1, 14);mono->Printf("%3d", Vessels.Count());
	mono->Set_Cursor(1, 15);mono->Printf("%3d", Buildings.Count());
	mono->Set_Cursor(1, 16);mono->Printf("%3d", Terrains.Count());
	mono->Set_Cursor(1, 17);mono->Printf("%3d", Bullets.Count());
	mono->Set_Cursor(1, 18);mono->Printf("%3d", Anims.Count());
	mono->Set_Cursor(1, 19);mono->Printf("%3d", Teams.Count());
	mono->Set_Cursor(1, 20);mono->Printf("%3d", Triggers.Count());
	mono->Set_Cursor(1, 21);mono->Printf("%3d", TriggerTypes.Count());
	mono->Set_Cursor(1, 22);mono->Printf("%3d", Factories.Count());

	SpareTicks = min((long)SpareTicks, (long)TIMER_SECOND);

	/*
	**	CPU utilization record.
	*/
	mono->Sub_Window(15, 1, 6, 11);
	mono->Scroll();
	mono->Set_Cursor(0, 10);
	mono->Printf("%3d%%", ((TIMER_SECOND-SpareTicks)*100) / TIMER_SECOND);

	/*
	**	Update the frame rate log.
	*/
	mono->Sub_Window(22, 1, 6, 11);
	mono->Scroll();
	mono->Set_Cursor(0, 10);
	mono->Printf("%4d", FramesPerSecond);

	/*
	**	Update the findpath calc record.
	*/
	mono->Sub_Window(50, 1, 6, 11);
	mono->Scroll();
	mono->Set_Cursor(0, 10);
	mono->Printf("%4d", PathCount);
	PathCount = 0;

	/*
	**	Update the cell redraw record.
	*/
	mono->Sub_Window(29, 1, 6, 11);
	mono->Scroll();
	mono->Set_Cursor(0, 10);
	mono->Printf("%5d", CellCount);
	CellCount = 0;

	/*
	**	Update the target scan record.
	*/
	mono->Sub_Window(36, 1, 6, 11);
	mono->Scroll();
	mono->Set_Cursor(0, 10);
	mono->Printf("%5d", TargetScan);
	TargetScan = 0;

	/*
	**	Sidebar redraw record.
	*/
	mono->Sub_Window(43, 1, 6, 11);
	mono->Scroll();
	mono->Set_Cursor(0, 10);
	mono->Printf("%5d", SidebarRedraws);
	SidebarRedraws = 0;

	/*
	**	Update the CPU utilization chart.
	*/
	mono->Sub_Window(15, 13, 63, 10);
	mono->Pan(1);
	mono->Sub_Window(15, 13, 64, 10);
	int graph = RECORDHEIGHT * fixed(TIMER_SECOND-SpareTicks, TIMER_SECOND);
	for (int row = 1; row < RECORDHEIGHT; row += 2) {
		static char _barchar[4] = {' ', 220, 0, 219};
		char str[2];
		int index = 0;

		index |= (graph >= row) ? 0x01 : 0x00;
		index |= (graph >= row+1) ? 0x02: 0x00;

		str[1] = '\0';
		str[0] = _barchar[index];
		mono->Text_Print(str, 62, 9-(row/2));
	}
	mono->Sub_Window();


	SpareTicks = 0;
	FramesPerSecond = 0;
}
#endif


/***********************************************************************************************
 * LogicClass::AI -- Handles AI logic processing for game objects.                             *
 *                                                                                             *
 *    This routine is used to perform the AI processing for all game objects. This includes    *
 *    all houses, factories, objects, and teams.                                               *
 *                                                                                             *
 * INPUT:   none                                                                               *
 *                                                                                             *
 * OUTPUT:  none                                                                               *
 *                                                                                             *
 * WARNINGS:   none                                                                            *
 *                                                                                             *
 * HISTORY:                                                                                    *
 *   05/29/1994 JLB : Created.                                                                 *
 *   12/17/1994 JLB : Must perform one complete pass rather than bailing early.                *
 *   12/23/1994 JLB : Ensures that no object gets skipped if it was deleted.                   *
 *=============================================================================================*/
void LogicClass::AI(void)
{
	int index;

	FramesPerSecond++;

	/*
	** Fading to B&W or color due to the chronosphere is handled here.
	*/
	Scen.Do_Fade_AI();

	/*
	**	Handle any general timer trigger events.
	*/
	for (LogicTriggerID = 0; LogicTriggerID < LogicTriggers.Count(); LogicTriggerID++) {
		TriggerClass * trig = LogicTriggers[LogicTriggerID];

		/*
		**	Global changed trigger event might be triggered.
		*/
		if (Scen.IsGlobalChanged) {
			if (trig->Spring(TEVENT_GLOBAL_SET)) continue;
			if (trig->Spring(TEVENT_GLOBAL_CLEAR)) continue;
		}

		/*
		**	Bridge change event.
		*/
		if (Scen.IsBridgeChanged) {
			if (trig->Spring(TEVENT_ALL_BRIDGES_DESTROYED)) continue;
		}

		/*
		**	General time expire trigger events can be sprung without warning.
		*/
		if (trig->Spring(TEVENT_TIME)) continue;

		/*
		**	The mission timer expiration trigger event might spring if the timer is active
		**	but at a value of zero.
		*/
		if (Scen.MissionTimer.Is_Active() && Scen.MissionTimer == 0) {
			if (trig->Spring(TEVENT_MISSION_TIMER_EXPIRED)) continue;
		}
	}

	/*
	**	Clean up any status values that were maintained only for logic trigger
	**	purposes.
	*/
	if (Scen.MissionTimer.Is_Active() && Scen.MissionTimer == 0) {
		Scen.MissionTimer.Stop();
		Map.Flag_To_Redraw(true);			// Used only to cause tabs to redraw in new state.
	}
	Scen.IsGlobalChanged = false;
	Scen.IsBridgeChanged = false;
	/*
	**	Shadow creeping back over time is handled here.
	*/
	if (Special.IsShadowGrow && Rule.ShroudRate != 0 && Scen.ShroudTimer == 0) {
		Scen.ShroudTimer = TICKS_PER_MINUTE * Rule.ShroudRate;
		Map.Encroach_Shadow();
	}

	/*
	**	Team AI is processed.
	*/
	for (index = 0; index < Teams.Count(); index++) {
		Teams.Ptr(index)->AI();
	}

	/*
	** If there's a time quake, handle it here.
	*/
	if (TimeQuake) {
		Sound_Effect(VOC_KABOOM15);
		Shake_The_Screen(8);
	}

	ChronalVortex.AI();

	/*
	**	Random skirmish events -- paratroopers, air strikes, etc.
	*/
	if (Session.Type == GAME_SKIRMISH) {
		if (NextRandomEventTime == 0) {
			Schedule_Next_Random_Event();
		} else if (SDL_GetTicks() >= NextRandomEventTime) {
			Do_Random_Skirmish_Event();
			Schedule_Next_Random_Event();
		}
	}

	/*
	**	AI for all sentient objects is processed.
	*/
	for (index = 0; index < Count(); index++) {
		ObjectClass * obj = (*this)[index];

		BStart(BENCH_AI);
		obj->AI();
		BEnd(BENCH_AI);

		if (TimeQuake && obj != NULL && obj->IsActive && !obj->IsInLimbo && obj->Strength) {
			int damage = obj->Class_Of().MaxStrength * Rule.QuakeDamagePercent;
#ifdef FIXIT_CSII	//	checked - ajw 9/28/98
			if (TimeQuakeCenter) {
				if(::Distance(obj->As_Target(),TimeQuakeCenter)/256 < MTankDistance) {
					switch(obj->What_Am_I()) {
						case RTTI_INFANTRY:
							damage = QuakeInfantryDamage;
							break;
						case RTTI_BUILDING:
							damage = QuakeBuildingDamage * obj->Class_Of().MaxStrength;
							break;
						default:
							damage = QuakeUnitDamage * obj->Class_Of().MaxStrength;
							break;
					}
					if (damage) {
						obj->Clicked_As_Target();
						new AnimClass(ANIM_MINE_EXP1, obj->Center_Coord());
					}
					obj->Take_Damage(damage, 0, WARHEAD_AP, 0, true);
				}
			} else {
				obj->Take_Damage(damage, 0, WARHEAD_AP, 0, true);
			}
#else
			obj->Take_Damage(damage, 0, WARHEAD_AP, 0, true);
#endif
		}
		/*
		**	If the object was destroyed in the process of performing its AI, then
		**	adjust the index so that no object gets skipped.
		*/
		if (obj != (*this)[index]) {
			index--;
		}
	}
	HouseClass::Recalc_Attributes();

	/*
	**	Map related logic is performed.
	*/
	Map.Logic();

	/*
	**	Factory processing is performed.
	*/
	for (index = 0; index < Factories.Count(); index++) {
		Factories.Ptr(index)->AI();
	}

	/*
	**	House processing is performed.
	*/
#ifdef FIXIT_VERSION_3
	if( Session.Type != GAME_NORMAL )
	{
		for (HousesType house = HOUSE_MULTI1; house < HOUSE_COUNT; house++) {
			HouseClass * hptr = HouseClass::As_Pointer(house);
			if (hptr && hptr->IsActive) {
				hptr->AI();
			}
		}
	}
	else
	{
		for (HousesType house = HOUSE_FIRST; house < HOUSE_COUNT; house++) {
			HouseClass * hptr = HouseClass::As_Pointer(house);
			if (hptr && hptr->IsActive) {
				hptr->AI();
			}
		}
	}
#else	//	AI() is called redundantly 12 times in multiplayer games here. ajw
	for (HousesType house = HOUSE_FIRST; house < HOUSE_COUNT; house++) {
		HouseClass * hptr = HouseClass::As_Pointer(house);
		if (hptr && hptr->IsActive) {
			hptr->AI();
		}
	}
#endif

#ifdef FIXIT_VERSION_3			//	For endgame auto-sonar pulse.
	if( Session.Type != GAME_NORMAL && Scen.AutoSonarTimer == 0 )
	{
		if( bAutoSonarPulse )
		{
			Map.Activate_Pulse();
			Sound_Effect(VOC_SONAR);
			bAutoSonarPulse = false;
		}
#define AUTOSONAR_PERIOD	TICKS_PER_SECOND * 40;
		Scen.AutoSonarTimer = AUTOSONAR_PERIOD;
	}
#endif
}


/***********************************************************************************************
 * LogicClass::Detach -- Detatch the specified target from the logic system.                   *
 *                                                                                             *
 *    This routine is called when the specified target object is about to be removed from the  *
 *    game system and all references to it must be severed. The only thing that the logic      *
 *    system looks for in this case is to see if the target refers to a trigger and if so,     *
 *    it scans through the trigger list and removes all references to it.                      *
 *                                                                                             *
 * INPUT:   target   -- The target to remove from the sytem.                                   *
 *                                                                                             *
 * OUTPUT:  none                                                                               *
 *                                                                                             *
 * WARNINGS:   none                                                                            *
 *                                                                                             *
 * HISTORY:                                                                                    *
 *   07/30/1996 JLB : Created.                                                                 *
 *=============================================================================================*/
void LogicClass::Detach(TARGET target, bool )
{
	/*
	**	Remove any triggers from the logic trigger list.
	*/
	if (Is_Target_Trigger(target)) {
		for (int index = 0; index < LogicTriggers.Count(); index++) {
			if (As_Trigger(target) == LogicTriggers[index]) {
				LogicTriggers.Delete(index);
				index--;
			}
		}
	}
}

