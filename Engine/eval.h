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

    int Search(int depth, int alpha, int beta, int ply){
        const int original_alpha = alpha;

        // If this position is in TT, handle returning the stored score
        TEntry& info = TT.GetEntry(board.hash_key);
        if(info.hash_key == board.hash_key && info.depth >= depth){
            int stored_score = info.score;
            
            // Denormalise depth to mate
            if(stored_score > CHECKMATE_THRESHOLD){ stored_score -= ply; }
            else if(stored_score < -CHECKMATE_THRESHOLD){stored_score += ply; }

            if(info.flag == TEntryFlag::exact){ return stored_score; }
            if(info.flag == TEntryFlag::LB && stored_score >= beta){ return stored_score; }
            if(info.flag == TEntryFlag::UB && stored_score <= alpha){ return stored_score; }
            if(alpha >= beta){ return stored_score; }
        }

        // Leaf node - return the position's evaluation
        if(depth == 0){
            nodes++;
            // Eventually implement quiescence, and put data into the TTable here with a special depth -1 to signal 
            // that the position was evaluated by the quiescence search
            //return (board.to_move == Colour::white ? eval.StaticEvaluation() : -eval.StaticEvaluation());
            return Quiescence();
        }

        int score = 0;
        int best_score = -INFTY;
        uint16_t best_move = 0;
        TEntry entry;
        bool legal_moves = false;

        MoveList list; GeneratePseudoLegalMoves(list);
        for(int i = 0; i < list.count; i++){
            UnmakeMoveGameState irr_info = board.MakeMove(list.list[i], board.to_move);
            if(board.InCheck(static_cast<Colour>(!board.to_move))){ board.UnmakeMove(list.list[i], board.to_move, irr_info); continue; }
            legal_moves = true;
            score = -Search(depth - 1, -beta, -alpha, ply + 1);
            board.UnmakeMove(list.list[i], board.to_move, irr_info);

            // Found a better move - raise best_score
            if(score > best_score){ 
                best_score = score;
                best_move = list.list[i];

                // VVV TEMPORARY MEASURE
                if(depth == search_depth){ best_move_temp = list.list[i]; }

                // Only set best_move if score beats alpha
                if(score > alpha){ alpha = score; }
            }

            // Beta cutoff
            if(best_score >= beta){
                break;
            }
        }

        // Checkmate and stalemate
        if(!legal_moves){ best_score = (board.InCheck(board.to_move) ? -CHECKMATE + ply : STALEMATE); }

        // Normalise depth to mate before inserting into TT
        int TT_score = best_score;
        if(TT_score > CHECKMATE_THRESHOLD){ TT_score += ply; }
        else if(TT_score < -CHECKMATE_THRESHOLD){ TT_score -= ply; }
        entry.score = TT_score;

        // Prepare this position's TT entry
        entry.hash_key = board.hash_key; entry.age = search_age; entry.depth = depth; entry.best_move = best_move;
        if(TT_score <= original_alpha){ entry.flag = TEntryFlag::UB; }
        else if(TT_score >= beta){ entry.flag = TEntryFlag::LB; }
        else{ entry.flag = TEntryFlag::exact; }
        
        // Insert if appropriate
        if(TT.AppropriateToOverwrite(info, entry)){ TT.SetEntry(entry, board.hash_key); }

        return best_score;
    }

    int Quiescence(int alpha, int beta, int ply){
        int score = eval.StaticEvaluation();
    }
private:
    int search_age = 0;
};

Engine engine;