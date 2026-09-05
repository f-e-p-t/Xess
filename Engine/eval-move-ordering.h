#include "movegen.h"
#include <iostream>

// |--------------|
// | Search Stack |------------------------------------------------------------
// |--------------|

// Let the search remember these things about its current variation
class Stack {
public:
    int ply;
    uint16_t current_move = 0;
    int moves_searched = 0;
    int static_eval;
    bool in_check;
    bool on_PV_line;
    int current_LMR_reduction = 0;
private:

};

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

    int PawnStructure(){
        int val = 0;

        // MAYBE USE A SEPARATE TT FOR PAWN STRUCTURES ???

        // Penalties for isolated pawns
        const u64 wp = board.pieces[Colour::white][Piece::pawn];
        if( (wp & FILE_A) && !(wp & FILE_B) ){ val -= 15; }
        if( (wp & FILE_B) && !(wp & (FILE_A | FILE_C)) ){ val -= 15; }
        if( (wp & FILE_C) && !(wp & (FILE_B | FILE_D)) ){ val -= 15; }
        if( (wp & FILE_D) && !(wp & (FILE_C | FILE_E)) ){ val -= 15; }
        if( (wp & FILE_E) && !(wp & (FILE_D | FILE_F)) ){ val -= 15; }
        if( (wp & FILE_F) && !(wp & (FILE_E | FILE_G)) ){ val -= 15; }
        if( (wp & FILE_G) && !(wp & (FILE_F | FILE_H)) ){ val -= 15; }
        if( (wp & FILE_H) && !(wp & FILE_G) ){ val -= 15; }

        const u64 bp = board.pieces[Colour::black][Piece::pawn];
        if( (bp & FILE_A) && !(bp & FILE_B) ){ val += 15; }
        if( (bp & FILE_B) && !(bp & (FILE_A | FILE_C)) ){ val += 15; }
        if( (bp & FILE_C) && !(bp & (FILE_B | FILE_D)) ){ val += 15; }
        if( (bp & FILE_D) && !(bp & (FILE_C | FILE_E)) ){ val += 15; }
        if( (bp & FILE_E) && !(bp & (FILE_D | FILE_F)) ){ val += 15; }
        if( (bp & FILE_F) && !(bp & (FILE_E | FILE_G)) ){ val += 15; }
        if( (bp & FILE_G) && !(bp & (FILE_F | FILE_H)) ){ val += 15; }
        if( (bp & FILE_H) && !(bp & FILE_G) ){ val += 15; }

        // Penalties for doubled pawns
        const u64 wptrails = (wp << 8) | (wp << 16) | (wp << 24) | (wp << 32) | (wp << 40) | (wp << 48);
        val -= (15 * NumberOfNonZeroBits(wp & wptrails));
        const u64 bptrails = (bp >> 8) | (bp >> 16) | (bp >> 24) | (bp >> 32) | (bp >> 40) | (bp >> 48);
        val += (15 * NumberOfNonZeroBits(bp & bptrails));

        return val;
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

        // Pawn heatmaps
        int index;
        u64 bitboard;
        bitboard = board.pieces[Colour::white][Piece::pawn];
        while(bitboard){
            index = GetLSBitIndex(bitboard);
            val += Heatmap::pawn[GameStage::midgame][Colour::white][index];
            bitboard &= bitboard - 1;
        }

        bitboard = board.pieces[Colour::black][Piece::pawn];
        while(bitboard){
            index = GetLSBitIndex(bitboard);
            val -= Heatmap::pawn[GameStage::midgame][Colour::black][index];
            bitboard &= bitboard - 1;
        }
        
        // King heatmaps
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
        
        // Pawn heatmaps
        int index;
        u64 bitboard;
        bitboard = board.pieces[Colour::white][Piece::pawn];
        while(bitboard){
            index = GetLSBitIndex(bitboard);
            val += Heatmap::pawn[GameStage::endgame][Colour::white][index];
            bitboard &= bitboard - 1;
        }

        bitboard = board.pieces[Colour::black][Piece::pawn];
        while(bitboard){
            index = GetLSBitIndex(bitboard);
            val -= Heatmap::pawn[GameStage::endgame][Colour::black][index];
            bitboard &= bitboard - 1;
        }

        // King heatmaps
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
        val += PawnStructure();

        val += HandleStageIndependentHeatmaps();
        val += ((stage * StaticEvaluationMidgameExclusive()) + ((STAGE_MAX - stage) * StaticEvaluationEndgameExclusive())) / STAGE_MAX;

        return val;
    }
private:

};

Evaluation eval;

// |-----------------------------------|
// | PV, killer, history, countermoves |---------------------------------------
// |-----------------------------------|

uint16_t PV_table[MAX_PLY][MAX_PLY] = {0};
int PV_length[MAX_PLY] = {0};

uint16_t last_PV_table[MAX_PLY][MAX_PLY] = {0};
int last_PV_length[MAX_PLY] = {0};

struct KillerMovePair {
    uint16_t one = 0;
    uint16_t two = 0;
};

KillerMovePair killer_moves[MAX_PLY];

void WipeKillerTable(){
    for(int i = 0; i < MAX_PLY; i++){
        killer_moves[i].one = 0;
        killer_moves[i].two = 0;
    }
}

// [colour][source][target]
uint16_t history_moves[2][64][64] = {0};

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

int ScoreMove(uint16_t move, Stack * ss){
    int source = move & 0b0000000000111111;
    int target = (move & 0b0000111111000000) >> 6;
    int flag = (move & 0b1111000000000000) >> 12;

    int score = 0;

    // Captures
    if(flag == 4 || flag == 5 || flag >= 12){
        if(flag == 5){
            score += (10000 + mvv_lva[0][0]);
        } else{
            Piece source_piece = board.PieceAtSquare(source, board.to_move);
            Piece target_piece = board.PieceAtSquare(target, static_cast<Colour>(!board.to_move));

            if(PieceValue(source_piece) <= PieceValue(target_piece) && source_piece != Piece::king){
                score += (10000 + mvv_lva[source_piece][target_piece]);
                if(flag >= 12){ score += piece_promotion_value_by_flag[flag] - PAWN_VALUE_CTP; }
            } else{
                int SEE_score = SEE(move);
                score += (SEE_score >= 0 ? (10000 + SEE_score) : (SEE_score));
            }
        }
    }

    // Quiet promotions
    else if(8 <= flag && flag <= 11){
        score += 10000 + piece_promotion_value_by_flag[flag] - PAWN_VALUE_CTP;
    }

    // Quiet moves
    else if(flag <= 3){
        // Killer table
        if(move == killer_moves[ss->ply].one){ score += 6999; }
        else if(move == killer_moves[ss->ply].two){ score += 6500; }

        // History table
        score += HistoryMoveScoringFormula(history_moves[board.to_move][source][target]);
    }

    return score;
}

void ScoreMoveList(MoveList& list, Stack * ss, uint16_t PV_TT_move){
    for(int i = 0; i < list.count; i++){
        list.score_list[i] = ScoreMove(list.list[i], ss);

        // A huge bonus for the best move (taken from the TT)
        if(list.list[i] == PV_TT_move){ list.score_list[i] += 20000; }
    }
}

void ScoreQuiescenceMoveList(MoveList& list, Stack * ss){
    for(int i = 0; i < list.count; i++){
        list.score_list[i] = ScoreMove(list.list[i], ss);
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