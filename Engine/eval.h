#include "movegen.h"
#include <iostream>

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

// |---------------------|
// | PV, killer, history |-----------------------------------------------------
// |---------------------|

uint16_t PV_table[MAX_PLY][MAX_PLY] = {0};
int PV_length[MAX_PLY] = {0};

struct KillerMovePair {
    uint16_t one = 0;
    uint16_t two = 0;
};

KillerMovePair killer_moves[MAX_PLY];

// [source][target]
uint16_t history_moves[64][64] = {0};

// |---------------|
// | Move Ordering |-----------------------------------------------------------
// |---------------|

// [attacker][victim] ~ [source piece][target piece]
int mvv_lva[6][6] = {
    {105, 325, 335, 505, 905, 0}, // By pawn
    {104, 324, 334, 504, 904, 0}, // By knight
    {103, 323, 333, 503, 903, 0}, // By bishop
    {102, 322, 332, 502, 902, 0}, // By rook
    {101, 321, 332, 501, 901, 0}, // By queen
    {100, 320, 300, 500, 900, 0} // By king
};

int ScoreMove(uint16_t move, int ply){
    int source = move & 0b0000000000111111;
    int target = (move & 0b0000111111000000) >> 6;
    int flag = (move & 0b1111000000000000) >> 12;

    int score = 0;

    // Non-EP captures (MVV-LVA)
    if(flag == 4 || flag >= 12){
        Piece source_piece = board.PieceAtSquare(source, board.to_move);
        Piece target_piece = board.PieceAtSquare(target, static_cast<Colour>(!board.to_move));

        score += mvv_lva[source_piece][target_piece];

        // Extra points if the capture is a promotion
        switch(flag){
            case MoveFlag::capture_knight_promo: { score += KNIGHT_VALUE_CTP - PAWN_VALUE_CTP; break; }
            case MoveFlag::capture_bishop_promo: { score += BISHOP_VALUE_CTP - PAWN_VALUE_CTP; break; }
            case MoveFlag::capture_rook_promo: { score += ROOK_VALUE_CTP - PAWN_VALUE_CTP; break; }
            case MoveFlag::capture_queen_promo: { score += QUEEN_VALUE_CTP - PAWN_VALUE_CTP;  break; }
            default: { break; }
        }

        // Bonus for captures
        score += 5000;
    }

    // Quiet promotions
    else if(8 <= flag && flag <= 11){
        switch(flag){
            case MoveFlag::quiet_knight_promo: { score += KNIGHT_VALUE_CTP - PAWN_VALUE_CTP; break; }
            case MoveFlag::quiet_bishop_promo: { score += BISHOP_VALUE_CTP - PAWN_VALUE_CTP; break; }
            case MoveFlag::quiet_rook_promo: { score += ROOK_VALUE_CTP - PAWN_VALUE_CTP; break; }
            case MoveFlag::quiet_queen_promo: { score += QUEEN_VALUE_CTP - PAWN_VALUE_CTP;  break; }
            default: { break; }
        }

        // Bonus for promotions
        score += 5000;
    }

    // Quiet moves (killers, history)
    else if(flag <= 3){
        // Killers get a slightly smaller bonus than captures
        if(move == killer_moves[ply].one){ score += 4000; }
        else if(move == killer_moves[ply].two){ score += 3500; }

        // History
        else{
            score += history_moves[source][target];
        }
    }

    // EP also gets the same bonus as captures
    else{ score += 5000; }

    return score;
}

void ScoreMoveList(MoveList& list, uint16_t best_move, int ply){
    for(int i = 0; i < list.count; i++){
        list.score_list[i] = ScoreMove(list.list[i], ply);

        // A huge bonus for the best move (taken from the TT)
        if(list.list[i] == best_move){ list.score_list[i] += 10000; }
    }
}

void ScoreQuiescenceMoveList(MoveList& list, int ply){
    for(int i = 0; i < list.count; i++){
        list.score_list[i] = ScoreMove(list.list[i], ply);
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

// |---------------------------------|
// | Engine Properties and Functions |-----------------------------------------
// |---------------------------------|

u64 nodes_searched = 0;

class Engine {
public:
    int search_depth;

    int transposition_table_size_MB;

    int Search(int depth, int alpha, int beta, int ply, bool on_PV){
        const int original_alpha = alpha;
        bool found_PV = false;
        PV_length[ply] = ply;

        // If this position is in TT, handle returning the stored score
        TEntry& info = TT.GetEntry(board.hash_key);
        bool TT_match = (info.hash_key == board.hash_key);
        if(TT_match && info.depth >= depth && !on_PV){
            int stored_score = info.score;
            
            // Denormalise depth to mate
            if(stored_score > CHECKMATE_THRESHOLD){ stored_score -= ply; }
            else if(stored_score < -CHECKMATE_THRESHOLD){ stored_score += ply; }

            if(info.flag == TEntryFlag::exact){ return stored_score; }
            if(info.flag == TEntryFlag::LB && stored_score >= beta){ return stored_score; }
            if(info.flag == TEntryFlag::UB && stored_score <= alpha){ return stored_score; }
            if(alpha >= beta){ return stored_score; }
        }

        // Leaf node - hand over to Quiescence
        if(depth == 0){
            nodes_searched++;
            return Quiescence(alpha, beta, ply);
        }

        int score = 0;
        int best_score = -INFTY;
        uint16_t best_move = 0;
        TEntry entry;
        bool legal_moves = false;

        MoveList list; GeneratePseudoLegalMoves(list);

        // Move scoring
        if(on_PV){ ScoreMoveList(list, PV_table[0][ply], ply); }
        else if(TT_match && info.flag != TEntryFlag::UB){ ScoreMoveList(list, info.best_move, ply); }
        else{ ScoreMoveList(list, 0, ply); }

        for(int i = 0; i < list.count; i++){
            
            // Ongoing move ordering
            PrepareBestMove(list, i);

            UnmakeMoveGameState irr_info = board.MakeMove(list.list[i], board.to_move);
            if(board.InCheck(static_cast<Colour>(!board.to_move))){ board.UnmakeMove(list.list[i], board.to_move, irr_info); continue; }
            legal_moves = true;

            // Are we on the PV, and is this move the PV move
            bool child_on_PV = on_PV && (list.list[i] == PV_table[0][ply]);

            // (PVS) If found a move valued between alpha and beta, tighten the window to 1 CTP
            if(found_PV){
                score = -Search(depth - 1, -alpha - 1, -alpha, ply + 1, child_on_PV);

                // If it fails, re-search as usual
                if(score > alpha && score < beta){ score = -Search(depth - 1, -beta, -alpha, ply + 1, child_on_PV); }
            }

            // Otherwise, proceed as usual
            else{ score = -Search(depth - 1, -beta, -alpha, ply + 1, child_on_PV); }

            board.UnmakeMove(list.list[i], board.to_move, irr_info);

            // Better move
            if(score > best_score){
                best_score = score; best_move = list.list[i];

                if(score > alpha){
                    alpha = score; found_PV = true;
                    
                    PV_table[ply][ply] = list.list[i];
                    for(int j = ply + 1; j < PV_length[ply + 1]; j++){ PV_table[ply][j] = PV_table[ply + 1][j]; }
                    PV_length[ply] = PV_length[ply + 1];
                }
            }

            // Beta cutoff (fail-high)
            if(best_score >= beta){
                // Quiet move - insert killer move, update history table
                if( ((list.list[i] & 0b1111000000000000) >> 12) <= 3 ){
                    killer_moves[ply].two = killer_moves[ply].one;
                    killer_moves[ply].one = list.list[i];
                
                    int source = list.list[i] & 0b0000000000111111;
                    int target = (list.list[i] & 0b0000111111000000) >> 6;
                    history_moves[source][target] = std::min(history_moves[source][target] + depth, 3000);
                }
    
                break;
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
        int static_eval = (board.to_move == Colour::white ? eval.StaticEvaluation() : -eval.StaticEvaluation());

        int best_score = static_eval;
        if(best_score >= beta){ return best_score; }
        if(best_score > alpha){ alpha = best_score; }

        MoveList list; GeneratePseudoLegalMoves(list); FilterCapturesAndPromotions(list); ScoreQuiescenceMoveList(list, ply);
        for(int i = 0; i < list.count; i++){
            nodes_searched++;
            PrepareBestMove(list, i);

            // If the move is not a pawn promotion, apply delta pruning
            if( ((list.list[i] & 0b1111000000000000) >> 12) < 8 ){
                int target_square = (list.list[i] & 0b0000111111000000) >> 6;
                Piece target_piece = board.PieceAtSquare(target_square, board.to_move);
                int target_value = PieceValue(target_piece);
                
                if(static_eval + target_value + DELTA < alpha){ continue; }
            }

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

    void IterativeSearch(int depth){
        for(int iteration_depth = 1; iteration_depth <= depth; iteration_depth++){
            int s = Search(iteration_depth, -INFTY, INFTY, 0, true);
            std::cout << "Iteration " << iteration_depth << ": " << s << " | ";
            search_age++;
        }

        std::cout << "\n";
    }

    void PrintPVToTerminal(){
        std::cout << "\nPrincipal Variation:\n";
        for(int i = 0; i < PV_length[0]; i++){
            std::cout << "ply " << i << ": (";
            PrintMoveToTerminal(PV_table[0][i]);
            std::cout << ")\n";
        }
    }
private:
    int search_age = 0;
};

Engine engine;