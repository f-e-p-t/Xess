#include "movegen.h"
#include <iostream>

// Temporary measure
uint16_t best_move;

// |------------|
// | Evaluation |--------------------------------------------------------------
// |------------|

class Evaluation {
public:
    int Material(){
        int mat = 0;
        
        mat += PAWN_VALUE_CTP * NumberOfNonZeroBits(board.pieces[Colour::white][Piece::pawn]);
        mat += KNIGHT_VALUE_CTP * NumberOfNonZeroBits(board.pieces[Colour::white][Piece::knight]);
        mat += BISHOP_VALUE_CTP * NumberOfNonZeroBits(board.pieces[Colour::white][Piece::bishop]);
        mat += ROOK_VALUE_CTP * NumberOfNonZeroBits(board.pieces[Colour::white][Piece::rook]);
        mat += QUEEN_VALUE_CTP * NumberOfNonZeroBits(board.pieces[Colour::white][Piece::queen]);
        
        mat -= PAWN_VALUE_CTP * NumberOfNonZeroBits(board.pieces[Colour::black][Piece::pawn]);
        mat -= KNIGHT_VALUE_CTP * NumberOfNonZeroBits(board.pieces[Colour::black][Piece::knight]);
        mat -= BISHOP_VALUE_CTP * NumberOfNonZeroBits(board.pieces[Colour::black][Piece::bishop]);
        mat -= ROOK_VALUE_CTP * NumberOfNonZeroBits(board.pieces[Colour::black][Piece::rook]);
        mat -= QUEEN_VALUE_CTP * NumberOfNonZeroBits(board.pieces[Colour::black][Piece::queen]);
    
        return mat;
    }

    int StaticEvaluation(){
        int val = Material();

        return val;
    }
private:

};

Evaluation eval;

// |---------------------------------|
// | Engine Properties and Functions |-----------------------------------------
// |---------------------------------|

class Engine {
public:
    int search_depth;

    int transposition_table_size_MB;

    int Search(int depth, int alpha, int beta){
        if(depth == 0){
            nodes++;

            return (board.to_move == Colour::white ? eval.StaticEvaluation() : -eval.StaticEvaluation());
        }

        bool legal_moves = false;
        MoveList list; GeneratePseudoLegalMoves(list);
        for(int i = 0; i < list.count; i++){
            UnmakeMoveGameState IrrInfo = board.MakeMove(list.list[i], board.to_move);
            if(board.InCheck(static_cast<Colour>(!board.to_move))){ board.UnmakeMove(list.list[i], board.to_move, IrrInfo); continue; }
            legal_moves = true;
            int evaluation = -Search(depth - 1, -beta, -alpha);
            board.UnmakeMove(list.list[i], board.to_move, IrrInfo);
            // VVV best_move is a temporary measure
            if(evaluation > alpha && depth == search_depth){ best_move = list.list[i]; }
            // ^^^ temporary measure
            alpha = std::max(alpha, evaluation);
            if(alpha >= beta){ return alpha; }
        }

        if(!legal_moves){ return (board.InCheck(board.to_move) ? -CHECKMATE + depth :  STALEMATE); }

        return alpha;
    }
private:

};

Engine engine;