#include "movegen.h"
#include <iostream>

// Temporary measure
uint16_t best_move_temp;

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
        uint16_t best_move = 0;

        // Leaf node - return the position's evaluation
        if(depth == 0){
            nodes++;
            return (board.to_move == Colour::white ? eval.StaticEvaluation() : -eval.StaticEvaluation());
        }

        // If this position is in TT and certain conditions are met, return the known score
        TEntry& info = TT.GetEntry(board.hash_key);
        if(info.hash_key == board.hash_key && info.depth >= depth){
            if(info.flag == TEntryFlag::exact){ return info.score; }
            if(info.flag == TEntryFlag::LB && info.score >= beta){ return info.score; }
            if(info.flag == TEntryFlag::UB && info.score <= alpha){ return info.score; }
        }

        int score = 0;
        int best_score = -INFTY;
        bool legal_moves = false;

        MoveList list; GeneratePseudoLegalMoves(list);
        for(int i = 0; i < list.count; i++){
            UnmakeMoveGameState irr_info = board.MakeMove(list.list[i], board.to_move);
            if(board.InCheck(static_cast<Colour>(!board.to_move))){ board.UnmakeMove(list.list[i], board.to_move, irr_info); continue; }
            legal_moves = true;
            score = -Search(depth - 1, -beta, -alpha);
            board.UnmakeMove(list.list[i], board.to_move, irr_info);

            // VVV best_move is a temporary measure
            if(score > alpha && depth == search_depth){ best_move_temp = list.list[i]; }
            // ^^^ temporary measure

            // Found a better move - raise best_score
            if(score > best_score){ 
                best_score = score;

                // Only set best_move if score beats alpha
                if(score > alpha){ alpha = score; best_move = list.list[i]; }
            }

            // Beta cutoff
            if(best_score >= beta){
                break;
            }
        }

        if(!legal_moves){ best_score = (board.InCheck(board.to_move) ? -CHECKMATE + depth :  STALEMATE); }

        // Put things in the TT here

        return best_score;
    }
private:
    int search_age = 0;
};

Engine engine;