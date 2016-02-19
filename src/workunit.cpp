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

#include <algorithm> // For std::count
#include <cassert>

#include "movegen.h"
#include "search.h"
#include "workunit.h"
#include "uci.h"

using namespace Search;

WorkUnitPool WorkUnits; // Global object

/// Thread constructor launches the thread and then waits until it goes to sleep
/// in idle_loop().

WorkUnit::WorkUnit() {
  idx = WorkUnits.size();
  maxPly = callsCnt = 0;
  history.clear();
  counterMoves.clear();
}






/// ThreadPool::init() creates and launches requested threads that will go
/// immediately to sleep. We cannot use a constructor because Threads is a
/// static object and we need a fully initialized engine at this point due to
/// allocation of Endgames in the Thread constructor.

void WorkUnitPool::init() {

  push_back(new WorkUnit);
  read_uci_options();
}


/// ThreadPool::exit() terminates threads before the program exits. Cannot be
/// done in destructor because threads must be terminated before deleting any
/// static objects while still in main().

void WorkUnitPool::exit() {

  while (size())
      delete back(), pop_back();
}


/// ThreadPool::read_uci_options() updates internal threads parameters from the
/// corresponding UCI options and creates/destroys threads to match requested
/// number. Thread objects are dynamically allocated.

void WorkUnitPool::read_uci_options() {

  size_t requested = Options["Threads"];
  size_t overload  = 1;  //Options["Overload"];
  assert(requested > 0);

  while (size() < requested + overload)
      push_back(new WorkUnit);

  while (size() > requested + overload)
      delete back(), pop_back();
}


/// ThreadPool::nodes_searched() returns the number of nodes searched

int64_t WorkUnitPool::nodes_searched() {

  int64_t nodes = 0;
  for (WorkUnit* wu : *this)
      nodes += wu->rootPos.nodes_searched();
  return nodes;
}
