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

    // Finds game stage (midgame --> endgame) based on non-pawn pieces on the board. N = 1, B = 1, R = 2, Q = 4
    int Stage(){
        int mat = 0;

        mat += NumberOfNonZeroBits(board.pieces[Colour::white][Piece::knight]);
        mat += NumberOfNonZeroBits(board.pieces[Colour::white][Piece::bishop]);
        mat += 2 * NumberOfNonZeroBits(board.pieces[Colour::white][Piece::rook]);
        mat += 4 * NumberOfNonZeroBits(board.pieces[Colour::white][Piece::queen]);
        mat += NumberOfNonZeroBits(board.pieces[Colour::black][Piece::knight]);
        mat += NumberOfNonZeroBits(board.pieces[Colour::black][Piece::bishop]);
        mat += 2 * NumberOfNonZeroBits(board.pieces[Colour::black][Piece::rook]);
        mat += 4 * NumberOfNonZeroBits(board.pieces[Colour::black][Piece::queen]);

        return std::min(mat, STAGE_MAX);
    }

    int HandleStageIndependentHeatmaps(){
        int val = 0;
        int index;
        u64 bitboard;
        
        // Pawns
        bitboard = board.pieces[Colour::white][Piece::pawn];
        while(bitboard){
            index = GetLSBitIndex(bitboard);
            val += Heatmap::pawn[Colour::white][index];
            bitboard &= bitboard - 1;
        }

        bitboard = board.pieces[Colour::black][Piece::pawn];
        while(bitboard){
            index = GetLSBitIndex(bitboard);
            val -= Heatmap::pawn[Colour::black][index];
            bitboard &= bitboard - 1;
        }

        // Knights
        bitboard = board.pieces[Colour::white][Piece::knight];
        while(bitboard){
            index = GetLSBitIndex(bitboard);
            val += Heatmap::knight[Colour::white][index];
            bitboard &= bitboard - 1;
        }

        bitboard = board.pieces[Colour::black][Piece::knight];
        while(bitboard){
            index = GetLSBitIndex(bitboard);
            val -= Heatmap::knight[Colour::black][index];
            bitboard &= bitboard - 1;
        }

        // Bishops
        bitboard = board.pieces[Colour::white][Piece::bishop];
        while(bitboard){
            index = GetLSBitIndex(bitboard);
            val += Heatmap::bishop[Colour::white][index];
            bitboard &= bitboard - 1;
        }

        bitboard = board.pieces[Colour::black][Piece::bishop];
        while(bitboard){
            index = GetLSBitIndex(bitboard);
            val -= Heatmap::bishop[Colour::black][index];
            bitboard &= bitboard - 1;
        }

        // Rooks
        bitboard = board.pieces[Colour::white][Piece::rook];
        while(bitboard){
            index = GetLSBitIndex(bitboard);
            val += Heatmap::rook[Colour::white][index];
            bitboard &= bitboard - 1;
        }

        bitboard = board.pieces[Colour::black][Piece::rook];
        while(bitboard){
            index = GetLSBitIndex(bitboard);
            val -= Heatmap::rook[Colour::black][index];
            bitboard &= bitboard - 1;
        }

        // Queens
        bitboard = board.pieces[Colour::white][Piece::queen];
        while(bitboard){
            index = GetLSBitIndex(bitboard);
            val += Heatmap::queen[Colour::white][index];
            bitboard &= bitboard - 1;
        }

        bitboard = board.pieces[Colour::black][Piece::queen];
        while(bitboard){
            index = GetLSBitIndex(bitboard);
            val -= Heatmap::queen[Colour::black][index];
            bitboard &= bitboard - 1;
        }

        return val;
    }

    int StaticEvaluationMidgameExclusive(){
        int val = 0;
        
        // King heatmaps
        int index;
        u64 bitboard;
        bitboard = board.pieces[Colour::white][Piece::king];
        while(bitboard){
            index = GetLSBitIndex(bitboard);
            val += Heatmap::king[GameStage::midgame][Colour::white][index];
            bitboard &= bitboard - 1;
        }

        bitboard = board.pieces[Colour::black][Piece::king];
        while(bitboard){
            index = GetLSBitIndex(bitboard);
            val -= Heatmap::king[GameStage::midgame][Colour::black][index];
            bitboard &= bitboard - 1;
        }

        return val;
    }

    int StaticEvaluationEndgameExclusive(){
        int val = 0;
        
        // King heatmaps
        int index;
        u64 bitboard;
        bitboard = board.pieces[Colour::white][Piece::king];
        while(bitboard){
            index = GetLSBitIndex(bitboard);
            val += Heatmap::king[GameStage::endgame][Colour::white][index];
            bitboard &= bitboard - 1;
        }

        bitboard = board.pieces[Colour::black][Piece::king];
        while(bitboard){
            index = GetLSBitIndex(bitboard);
            val -= Heatmap::king[GameStage::endgame][Colour::black][index];
            bitboard &= bitboard - 1;
        }

        return val;
    }

    int StaticEvaluation(){
        int stage = Stage();

        int val = Material();

        val += HandleStageIndependentHeatmaps();
        val += (((STAGE_MAX - stage) * StaticEvaluationMidgameExclusive()) + (stage * StaticEvaluationEndgameExclusive())) / STAGE_MAX;

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

// |-----|
// | SEE |---------------------------------------------------------------------
// |-----|

// Static Exchange Evaluation (No: pin detection)
int SEE(uint16_t move){
    int source = move & 0b0000000000111111;
    int target = (move & 0b0000111111000000) >> 6;
    int flag = (move & 0b1111000000000000) >> 12;

    if(flag <= 3){ return 0; } // <-- Redundant safety net

    int prev_attacker_value = 0; Colour to_move = board.to_move; u64 occ_mask = board.total_occ; int exchange[32] = {0}; int d = 0;

    // ------------

    prev_attacker_value = PieceValue(board.PieceAtSquare(source, to_move));

    // If the move was EP
    if(flag == MoveFlag::EP_capture){
        exchange[d] += PAWN_VALUE_CTP;
        int capture_square = target + (to_move == Colour::white ? 8 : -8);
        occ_mask ^= (1ULL << capture_square); occ_mask ^= (1ULL << target);
    }

    // If the move was a promotion
    else if(flag >= 8){
        exchange[d] += (piece_promotion_value_by_flag[flag] - PAWN_VALUE_CTP);
        prev_attacker_value = piece_promotion_value_by_flag[flag];
        if(flag <= 11){ occ_mask ^= (1ULL << target); } // <-- If the promotion is quiet, toggle on the empty target square
        else{ exchange[d] += PieceValue(board.PieceAtSquare(target, static_cast<Colour>(!board.to_move))); }
    }

    // Otherwise (normal captures)
    else{
        exchange[d] += PieceValue(board.PieceAtSquare(target, static_cast<Colour>(!board.to_move)));
    }
    occ_mask ^= (1ULL << source);
    to_move = static_cast<Colour>(!to_move);

    // ------------

    // Complete the exchange
    while(true){
        AttackerInfo info = board.LeastValuableAttackerInMask(occ_mask, target, to_move);
        if(info.source == Square::NO_SQUARE){ break; } // <-- PICK UP BUGFIXING FROM HERE

        // Break if this capture would put the king in check (king is most valuable, so this would be white's last capture)
        if(info.piece == Piece::king){
            u64 new_occ_mask = occ_mask ^ (1ULL << info.source);
            AttackerInfo other_info = board.LeastValuableAttackerInMask(new_occ_mask, target, static_cast<Colour>(!to_move));
            if(other_info.piece != Piece::NO_PIECE){ break; }
        }

        d++;
        exchange[d] = prev_attacker_value - exchange[d - 1];
        prev_attacker_value = PieceValue(info.piece);

        // Special case - the capture is a pawn promotion (we do not have the move, so assume it is a queen promo)
        if(
            (to_move == Colour::white && info.piece == Piece::pawn && target <= 7) ||
            (to_move == Colour::black && info.piece == Piece::pawn && target >= 56)
        ){
            exchange[d] += (QUEEN_VALUE_CTP - PAWN_VALUE_CTP);
            prev_attacker_value = QUEEN_VALUE_CTP;
        }

        occ_mask ^= (1ULL << info.source);
        to_move = static_cast<Colour>(!to_move);
    }

    // Now evaluate the exchange with minmax
    while(d > 0)
    {
        exchange[d - 1] = -std::max(-exchange[d - 1], exchange[d]);
        d--;
    }

    return exchange[0];
}

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

    // Captures
    if(flag == 4 || flag == 5 || flag >= 12){
        if(flag == 5){
            score += (5000 + mvv_lva[0][0]);
        } else{
            Piece source_piece = board.PieceAtSquare(source, board.to_move);
            Piece target_piece = board.PieceAtSquare(target, static_cast<Colour>(!board.to_move));

            if(PieceValue(source_piece) <= PieceValue(target_piece) && source_piece != Piece::king){
                score += (5000 + mvv_lva[source_piece][target_piece]);
                if(flag >= 12){ score += piece_promotion_value_by_flag[flag] - PAWN_VALUE_CTP; }
            } else{
                //int SEE_score = SEE(move);
                //score += (5000 + mvv_lva[source_piece][target_piece]);
                //score += (SEE_score >= 0 ? GOOD_CAPTURE_BONUS : -BAD_CAPTURE_PENALTY);
                score += 5000 + SEE(move);
            }


        }
    }

    // Quiet promotions
    else if(8 <= flag && flag <= 11){
        score += 5000 + piece_promotion_value_by_flag[flag] - PAWN_VALUE_CTP;
    }

    // Quiet moves (killers, history)
    else if(flag <= 3){
        // Killers get a slightly smaller bonus than captures
        if(move == killer_moves[ply].one){ score += 4000; }
        else if(move == killer_moves[ply].two){ score += 3500; }

        // History
        else{
            score += HistoryMoveScoringFormula(history_moves[source][target]);
        }
    }

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

            if(
                info.flag == TEntryFlag::exact ||
                info.flag == TEntryFlag::LB && stored_score >= beta ||
                info.flag == TEntryFlag::UB && stored_score <= alpha ||
                alpha >= beta
            ){
                nodes_searched++; return stored_score;
            }
        }

        // Leaf node - hand over to Quiescence
        if(depth == 0){
            return Quiescence(alpha, beta, ply);
        }

        nodes_searched++;

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
            PrepareBestMove(list, i);
            //std::cout << list.score_list[i] << " ";

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
                    if(list.list[i] != killer_moves[ply].one){
                        killer_moves[ply].two = killer_moves[ply].one;
                        killer_moves[ply].one = list.list[i];
                    }
                
                    int source = list.list[i] & 0b0000000000111111;
                    int target = (list.list[i] & 0b0000111111000000) >> 6;
                    history_moves[source][target] += depth * depth;
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
        nodes_searched++;
        int static_eval = (board.to_move == Colour::white ? eval.StaticEvaluation() : -eval.StaticEvaluation());

        int best_score = static_eval;
        if(best_score >= beta){ return best_score; }
        if(best_score > alpha){ alpha = best_score; }

        bool in_check = board.InCheck(board.to_move);

        MoveList list; GeneratePseudoLegalMoves(list); FilterCapturesAndPromotions(list); ScoreQuiescenceMoveList(list, ply);
        for(int i = 0; i < list.count; i++){
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

            // Clear what needs to be cleared
            memset(killer_moves, 0, sizeof(killer_moves));
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