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

    int Search(int depth, Stack * ss, int alpha, int beta){
        if(stop){ return 0; } // <-- Time limit safety measure
        const int original_alpha = alpha;
        bool PV_node = (beta - alpha > 1);
        PV_length[ss->ply] = ss->ply;

        TEntry& info = TT.GetEntry(board.hash_key);
        bool TT_match = (info.hash_key == board.hash_key);
        if(TT_match && !PV_node && info.depth >= depth){
            int stored_score = info.score;
            
            // Denormalise depth to mate
            if(stored_score > CHECKMATE_THRESHOLD){ stored_score -= ss->ply; }
            else if(stored_score < -CHECKMATE_THRESHOLD){ stored_score += ss->ply; }

            if(
                info.flag == TEntryFlag::exact ||
                (info.flag == TEntryFlag::LB && stored_score >= beta) ||
                (info.flag == TEntryFlag::UB && stored_score <= alpha)
            ){
                nodes_searched++; return stored_score;
            }
        }

        if(depth == 0){
            return Quiescence(ss, alpha, beta);
        }

        nodes_searched++;

        int score; int best_score = -INFTY; uint16_t best_move = 0; TEntry entry; ss->current_move = 0;
        int flag; ss->in_check = board.InCheck(board.to_move); ss->moves_searched = 0;

        // NMP
        int NMP_reduction = 2;
        if(
            (ss - 1)->current_move != NULL_MOVE && !ss->in_check && !PV_node && depth - 1 - NMP_reduction >= 0 &&
            ss->ply >= NMP_min_ply && board.SideHasNonPawnMaterial(board.to_move) && beta >= -2000 &&
            (board.to_move == Colour::white ? eval.StaticEvaluation() : -eval.StaticEvaluation()) >= beta
        ){
            ss->current_move = NULL_MOVE;
            UnmakeMoveGameState irr_info_null = board.MakeNullMove(board.to_move);
            int null_score = -Search(depth - 1 - NMP_reduction, ss + 1, -beta, -beta + 1);
            board.UnmakeNullMove(board.to_move, irr_info_null);
            if(stop){ return 0; } // <-- Time limit safety measure

            if(null_score >= beta && std::abs(null_score) < CHECKMATE_THRESHOLD){

                // If the depth is low enough, skip the verification search
                if(depth < 10){ return null_score; }                

                // Set NMP_min_ply forward to delay NMP in verification search
                int NMP_min_ply_restore = NMP_min_ply;
                NMP_min_ply = ss->ply + 3 + (depth / 4);
                int verification = Search(depth - 1 - NMP_reduction, ss, beta - 1, beta);
                NMP_min_ply = NMP_min_ply_restore;
                if(stop){ return 0; } // <-- Time limit safety measure

                // Verified
                if(verification >= beta){ return null_score; }
            }
        }

        MoveList list; GeneratePseudoLegalMoves(list);

        // Move scoring
        if(ss->on_PV_line && PV_node){ ScoreMoveList(list, ss, last_PV_table[0][ss->ply]); }
        else if(TT_match){ ScoreMoveList(list, ss, info.best_move); }
        else{ ScoreMoveList(list, ss, 0); }

        for(int i = 0; i < list.count; i++){
            PrepareBestMove(list, i);
            ss->current_move = list.list[i];
            flag = (ss->current_move & 0b1111000000000000) >> 12;

            // Make the move
            UnmakeMoveGameState irr_info = board.MakeMove(ss->current_move, board.to_move);
            if(board.InCheck(static_cast<Colour>(!board.to_move))){ board.UnmakeMove(ss->current_move, board.to_move, irr_info); continue; }

            (ss + 1)->on_PV_line = ss->on_PV_line && (ss->current_move == last_PV_table[0][ss->ply]);

            // PVS and LMR
            ss->current_LMR_reduction = CalculateLMRReduction(ss->current_move, depth, ss->moves_searched, ss->ply, ss->in_check);
            if(ss->moves_searched){
                score = -Search(depth - 1 - ss->current_LMR_reduction, ss + 1, -alpha - 1, -alpha);

                if(score > alpha && score < beta){ score = -Search(depth - 1, ss + 1, -beta, -alpha); }
            } else{
                score = -Search(depth - 1, ss + 1, -beta, -alpha);
            }

            board.UnmakeMove(ss->current_move, board.to_move, irr_info);
            ss->moves_searched++;

            if(stop){ return 0; } // <-- Time limit safety measure

            // Better move
            if(score > best_score){
                best_score = score; best_move = ss->current_move;

                if(score > alpha){
                    alpha = score;

                    PV_table[ss->ply][ss->ply] = ss->current_move;
                    for(int j = ss->ply + 1; j < PV_length[ss->ply + 1]; j++){ PV_table[ss->ply][j] = PV_table[ss->ply + 1][j]; }
                    PV_length[ss->ply] = PV_length[ss->ply + 1];
                }
            }

            // Beta cutoff (fail-high)
            if(best_score >= beta){
                if(flag <= 3){
                    if(ss->current_move != killer_moves[ss->ply].one){
                        killer_moves[ss->ply].two = killer_moves[ss->ply].one;
                        killer_moves[ss->ply].one = ss->current_move;
                    }

                    int source = ss->current_move & 0b0000000000111111;
                    int target = (ss->current_move & 0b0000111111000000) >> 6;
                    history_moves[board.to_move][source][target] += (depth) * (depth);
                }
    
                break;
            }
        }

        // Checkmate and stalemate
        if(!ss->moves_searched){ PV_length[ss->ply] = ss->ply; best_score = (ss->in_check ? -CHECKMATE + ss->ply : STALEMATE); }
        
        // Normalise depth to mate before inserting into TT
        int TT_score = best_score;
        if(TT_score > CHECKMATE_THRESHOLD){ TT_score += ss->ply; }
        else if(TT_score < -CHECKMATE_THRESHOLD){ TT_score -= ss->ply; }
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

    int Quiescence(Stack * ss, int alpha, int beta){
        nodes_searched++;

        ss->in_check = board.InCheck(board.to_move);
        int static_eval = (board.to_move == Colour::white ? eval.StaticEvaluation() : -eval.StaticEvaluation());

        // Do not stand pat if in check
        int best_score = (ss->in_check ? -INFTY : static_eval);
        if(best_score >= beta){ return best_score; }
        if(best_score > alpha){ alpha = best_score; }

        ss->moves_searched = 0;

        // If in check, search all moves
        MoveList list; GeneratePseudoLegalMoves(list);
        if(!ss->in_check){ FilterCapturesAndPromotions(list); }
        ScoreQuiescenceMoveList(list, ss);
        for(int i = 0; i < list.count; i++){
            PrepareBestMove(list, i);
            ss->current_move = list.list[i];

            // If the move is a non-pawn-promotion capture and we are not in check, apply delta pruning
            int flag = ((ss->current_move & 0b1111000000000000) >> 12);
            if(!ss->in_check && flag > 3 && flag < 8){
                int target_square = (ss->current_move & 0b0000111111000000) >> 6;
                Piece target_piece = board.PieceAtSquare(target_square, static_cast<Colour>(!board.to_move));
                int target_value = PieceValue(target_piece);
                if(flag == MoveFlag::EP_capture){ target_value = PAWN_VALUE_CTP; }
                
                if(best_score + target_value + DELTA < alpha){ continue; }
            }

            UnmakeMoveGameState irr_info = board.MakeMove(ss->current_move, board.to_move);
            if(board.InCheck(static_cast<Colour>(!board.to_move))){ board.UnmakeMove(ss->current_move, board.to_move, irr_info); continue; }

            int score = -Quiescence(ss + 1, -beta, -alpha);
            board.UnmakeMove(ss->current_move, board.to_move, irr_info);
            ss->moves_searched++;

            if(score > best_score){ best_score = score; }
            if(score >= beta){ return score; }
            if(score > alpha){ alpha = score; }
        }

        if(!ss->moves_searched && ss->in_check){ best_score = -CHECKMATE + ss->ply; }

        return best_score;
    }

    void IterativeSearch(){
        int iteration_depth = 1;
        int s = 0;

        // Set up the search stack
        Stack stack[MAX_PLY + 10] = {};
        Stack * ss = stack + 7;
        for(int i = 0; i <= MAX_PLY + 2; i++){ (ss + i)->ply = i; }
        ss->on_PV_line = true; // (Root node)

        while(iteration_depth <= search_depth_max){
            s = Search(iteration_depth, ss, -INFTY, INFTY);

            // Time has run out - do not update last_PV_table (the one the move is played from) and break
            if(stop){ search_age++; nodes_searched = 0; break; }

            memcpy(last_PV_table, PV_table, sizeof(last_PV_table));
            memcpy(last_PV_length, PV_length, sizeof(last_PV_length));

            std::cout << "Depth " << iteration_depth << " | Nodes: " << nodes_searched << " | Score: " << s << " | PV:";
            PrintPVToTerminal();
            std::cout << "\n";
            
            nodes_searched = 0;
            iteration_depth++;
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