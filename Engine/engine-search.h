#include "eval-move-ordering.h"
#include <iostream>

// |---------------------------------|
// | Engine Properties and Functions |-----------------------------------------
// |---------------------------------|

u64 nodes_searched = 0;

std::atomic<bool> stop = false;

void StartSearchTimer(){
    if(engine_search_time_limit_ms < 0){ return; }
    Sleep(engine_search_time_limit_ms);
    stop = true;
}

class Engine {
public:
    int search_depth_max;

    int transposition_table_size_MB;

    int Search(int depth, int alpha, int beta, int ply, bool PV_line, bool NMP_branch){
        if(stop){ return 0; } // <-- Time limit safety measure
        const int original_alpha = alpha;
        bool found_PV = false;
        bool PV_node = (beta - alpha > 1);
        PV_length[ply] = ply;

        TEntry& info = TT.GetEntry(board.hash_key);
        bool TT_match = (info.hash_key == board.hash_key);
        if(TT_match && info.depth >= depth && !PV_node){
            int stored_score = info.score;
            
            // Denormalise depth to mate
            if(stored_score > CHECKMATE_THRESHOLD){ stored_score -= ply; }
            else if(stored_score < -CHECKMATE_THRESHOLD){ stored_score += ply; }

            if(
                info.flag == TEntryFlag::exact ||
                (info.flag == TEntryFlag::LB && stored_score >= beta) ||
                (info.flag == TEntryFlag::UB && stored_score <= alpha)
            ){
                nodes_searched++; return stored_score;
            }
        }

        if(depth == 0){
            return Quiescence(alpha, beta, ply);
        }

        nodes_searched++;

        int score; int best_score = -INFTY; uint16_t best_move = 0; TEntry entry; bool legal_moves = false; int reduction;
        int flag; bool in_check = board.InCheck(board.to_move);

        // NMP
        int NMP_reduction = 2;
        if(
            !NMP_branch && !in_check && !PV_node && depth - 1 - NMP_reduction >= 0 && ply >= NMP_MIN_PLY &&
            board.SideHasNonPawnMaterial(board.to_move) &&
            (board.to_move == Colour::white ? eval.StaticEvaluation() : -eval.StaticEvaluation()) >= beta
        ){
            UnmakeMoveGameState irr_info_null = board.MakeNullMove(board.to_move);
            int null_score = -Search(depth - 1 - NMP_reduction, -beta, -beta + 1, ply + 1, false, true);
            board.UnmakeNullMove(board.to_move, irr_info_null);
            if(stop){ return 0; } // <-- Time limit safety measure
            if(null_score >= beta && std::abs(null_score) < CHECKMATE_THRESHOLD){
                int verification = Search(depth - 1 - NMP_reduction, beta - 1, beta, ply, false, false);
                if(verification >= beta){ return null_score; }
            }
        }

        MoveList list; GeneratePseudoLegalMoves(list);

        // Move scoring
        if(PV_line && PV_node){ ScoreMoveList(list, last_PV_table[0][ply], ply); }
        else if(TT_match && info.flag != TEntryFlag::UB){ ScoreMoveList(list, info.best_move, ply); }
        else{ ScoreMoveList(list, 0, ply); }

        for(int i = 0; i < list.count; i++){
            PrepareBestMove(list, i);
            flag = (list.list[i] & 0b1111000000000000) >> 12;
            UnmakeMoveGameState irr_info = board.MakeMove(list.list[i], board.to_move);
            if(board.InCheck(static_cast<Colour>(!board.to_move))){ board.UnmakeMove(list.list[i], board.to_move, irr_info); continue; }
            legal_moves = true;

            bool child_on_PV_line = PV_line && (list.list[i] == PV_table[0][ply]);

            // PVS and LMR
            reduction = CalculateLMRReduction(list.list[i], depth, i, ply, in_check);
            if(found_PV){
                score = -Search(depth - 1 - reduction, -alpha - 1, -alpha, ply + 1, child_on_PV_line, false);

                if(score > alpha && score < beta){ score = -Search(depth - 1, -beta, -alpha, ply + 1, child_on_PV_line, false); }
            } else{
                score = -Search(depth - 1 - reduction, -beta, -alpha, ply + 1, child_on_PV_line, false);

                if(score > alpha && score < beta){ score = -Search(depth - 1, -beta, -alpha, ply + 1, child_on_PV_line, false); }
            }

            board.UnmakeMove(list.list[i], board.to_move, irr_info);

            if(stop){ return 0; } // <-- Time limit safety measure

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
                if(flag <= 3){
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
        if(!legal_moves){ PV_length[ply] = ply; best_score = (in_check ? -CHECKMATE + ply : STALEMATE); }
        
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

    void IterativeSearch(){
        int iteration_depth = 1;
        int s = 0;
        while(iteration_depth <= search_depth_max){
            s = Search(iteration_depth, -INFTY, INFTY, 0, true, false);

            // Time has run out - do not update last_PV_table (the one the move is played from) and break
            if(stop){ search_age++; nodes_searched = 0; break; }

            memcpy(last_PV_table, PV_table, sizeof(last_PV_table));
            memcpy(last_PV_length, PV_length, sizeof(last_PV_length));

            std::cout << "Depth " << iteration_depth << " | Nodes: " << nodes_searched << " | Score: " << s << " | PV:";
            PrintPVToTerminal();
            std::cout << "\n";
            
            nodes_searched = 0;
            iteration_depth++;

            memset(killer_moves, 0, sizeof(killer_moves));
        }

        search_age++;

        std::cout << "\n";
    }

    // Also returns 0 if inappropriate to reduce
    int CalculateLMRReduction(uint16_t move, int depth, int moves, int ply, bool in_check){
        int source = move & 0b0000000000111111;
        int target = (move & 0b0000111111000000) >> 6;
        int flag = (move & 0b1111000000000000) >> 12;

        Piece moved_piece_if_not_promo = board.PieceAtSquare(target, static_cast<Colour>(!board.to_move));

        // Conditions to avoid LMR
        if(
            in_check ||
            move == killer_moves[ply].one ||
            move == killer_moves[ply].two
        ){
            return 0;
        }

        if(flag <= 3){
            return LMR_table_quiet[depth][moves];
        } else{
            return LMR_table_captures_promos[depth][moves];
        }
    }

    void PrintPVToTerminal(){
        for(int i = 0; i < last_PV_length[0]; i++){
            std::cout << " ";
            PrintMoveToTerminalNoFlag(last_PV_table[0][i]);
        }
    }
private:
    int search_age = 0;
};

Engine engine;