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

#include <cassert>

#include "movepick.h"
#include "thread.h"

namespace {

  enum Stages {
    MAIN_SEARCH, GOOD_CAPTURES, KILLERS, GOOD_QUIETS, BAD_QUIETS, BAD_CAPTURES,
    EVASION, ALL_EVASIONS,
    QSEARCH_WITH_CHECKS, QCAPTURES_1, CHECKS,
    QSEARCH_WITHOUT_CHECKS, QCAPTURES_2,
    PROBCUT, PROBCUT_CAPTURES,
    RECAPTURE, RECAPTURES,
    STOP
  };

  // Our insertion sort, which is guaranteed to be stable, as it should be
  void insertion_sort(ExtMove* begin, ExtMove* end)
  {
    ExtMove tmp, *p, *q;

    for (p = begin + 1; p < end; ++p)
    {
        tmp = *p;
        for (q = p; q != begin && *(q-1) < tmp; --q)
            *q = *(q-1);
        *q = tmp;
    }
  }

  // pick_best() finds the best move in the range (begin, end) and moves it to
  // the front. It's faster than sorting all the moves in advance when there
  // are few moves, e.g., the possible captures.
  Move pick_best(ExtMove* begin, ExtMove* end)
  {
      std::swap(*begin, *std::max_element(begin, end));
      return *begin;
  }

} // namespace


/// Constructors of the MovePicker class. As arguments we pass information
/// to help it to return the (presumably) good moves first, to decide which
/// moves to return (in the quiescence search, for instance, we only want to
/// search captures, promotions, and some checks) and how important good move
/// ordering is at the current node.

MovePicker::MovePicker(const Position& p, Move ttm, Depth d, Search::Stack* s)
           : pos(p), ss(s), depth(d) {

  assert(d > DEPTH_ZERO);

  Square prevSq = to_sq((ss-1)->currentMove);
  countermove = pos.this_thread()->counterMoves[pos.piece_on(prevSq)][prevSq];

  stage = pos.checkers() ? EVASION : MAIN_SEARCH;
  ttMove = ttm && pos.pseudo_legal(ttm) ? ttm : MOVE_NONE;
  endMoves += (ttMove != MOVE_NONE);
}

MovePicker::MovePicker(const Position& p, Move ttm, Depth d, Square s)
           : pos(p) {

  assert(d <= DEPTH_ZERO);

  if (pos.checkers())
      stage = EVASION;

  else if (d > DEPTH_QS_NO_CHECKS)
      stage = QSEARCH_WITH_CHECKS;

  else if (d > DEPTH_QS_RECAPTURES)
      stage = QSEARCH_WITHOUT_CHECKS;

  else
  {
      stage = RECAPTURE;
      recaptureSquare = s;
      ttm = MOVE_NONE;
  }

  ttMove = ttm && pos.pseudo_legal(ttm) ? ttm : MOVE_NONE;
  endMoves += (ttMove != MOVE_NONE);
}

MovePicker::MovePicker(const Position& p, Move ttm, Value th)
           : pos(p), threshold(th) {

  assert(!pos.checkers());

  stage = PROBCUT;

  // In ProbCut we generate captures with SEE higher than the given threshold
  ttMove =   ttm
          && pos.pseudo_legal(ttm)
          && pos.capture(ttm)
          && pos.see(ttm) > threshold ? ttm : MOVE_NONE;

  endMoves += (ttMove != MOVE_NONE);
}


/// score() assigns a numerical value to each move in a move list. The moves with
/// highest values will be picked first.
template<>
void MovePicker::score<CAPTURES>() {
  // Winning and equal captures in the main search are ordered by MVV, preferring
  // captures near our home rank. Surprisingly, this appears to perform slightly
  // better than SEE-based move ordering: exchanging big pieces before capturing
  // a hanging piece probably helps to reduce the subtree size.
  // In the main search we want to push captures with negative SEE values to the
  // badCaptures[] array, but instead of doing it now we delay until the move
  // has been picked up, saving some SEE calls in case we get a cutoff.
  for (auto& m : *this)
      m.value =  PieceValue[MG][pos.piece_on(to_sq(m))]
               - Value(200 * relative_rank(pos.side_to_move(), to_sq(m)));
}

template<>
void MovePicker::score<QUIETS>() {

  const HistoryStats& history = pos.this_thread()->history;

  const CounterMoveStats* cm = (ss-1)->counterMoves;
  const CounterMoveStats* fm = (ss-2)->counterMoves;
  const CounterMoveStats* f2 = (ss-4)->counterMoves;

  for (auto& m : *this)
      m.value =      history[pos.moved_piece(m)][to_sq(m)]
               + (cm ? (*cm)[pos.moved_piece(m)][to_sq(m)] : VALUE_ZERO)
               + (fm ? (*fm)[pos.moved_piece(m)][to_sq(m)] : VALUE_ZERO)
               + (f2 ? (*f2)[pos.moved_piece(m)][to_sq(m)] : VALUE_ZERO);
}

template<>
void MovePicker::score<EVASIONS>() {
  // Try winning and equal captures ordered by MVV/LVA, then non-captures ordered
  // by history value, then bad captures and quiet moves with a negative SEE ordered
  // by SEE value.
  const HistoryStats& history = pos.this_thread()->history;
  Value see;

  for (auto& m : *this)
      if ((see = pos.see_sign(m)) < VALUE_ZERO)
          m.value = see - HistoryStats::Max; // At the bottom

      else if (pos.capture(m))
          m.value =  PieceValue[MG][pos.piece_on(to_sq(m))]
                   - Value(type_of(pos.moved_piece(m))) + HistoryStats::Max;
      else
          m.value = history[pos.moved_piece(m)][to_sq(m)];
}


/// generate_next_stage() generates, scores, and sorts the next bunch of moves
/// when there are no more moves to try for the current stage.

void MovePicker::generate_next_stage() {

  assert(stage != STOP);

  cur = moves;

  switch (++stage) {

  case GOOD_CAPTURES: case QCAPTURES_1: case QCAPTURES_2:
  case PROBCUT_CAPTURES: case RECAPTURES:
      endMoves = generate<CAPTURES>(pos, moves);
      score<CAPTURES>();
      break;

  case KILLERS:
      killers[0] = ss->killers[0];
      killers[1] = ss->killers[1];
      killers[2] = countermove;
      cur = killers;
      endMoves = cur + 2 + (countermove != killers[0] && countermove != killers[1]);
      break;

  case GOOD_QUIETS:
      endQuiets = endMoves = generate<QUIETS>(pos, moves);
      score<QUIETS>();
      endMoves = std::partition(cur, endMoves, [](const ExtMove& m) { return m.value > VALUE_ZERO; });
      insertion_sort(cur, endMoves);
      break;

  case BAD_QUIETS:
      cur = endMoves;
      endMoves = endQuiets;
      if (depth >= 3 * ONE_PLY)
          insertion_sort(cur, endMoves);
      break;

  case BAD_CAPTURES:
      // Just pick them in reverse order to get correct ordering
      cur = moves + MAX_MOVES - 1;
      endMoves = endBadCaptures;
      break;

  case ALL_EVASIONS:
      endMoves = generate<EVASIONS>(pos, moves);
      if (endMoves - moves > 1)
          score<EVASIONS>();
      break;

  case CHECKS:
      endMoves = generate<QUIET_CHECKS>(pos, moves);
      break;

  case EVASION: case QSEARCH_WITH_CHECKS: case QSEARCH_WITHOUT_CHECKS:
  case PROBCUT: case RECAPTURE: case STOP:
      stage = STOP;
      break;

  default:
      assert(false);
  }
}


/// next_move() is the most important method of the MovePicker class. It returns
/// a new pseudo legal move every time it is called, until there are no more moves
/// left. It picks the move with the biggest value from a list of generated moves
/// taking care not to return the ttMove if it has already been searched.

Move MovePicker::next_move() {

  Move move;

  while (true)
  {
      while (cur == endMoves && stage != STOP)
          generate_next_stage();

      switch (stage) {

      case MAIN_SEARCH: case EVASION: case QSEARCH_WITH_CHECKS:
      case QSEARCH_WITHOUT_CHECKS: case PROBCUT:
          ++cur;
          return ttMove;

      case GOOD_CAPTURES:
               move = pick_best(cur++, endMoves);
               if (move != ttMove)
               {
                   if (PieceValue[MG][pos.moved_piece(move)] <= PieceValue[MG][pos.piece_on(to_sq(move))])
                         return move; // this is like see_sign, legality of the first move is verified in the search
                   if (depth > 3)
                   {
                        Bitboard pinneds[2],pinners[2];
                        for (int i=0; i < 2; i++)
                        {
                           Color c = (Color) i;
                           Bitboard b, result = 0;
                           Square ksq = pos.square<KING>(~c);
                           Bitboard pinner = pinners[c] = (  (pos.pieces(  ROOK, QUEEN) & PseudoAttacks[ROOK][ksq])
                                   | (pos.pieces(BISHOP, QUEEN) & PseudoAttacks[BISHOP][ksq])) & pos.pieces(c);

                           while (pinner)
                           {
                               b = between_bb(ksq, pop_lsb(&pinner)) & pos.pieces();
                               if (!more_than_one(b))
                                   result |= b & pos.pieces(~c);
                           }
                           pinneds[~c] = result;
                        }



                     if (pinneds[0] || pinneds[1])
                     {

                         if (pinners[0] & to_sq(move))
                            pinners[0] ^= to_sq(move);
                         if (pinners[1] & to_sq(move))
                            pinners[1] ^= to_sq(move);
     //                  Value valp = pos.see_pin_aware(move, pinnerz, pinnedz);
     //                  Value val  = pos.see(move);
     //                    //
     //                    //                  if (pos.fen().compare("rn1qkb1r/pppbnppp/8/3p4/8/2N5/PPPPQPPP/R1B1KBNR w KQkq - 4 5") == 0) {
     //                    //                  //if (valp != val) {
     //                    if ((valp >= VALUE_ZERO && val < VALUE_ZERO) || (valp < VALUE_ZERO && val >= VALUE_ZERO) ) {
     //                      sync_cout << pos << " move " << UCI::move(move, false) << " last " << UCI::move((ss-1)->currentMove, false) <<"  seep: " << valp << " see: " << val << sync_endl;
     //                      sync_cout << Bitboards::pretty(between_bb(pos.square<KING>(~pos.side_to_move()), lsb(pinners[pos.side_to_move()])) & to_sq(move)) << sync_endl;
     //                      sync_cout << Bitboards::pretty(pinneds[~pos.side_to_move()]) << sync_endl;
     //                    }
//                         if (pos.see(move) >= VALUE_ZERO)
                         if (pos.see_pin_aware(move, pinners, pinneds) >= VALUE_ZERO)
                           return move;
                      }
                      else if (pos.see(move) >= VALUE_ZERO)
                          return move;
                   }
                   else
                     if (pos.see(move) >= VALUE_ZERO)
                     return move;

                   // Losing capture, move it to the tail of the array
                   *endBadCaptures-- = move;
               }
               break;

      case KILLERS:
          move = *cur++;
          if (    move != MOVE_NONE
              &&  move != ttMove
              &&  pos.pseudo_legal(move)
              && !pos.capture(move))
              return move;
          break;

      case GOOD_QUIETS: case BAD_QUIETS:
          move = *cur++;
          if (   move != ttMove
              && move != killers[0]
              && move != killers[1]
              && move != killers[2])
              return move;
          break;

      case BAD_CAPTURES:
          return *cur--;

      case ALL_EVASIONS: case QCAPTURES_1: case QCAPTURES_2:
          move = pick_best(cur++, endMoves);
          if (move != ttMove)
              return move;
          break;

      case PROBCUT_CAPTURES:
           move = pick_best(cur++, endMoves);
           if (move != ttMove && pos.see(move) > threshold)
               return move;
           break;

      case RECAPTURES:
          move = pick_best(cur++, endMoves);
          if (to_sq(move) == recaptureSquare)
              return move;
          break;

      case CHECKS:
          move = *cur++;
          if (move != ttMove)
              return move;
          break;

      case STOP:
          return MOVE_NONE;

      default:
          assert(false);
      }
  }
}
