/*
  Stockfish, a UCI chess playing engine derived from Glaurung 2.1
  Copyright (C) 2004-2008 Tord Romstad (Glaurung author)
  Copyright (C) 2008-2015 Marco Costalba, Joona Kiiski, Tord Romstad
  Copyright (C) 2015-2016 Marco Costalba, Joona Kiiski, Gary Linscott, Tord Romstad

  Stockfish is free software: you can redistribute it and/or modify
  it under the terms of the GNU General Public License as published by
  the Free Software Foundation, either version 3 of the License, or
  (at your option) any later version.

  Stockfish is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/


#include <atomic>
#include <bitset>
#include <condition_variable>
#include <mutex>
#include <thread>
#include <vector>

#include "material.h"
#include "movepick.h"
#include "pawns.h"
#include "position.h"
#include "search.h"



class WorkUnit {



public:
	WorkUnit();
//  virtual void search();
//  void idle_loop();
//  void start_searching(bool resume = false);
//  void wait_for_search_finished();
//  void wait(std::atomic_bool& b);

  Pawns::Table pawnsTable;
  Material::Table materialTable;
  Endgames endgames;
  size_t idx,PVIdx;
  int maxPly, callsCnt;

  Position rootPos;
  Search::RootMoveVector rootMoves;
  Depth rootDepth;
  HistoryStats history;
  MoveStats counterMoves;
  Depth completedDepth;
};




struct WorkUnitPool : public std::vector<WorkUnit*> {

  void init(); // No constructor and destructor, threads rely on globals that should
  void exit(); // be initialized and valid during the whole thread lifetime.

  WorkUnit* first() { return static_cast<WorkUnit*>(at(0)); }

  //void start_thinking(const Position&, const Search::LimitsType&, Search::StateStackPtr&);
  void read_uci_options();
  int64_t nodes_searched();
};

extern WorkUnitPool WorkUnits;
