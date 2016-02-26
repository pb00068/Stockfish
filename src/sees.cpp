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

#include <algorithm>
#include <cassert>

#include "bitboard.h"
#include "bitcount.h"
#include "position.h"
#include "sees.h"
#include "thread.h"



namespace Sees {

/// Sees::init()

void init()
{

}


/// Sees::probe() looks up the current position's sees entry

Entry* probe(const Position& pos, const Move move, const Piece attacked) {

  Key key = pos.see_key(move, attacked);
  Entry* e = pos.this_thread()->seeTable[key];

  if (e->key == key && e->square == to_sq(move) && e->attacked == attacked)
      return e;


  return nullptr;
}

/// Sees::save() store up the current position's sees entry

void save(const Position& pos, const Move move, const Piece attacked, Value value) {

  Key key = pos.see_key(move, attacked);
  Entry* e = pos.this_thread()->seeTable[key];

  e->key = key;
  e->square = to_sq(move);
  e->attacked = attacked;
  e->value = value;
}

} // namespace Sees
