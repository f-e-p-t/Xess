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

// |---------------|
// | Move Ordering |-----------------------------------------------------------
// |---------------|

// [attacker][victim] ~ [source][target]
int mvv_lva[6][6] = {
    {105, 325, 335, 505, 905, 0}, // By pawn
    {104, 324, 334, 504, 904, 0}, // By knight
    {103, 323, 333, 503, 903, 0}, // By bishop
    {102, 322, 332, 502, 902, 0}, // By rook
    {101, 321, 332, 501, 901, 0}, // By queen
    {100, 320, 300, 500, 900, 0} // By king
};

int ScoreMove(uint16_t move){
    int source = move & 0b0000000000111111;
    int target = (move & 0b0000111111000000) >> 6;
    int flag = (move & 0b1111000000000000) >> 12;

    int score = 0;

    // Captures (MVV-LVA)
    if(flag == 4 || flag >= 12){
        Piece source_piece = board.PieceAtSquare(source, board.to_move);
        Piece target_piece = board.PieceAtSquare(target, static_cast<Colour>(!board.to_move));

        score += mvv_lva[source_piece][target_piece];
    }

    return score;
}

void ScoreMoveList(MoveList& list, uint16_t best_move){
    for(int i = 0; i < list.count; i++){
        list.score_list[i] = ScoreMove(list.list[i]);

        // A huge bonus for the best move (taken from the TT)
        if(list.list[i] == best_move){ list.score_list[i] += 10000; }
    }
}

void ScoreQuiescenceMoveList(MoveList& list){
    for(int i = 0; i < list.count; i++){
        list.score_list[i] = ScoreMove(list.list[i]);
    }
}

void PrepareBestMove(MoveList& list, int index){
    int best_index = index;
    
    for(int i = index + 1; i < list.count; i++){
        if(list.score_list[i] > list.score_list[best_index]){ best_index = i; }
    }

    std::swap(list.list[index], list.list[best_index]);
    std::swap(list.score_list[index], list.score_list[best_index]);
}

// |---------------------|
// | Triangular PV Table |-----------------------------------------------------
// |---------------------|

uint16_t PV_table[MAX_PLY][MAX_PLY] = {0};
int PV_length[MAX_PLY] = {0};

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

        // Leaf node - hand over to Quiescence
        if(depth == 0){
            //return (board.to_move == Colour::white ? eval.StaticEvaluation() : -eval.StaticEvaluation());
            PV_length[ply] = ply;
            return Quiescence(alpha, beta, ply);
        }

        int score = 0;
        int best_score = -INFTY;
        uint16_t best_move = 0;
        TEntry entry;
        bool legal_moves = false;

        MoveList list; GeneratePseudoLegalMoves(list); ScoreMoveList(list, info.best_move);
        for(int i = 0; i < list.count; i++){
            
            // Ongoing move ordering
            PrepareBestMove(list, i);

            UnmakeMoveGameState irr_info = board.MakeMove(list.list[i], board.to_move);
            if(board.InCheck(static_cast<Colour>(!board.to_move))){ board.UnmakeMove(list.list[i], board.to_move, irr_info); continue; }
            legal_moves = true;
            score = -Search(depth - 1, -beta, -alpha, ply + 1);
            board.UnmakeMove(list.list[i], board.to_move, irr_info);

            // Better move - update the trackers
            if(score > best_score){ best_score = score; best_move = list.list[i]; }

            // Beta cutoff (fail-high)
            if(best_score >= beta){ break; }

            // No beta cutoff - raise alpha and fill PV table
            if(score > alpha){ alpha = score;
            
                PV_table[ply][ply] = list.list[i];
                for(int i = ply + 1; i < PV_length[ply + 1]; i++){
                    PV_table[ply][i] = PV_table[ply + 1][i];
                }
                PV_length[ply] = PV_length[ply + 1];
            }
        }

        // Checkmate and stalemate
        if(!legal_moves){ PV_length[ply] = ply; best_score = (board.InCheck(board.to_move) ? -CHECKMATE + ply : STALEMATE); }

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
        //std::cout << ply << " ";
        nodes++;
        int static_eval = (board.to_move == Colour::white ? eval.StaticEvaluation() : -eval.StaticEvaluation());

        int best_score = static_eval;
        if(best_score >= beta){ return best_score; }
        if(best_score > alpha){ alpha = best_score; }

        MoveList list; GeneratePseudoLegalMoves(list); FilterCapturesAndPromotions(list); ScoreQuiescenceMoveList(list);
        for(int i = 0; i < list.count; i++){
            PrepareBestMove(list, i);

            // If the move is not a pawn promotion, apply delta pruning
            if( ((list.list[i] & 0b1111000000000000) >> 12) < 8 ){
                int target_square = (list.list[i] & 0b0000111111000000) >> 6;
                Piece target_piece = board.PieceAtSquare(target_square, board.to_move);
                int target_value = PieceValue(target_piece);
                
                if(static_eval + target_value + DELTA < alpha){ nodes++; continue; }
            }

            //((list.list[i] & 0b1111000000000000) >> 12) < 8;
            //int target_square = (list.list[i] & 0b0000111111000000) >> 6;
            //Piece target_piece = board.PieceAtSquare(target_square, board.to_move);

            UnmakeMoveGameState irr_info = board.MakeMove(list.list[i], board.to_move);
            if(board.InCheck(static_cast<Colour>(!board.to_move))){ board.UnmakeMove(list.list[i], board.to_move, irr_info); continue; }
            int score = -Quiescence(-beta, -alpha, ply + 1);
            board.UnmakeMove(list.list[i], board.to_move, irr_info);

            if(score > best_score){ best_score = score; }
            if(score >= beta){ return score; }
            if(score > alpha){ alpha = score; }
        }

        return best_score;
    }

    int IterativeSearch(int depth){
        for(int d = 1; d < depth; d++){
            Search(d, -INFTY, INFTY, 0);
            search_age++;
        }
        
        int score = Search(depth, -INFTY, INFTY, 0);
        search_age++;

        return score;
    }

    void PrintPVLine(){
        for(int i = 0; i < PV_length[0]; i++){
            PrintMoveToTerminal(PV_table[0][i]);
        }
    }
private:
    int search_age = 0;
};

Engine engine;