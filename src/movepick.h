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

#ifndef MOVEPICK_H_INCLUDED
#define MOVEPICK_H_INCLUDED

#include <algorithm> // For std::max
#include <cstring>   // For std::memset

#include "movegen.h"
#include "position.h"
#include "search.h"
#include "types.h"


/// The Stats struct stores moves statistics. According to the template parameter
/// the class can store History and Countermoves. History records how often
/// different moves have been successful or unsuccessful during the current search
/// and is used for reduction and move ordering decisions.
/// Countermoves store the move that refute a previous one. Entries are stored
/// using only the moving piece and destination square, hence two moves with
/// different origin but same destination and piece will be considered identical.
template<typename T, bool CM = false>
struct Stats {

  static const Value Max = Value(1 << 28);

  const T* operator[](Piece pc) const { return table[pc % PIECE_NB][pc > PIECE_NB]; }
  T* operator[](Piece pc) { return table[pc % PIECE_NB][pc > PIECE_NB]; }
  void clear() { std::memset(table, 0, sizeof(table)); }

  void update(Piece pc, Square to, Move m) {

    if (m != table[pc][0][to])
        table[pc][0][to] = m;
  }

  void update(Piece pc, Square to, Value v, int gameply, int ply) {

    if (abs(int(v)) >= 324)
        return;

    BiVal bv = table[pc][0][to];
    int rootPly = gameply - ply;

    if (bv.maxply < rootPly - 10) { // first entry obsolete, shift and reset 2nd entry
      BiVal bv2 = table[pc][1][to];
      bv.value = bv2.value;
      bv.maxply = bv2.maxply;
      bv2.value = VALUE_ZERO;
      bv2.maxply = 0;
      table[pc][0][to] = bv;
    }


    if (bv.maxply == 0 || bv.maxply >= gameply - 100) {
      bv.value -= bv.value * abs(int(v)) / (CM ? 936 : 324);
      bv.value += int(v) * 32;
      bv.maxply = std::max(bv.maxply, gameply);
      table[pc][0][to] = bv;
    }
    else {
      bv = table[pc][1][to];
      bv.value -= bv.value * abs(int(v)) / (CM ? 936 : 324);
      bv.value += int(v) * 32;
      bv.maxply = std::max(bv.maxply, gameply);
      table[pc][1][to] = bv;
    }
  }

private:
  T table[PIECE_NB][2][SQUARE_NB];
};

typedef Stats<Move> MoveStats;
typedef Stats<BiVal, false> HistoryStats;
typedef Stats<BiVal,  true> CounterMoveStats;
typedef Stats<CounterMoveStats> CounterMoveHistoryStats;


/// MovePicker class is used to pick one pseudo legal move at a time from the
/// current position. The most important method is next_move(), which returns a
/// new pseudo legal move each time it is called, until there are no moves left,
/// when MOVE_NONE is returned. In order to improve the efficiency of the alpha
/// beta algorithm, MovePicker attempts to return the moves which are most likely
/// to get a cut-off first.

class MovePicker {
public:
  MovePicker(const MovePicker&) = delete;
  MovePicker& operator=(const MovePicker&) = delete;

  MovePicker(const Position&, Move, Depth, const HistoryStats&, Square);
  MovePicker(const Position&, Move, const HistoryStats&, Value);
  MovePicker(const Position&, Move, Depth, const HistoryStats&, const CounterMoveStats&, Move, Search::Stack*);

  Move next_move();

private:
  template<GenType> void score();
  void generate_next_stage();
  ExtMove* begin() { return moves; }
  ExtMove* end() { return endMoves; }

  const Position& pos;
  const HistoryStats& history;
  const CounterMoveStats* counterMoveHistory;
  Search::Stack* ss;
  Move countermove;
  Depth depth;
  Move ttMove;
  ExtMove killers[3];
  Square recaptureSquare;
  Value threshold;
  int stage;
  ExtMove *endQuiets, *endBadCaptures = moves + MAX_MOVES - 1;
  ExtMove moves[MAX_MOVES], *cur = moves, *endMoves = moves;
};

#endif // #ifndef MOVEPICK_H_INCLUDED
