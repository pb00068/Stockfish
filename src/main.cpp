/*
  Stockfish, a UCI chess playing engine derived from Glaurung 2.1
  Copyright (C) 2004-2008 Tord Romstad (Glaurung author)
  Copyright (C) 2008-2015 Marco Costalba, Joona Kiiski, Tord Romstad

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

#include <iostream>

#include "bitboard.h"
#include "evaluate.h"
#include "position.h"
#include "search.h"
#include "thread.h"
#include "tt.h"
#include "uci.h"
#include "syzygy/tbprobe.h"

const int halfDensityMa[][9] =
 {
    {2, 0, 1},
	{2, 1, 0},

	{4, 0, 0, 1, 1},
	{4, 0, 1, 1, 0},
	{4, 1, 1, 0, 0},
	{4, 1, 0, 0, 1},

	{6, 0, 0, 0, 1, 1, 1},
	{6, 0, 0, 1, 1, 1, 0},
	{6, 0, 1, 1, 1, 0, 0},
	{6, 1, 1, 1, 0, 0, 0},
	{6, 1, 1, 0, 0, 0, 1},
	{6, 1, 0, 0, 0, 1, 1},

	{8, 0, 0, 0, 0, 1, 1, 1, 1},
	{8, 0, 0, 0, 1, 1, 1, 1, 0},
	{8, 0, 0, 1, 1, 1, 1, 0 ,0},
	{8, 0, 1, 1, 1, 1, 0, 0 ,0},
	{8, 1, 1, 1, 1, 0, 0, 0 ,0},
	{8, 1, 1, 1, 0, 0, 0, 0 ,1},
	{8, 1, 1, 0, 0, 0, 0, 1 ,1},
	{8, 1, 0, 0, 0, 0, 1, 1 ,1},
 };

int main(int argc, char* argv[]) {

  std::cout << engine_info() << std::endl;


  for (int p=0; p<20; p++) {
	  for (size_t idx=1; idx<21; idx++) {
		  int row = (idx - 1) % 20;
		  std::cout << halfDensityMa[row][p % halfDensityMa[row][0] + 1] << " ";
	  }
	  std::cout << std::endl;
  }

  UCI::init(Options);
  PSQT::init();
  Bitboards::init();
  Position::init();
  Bitbases::init();
  Search::init();
  Eval::init();
  Pawns::init();
  Threads.init();
  Tablebases::init(Options["SyzygyPath"]);
  TT.resize(Options["Hash"]);

  UCI::loop(argc, argv);

  Threads.exit();
  return 0;
}
