#include "mask.h"
#include "heatmap.h"
#include <iostream>

std::string STARTPOS = "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1";
//std::string FEN = "5rk1/p4pbp/BpQ3p1/4p3/P7/2P1q1PP/1P2n1PB/3R3K w - - 1 0";
std::string FEN = STARTPOS;

int64_t nodes = 0;

// |-----------|
// | The board |---------------------------------------------------------------
// |-----------|

class UnmakeMoveGameState {
public:
    uint8_t en_passant_square;
    uint8_t castling_rights;
    int halfmove_clock;
    int fullmove_number;
    u64 hash_key;

    Piece source_piece;
    Piece target_piece;

    UnmakeMoveGameState(uint8_t ep, uint8_t cr, int hmc, int fmn, u64 hk): en_passant_square(ep), castling_rights(cr), halfmove_clock(hmc), fullmove_number(fmn), hash_key(hk) {}
};

class Board {
public:
    u64 pieces[2][6] = {0};
    u64 colour_occ[2] = {0};
    u64 total_occ = 0;

    Colour to_move;
    uint8_t en_passant_square = Square::NO_SQUARE; // Can only be one square at a time
    uint8_t castling_rights = 0;
    int halfmove_clock = 0; // Since a pawn was advanced or a capture was made (for the 50 move rule)
    int fullmove_number = 0;

    u64 hash_key;

    // ------------

    Piece PieceAtSquare(int sq, Colour side){
        for(int i = Piece::pawn; i <= Piece::king; i++){
            if(pieces[side][i] & (1ULL << sq)){ return static_cast<Piece>(i); }
        }

        return Piece::NO_PIECE;
    }

    bool SquareAttackedBy(int sq, Colour side){
        u64 O;
        u64 other = colour_occ[!side];

        // If the square can see the piece, the piece can see the square
        // By pawns
        if(pawn_attacks[!side][sq] & pieces[side][Piece::pawn]){ return true; }

        // By knights
        if(knight_attacks[sq] & pieces[side][Piece::knight]){ return true; }

        // By rooks (queens)
        O = rook_rays_no_edges[sq] & total_occ;
        if((rook_attacks[sq][HashRookOccConfig(sq, O)] & ~other) & (pieces[side][Piece::rook] | pieces[side][Piece::queen])){ return true; }

        // By bishops (queens)
        O = bishop_rays_no_edges[sq] & total_occ;
        if((bishop_attacks[sq][HashBishopOccConfig(sq, O)] & ~other) & (pieces[side][Piece::bishop] | pieces[side][Piece::queen])){ return true; }

        // By king
        if(king_attacks[sq] & pieces[side][Piece::king]){ return true; }

        // Square is not attacked
        return false;
    }

    bool InCheck(Colour side){
        int king_sq = GetLSBitIndex(pieces[side][Piece::king]);
        return SquareAttackedBy(king_sq, static_cast<Colour>(!side));
    }

    UnmakeMoveGameState MakeMove(uint16_t move, Colour side){
        int source = move & 0b0000000000111111;
        int target = (move & 0b0000111111000000) >> 6;
        int flag = (move & 0b1111000000000000) >> 12;

        // updating the hashkey:
        // To do:
        // 3, 4, 5, 8, 9, 10, 11, 12, 13, 14, 15

        UnmakeMoveGameState state = UnmakeMoveGameState(en_passant_square, castling_rights, halfmove_clock, fullmove_number, hash_key);

        u64 source_place = 1ULL << source;
        u64 target_place = 1Ull << target;

        Piece source_piece = PieceAtSquare(source, side);
        Piece target_piece = Piece::NO_PIECE;

        // EP square exists - it will be removed this move - must hash it out of the hash key before it is lost
        if(en_passant_square != Square::NO_SQUARE){ hash_key ^= EP_keys[en_passant_square]; }

        en_passant_square = Square::NO_SQUARE;

        castling_rights &= castling_rights_mask_table[source];
        castling_rights &= castling_rights_mask_table[target];

        source_piece == Piece::pawn ? halfmove_clock = 0 : halfmove_clock++;

        if(side == Colour::black){ fullmove_number++; }

        switch(flag){
            case MoveFlag::quiet_move:
            {
                u64 both_places = source_place | target_place;
                pieces[side][source_piece] ^= both_places;
                colour_occ[side] ^= both_places;
                total_occ ^= both_places;

                hash_key ^= piece_keys[side][source_piece][source];
                hash_key ^= piece_keys[side][source_piece][target];
                
                break;
            }

            case MoveFlag::double_pawn_push:
            {
                u64 both_places = source_place | target_place;
                pieces[side][source_piece] ^= both_places;
                colour_occ[side] ^= both_places;
                total_occ ^= both_places;

                // EP square has opened up - hash it into the hash key
                if(side == Colour::white){ en_passant_square = target + 8; hash_key ^= EP_keys[en_passant_square]; }
                else{ en_passant_square = target - 8; hash_key ^= EP_keys[en_passant_square]; }

                hash_key ^= piece_keys[side][source_piece][source];
                hash_key ^= piece_keys[side][source_piece][target];
                
                break;
            }

            case MoveFlag::KS_castle:
            {
                if(side == Colour::white){
                    pieces[side][Piece::king] ^= CASTLE_WHITE_KS_KING_XOR;
                    pieces[side][Piece::rook] ^= CASTLE_WHITE_KS_ROOK_XOR;
                    colour_occ[side] ^= CASTLE_WHITE_KS_OCC_XOR;
                    total_occ ^= CASTLE_WHITE_KS_OCC_XOR;
                } else{
                    pieces[side][Piece::king] ^= CASTLE_BLACK_KS_KING_XOR;
                    pieces[side][Piece::rook] ^= CASTLE_BLACK_KS_ROOK_XOR;
                    colour_occ[side] ^= CASTLE_BLACK_KS_OCC_XOR;
                    total_occ ^= CASTLE_BLACK_KS_OCC_XOR;
                }

                break;
            }

            case MoveFlag::QS_castle:
            {
                if(side == Colour::white){
                    pieces[side][Piece::king] ^= CASTLE_WHITE_QS_KING_XOR;
                    pieces[side][Piece::rook] ^= CASTLE_WHITE_QS_ROOK_XOR;
                    colour_occ[side] ^= CASTLE_WHITE_QS_OCC_XOR;
                    total_occ ^= CASTLE_WHITE_QS_OCC_XOR;
                } else{
                    pieces[side][Piece::king] ^= CASTLE_BLACK_QS_KING_XOR;
                    pieces[side][Piece::rook] ^= CASTLE_BLACK_QS_ROOK_XOR;
                    colour_occ[side] ^= CASTLE_BLACK_QS_OCC_XOR;
                    total_occ ^= CASTLE_BLACK_QS_OCC_XOR;
                }

                break;
            }

            case MoveFlag::normal_capture:
            {
                target_piece = PieceAtSquare(target, static_cast<Colour>(!side));
                u64 both_places = source_place | target_place;
                pieces[side][source_piece] ^= both_places;
                colour_occ[side] ^= both_places;
                pieces[!side][target_piece] ^= target_place;
                colour_occ[!side] ^= target_place;
                total_occ ^= source_place;

                halfmove_clock = 0;

                break;
            }

            case MoveFlag::EP_capture:
            {
                u64 both_places = source_place | target_place;
                pieces[side][Piece::pawn] ^= both_places;
                colour_occ[side] ^= both_places;
                total_occ ^= both_places;
                if(side == Colour::white){
                    pieces[!side][Piece::pawn] ^= (target_place << 8); colour_occ[!side] ^= (target_place << 8); total_occ ^= (target_place << 8);
                } else{
                    pieces[!side][Piece::pawn] ^= (target_place >> 8); colour_occ[!side] ^= (target_place >> 8); total_occ ^= (target_place >> 8);
                }

                break;
            }

            case MoveFlag::quiet_knight_promo:
            {
                u64 both_places = source_place | target_place;
                pieces[side][Piece::pawn] ^= source_place;
                pieces[side][Piece::knight] ^= target_place;
                colour_occ[side] ^= both_places;
                total_occ ^= both_places;

                break;
            }

            case MoveFlag::quiet_bishop_promo:
            {
                u64 both_places = source_place | target_place;
                pieces[side][Piece::pawn] ^= source_place;
                pieces[side][Piece::bishop] ^= target_place;
                colour_occ[side] ^= both_places;
                total_occ ^= both_places;

                break;
            }

            case MoveFlag::quiet_rook_promo:
            {
                u64 both_places = source_place | target_place;
                pieces[side][Piece::pawn] ^= source_place;
                pieces[side][Piece::rook] ^= target_place;
                colour_occ[side] ^= both_places;
                total_occ ^= both_places;

                break;
            }

            case MoveFlag::quiet_queen_promo:
            {
                u64 both_places = source_place | target_place;
                pieces[side][Piece::pawn] ^= source_place;
                pieces[side][Piece::queen] ^= target_place;
                colour_occ[side] ^= both_places;
                total_occ ^= both_places;

                break;
            }

            case MoveFlag::capture_knight_promo:
            {
                target_piece = PieceAtSquare(target, static_cast<Colour>(!side));
                u64 both_places = source_place | target_place;
                pieces[side][Piece::pawn] ^= source_place;
                pieces[side][Piece::knight] ^= target_place;
                colour_occ[side] ^= both_places;
                pieces[!side][target_piece] ^= target_place;
                colour_occ[!side] ^= target_place;
                total_occ ^= source_place;

                break;
            }

            case MoveFlag::capture_bishop_promo:
            {
                target_piece = PieceAtSquare(target, static_cast<Colour>(!side));
                u64 both_places = source_place | target_place;
                pieces[side][Piece::pawn] ^= source_place;
                pieces[side][Piece::bishop] ^= target_place;
                colour_occ[side] ^= both_places;
                pieces[!side][target_piece] ^= target_place;
                colour_occ[!side] ^= target_place;
                total_occ ^= source_place;

                break;
            }

            case MoveFlag::capture_rook_promo:
            {
                target_piece = PieceAtSquare(target, static_cast<Colour>(!side));
                u64 both_places = source_place | target_place;
                pieces[side][Piece::pawn] ^= source_place;
                pieces[side][Piece::rook] ^= target_place;
                colour_occ[side] ^= both_places;
                pieces[!side][target_piece] ^= target_place;
                colour_occ[!side] ^= target_place;
                total_occ ^= source_place;

                break;
            }

            case MoveFlag::capture_queen_promo:
            {
                target_piece = PieceAtSquare(target, static_cast<Colour>(!side));
                u64 both_places = source_place | target_place;
                pieces[side][Piece::pawn] ^= source_place;
                pieces[side][Piece::queen] ^= target_place;
                colour_occ[side] ^= both_places;
                pieces[!side][target_piece] ^= target_place;
                colour_occ[!side] ^= target_place;
                total_occ ^= source_place;

                break;
            }

            default:
            {
                // Unreachable
                break;
            }
        }

        to_move = static_cast<Colour>(!to_move);

        hash_key ^= castling_keys[castling_rights];
        hash_key ^= side_key;

        state.source_piece = source_piece;
        state.target_piece = target_piece;

        return state;
    }

    void UnmakeMove(uint16_t move, Colour current_side, UnmakeMoveGameState prev_state){
        Colour side = static_cast<Colour>(!current_side);

        en_passant_square = prev_state.en_passant_square;
        castling_rights = prev_state.castling_rights;
        halfmove_clock = prev_state.halfmove_clock;
        fullmove_number = prev_state.fullmove_number;
        hash_key = prev_state.hash_key;

        int source = move & 0b0000000000111111;
        int target = (move & 0b0000111111000000) >> 6;
        int flag = (move & 0b1111000000000000) >> 12;
        u64 source_place = 1ULL << source;
        u64 target_place = 1Ull << target;
        Piece source_piece = prev_state.source_piece;
        Piece target_piece = prev_state.target_piece;

        switch(flag){
            case MoveFlag::quiet_move:
            {
                u64 both_places = source_place | target_place;
                pieces[side][source_piece] ^= both_places;
                colour_occ[side] ^= both_places;
                total_occ ^= both_places;

                break;
            }

            case MoveFlag::double_pawn_push:
            {
                u64 both_places = source_place | target_place;
                pieces[side][source_piece] ^= both_places;
                colour_occ[side] ^= both_places;
                total_occ ^= both_places;
                
                break;
            }

            case MoveFlag::KS_castle:
            {
                if(side == Colour::white){
                    pieces[side][Piece::king] ^= CASTLE_WHITE_KS_KING_XOR;
                    pieces[side][Piece::rook] ^= CASTLE_WHITE_KS_ROOK_XOR;
                    colour_occ[side] ^= CASTLE_WHITE_KS_OCC_XOR;
                    total_occ ^= CASTLE_WHITE_KS_OCC_XOR;
                } else{
                    pieces[side][Piece::king] ^= CASTLE_BLACK_KS_KING_XOR;
                    pieces[side][Piece::rook] ^= CASTLE_BLACK_KS_ROOK_XOR;
                    colour_occ[side] ^= CASTLE_BLACK_KS_OCC_XOR;
                    total_occ ^= CASTLE_BLACK_KS_OCC_XOR;
                }

                break;
            }

            case MoveFlag::QS_castle:
            {
                if(side == Colour::white){
                    pieces[side][Piece::king] ^= CASTLE_WHITE_QS_KING_XOR;
                    pieces[side][Piece::rook] ^= CASTLE_WHITE_QS_ROOK_XOR;
                    colour_occ[side] ^= CASTLE_WHITE_QS_OCC_XOR;
                    total_occ ^= CASTLE_WHITE_QS_OCC_XOR;
                } else{
                    pieces[side][Piece::king] ^= CASTLE_BLACK_QS_KING_XOR;
                    pieces[side][Piece::rook] ^= CASTLE_BLACK_QS_ROOK_XOR;
                    colour_occ[side] ^= CASTLE_BLACK_QS_OCC_XOR;
                    total_occ ^= CASTLE_BLACK_QS_OCC_XOR;
                }

                break;
            }

            case MoveFlag::normal_capture:
            {
                u64 both_places = source_place | target_place;
                pieces[side][source_piece] ^= both_places;
                colour_occ[side] ^= both_places;
                pieces[!side][target_piece] ^= target_place;
                colour_occ[!side] ^= target_place;
                total_occ ^= source_place;

                break;
            }

            case MoveFlag::EP_capture:
            {
                u64 both_places = source_place | target_place;
                pieces[side][Piece::pawn] ^= both_places;
                colour_occ[side] ^= both_places;
                total_occ ^= both_places;
                if(side == Colour::white){
                    pieces[!side][Piece::pawn] ^= (target_place << 8); colour_occ[!side] ^= (target_place << 8); total_occ ^= (target_place << 8);
                } else{
                    pieces[!side][Piece::pawn] ^= (target_place >> 8); colour_occ[!side] ^= (target_place >> 8); total_occ ^= (target_place >> 8);
                }

                break;
            }

            case MoveFlag::quiet_knight_promo:
            {
                u64 both_places = source_place | target_place;
                pieces[side][Piece::pawn] ^= source_place;
                pieces[side][Piece::knight] ^= target_place;
                colour_occ[side] ^= both_places;
                total_occ ^= both_places;

                break;
            }

            case MoveFlag::quiet_bishop_promo:
            {
                u64 both_places = source_place | target_place;
                pieces[side][Piece::pawn] ^= source_place;
                pieces[side][Piece::bishop] ^= target_place;
                colour_occ[side] ^= both_places;
                total_occ ^= both_places;

                break;
            }

            case MoveFlag::quiet_rook_promo:
            {
                u64 both_places = source_place | target_place;
                pieces[side][Piece::pawn] ^= source_place;
                pieces[side][Piece::rook] ^= target_place;
                colour_occ[side] ^= both_places;
                total_occ ^= both_places;

                break;
            }

            case MoveFlag::quiet_queen_promo:
            {
                u64 both_places = source_place | target_place;
                pieces[side][Piece::pawn] ^= source_place;
                pieces[side][Piece::queen] ^= target_place;
                colour_occ[side] ^= both_places;
                total_occ ^= both_places;

                break;
            }

            case MoveFlag::capture_knight_promo:
            {
                u64 both_places = source_place | target_place;
                pieces[side][Piece::pawn] ^= source_place;
                pieces[side][Piece::knight] ^= target_place;
                colour_occ[side] ^= both_places;
                pieces[!side][target_piece] ^= target_place;
                colour_occ[!side] ^= target_place;
                total_occ ^= source_place;

                break;
            }

            case MoveFlag::capture_bishop_promo:
            {
                u64 both_places = source_place | target_place;
                pieces[side][Piece::pawn] ^= source_place;
                pieces[side][Piece::bishop] ^= target_place;
                colour_occ[side] ^= both_places;
                pieces[!side][target_piece] ^= target_place;
                colour_occ[!side] ^= target_place;
                total_occ ^= source_place;

                break;
            }

            case MoveFlag::capture_rook_promo:
            {
                u64 both_places = source_place | target_place;
                pieces[side][Piece::pawn] ^= source_place;
                pieces[side][Piece::rook] ^= target_place;
                colour_occ[side] ^= both_places;
                pieces[!side][target_piece] ^= target_place;
                colour_occ[!side] ^= target_place;
                total_occ ^= source_place;

                break;
            }

            case MoveFlag::capture_queen_promo:
            {
                u64 both_places = source_place | target_place;
                pieces[side][Piece::pawn] ^= source_place;
                pieces[side][Piece::queen] ^= target_place;
                colour_occ[side] ^= both_places;
                pieces[!side][target_piece] ^= target_place;
                colour_occ[!side] ^= target_place;
                total_occ ^= source_place;

                break;
            }

            default:
            {
                // Unreachable
                break;
            }
        }

        to_move = static_cast<Colour>(!to_move);

        return;
    }
private:

};

Board board;

struct MoveList {
    uint16_t list[256];
    int count;
};

// |---------------------|
// | Transposition Table |-----------------------------------------------------
// |---------------------|

void InitialiseHashKey(){
    u64 key = 0ULL;
    u64 bitboard;
    int index;

    for(int colour = 0; colour < 2; colour++){
        for(int piece = 0; piece < 6; piece++){
            bitboard = board.pieces[colour][piece];

            while(bitboard){
                index = GetLSBitIndex(bitboard);

                key ^= piece_keys[colour][piece][index];

                bitboard &= bitboard - 1;
            }
        }
    }

    if(board.en_passant_square != Square::NO_SQUARE){
        key ^= EP_keys[board.en_passant_square];
    }

    key ^= castling_keys[board.castling_rights];

    if(board.to_move == Colour::black){ key ^= side_key; }

    board.hash_key = key;
}

class TEntry {
    u64 hash_key = 0;
    int depth = 0;
    int age = 0;
    int flag = 0;
    int score = 0;
    uint16_t move = 0;
};

class TranspositionTable{
public:
    void SetSize(u64 megabytes){
        u64 total_bytes = megabytes * 1024 * 1024;
        u64 total_entries = total_bytes / sizeof(TEntry);

        // Must be a power of 2
        int LSBitIndex;
        while(total_entries){
            LSBitIndex = GetLSBitIndex(total_entries);
            total_entries &= total_entries - 1;
        }

        u64 total_entries_power_2 = 1 << LSBitIndex;
        table.resize(total_entries_power_2);

        hash_mask = total_entries_power_2 - 1;

        std::cout << "Transposition table allocated " << megabytes << "MB with " << total_entries_power_2 << " entries\n";
    }

    TEntry& GetEntry(u64 hash_key){
        return table[hash_key & hash_mask];
    }
private:
    std::vector<TEntry> table;

    u64 hash_mask;
};

TranspositionTable TT;

// |-----------|
// | Utilities |---------------------------------------------------------------
// |-----------|

void PrintBoardToTerminal(){
    std::cout << "\n-----------------------------------------------\n" << "to move: " << board.to_move << " | CR: " <<
    GetBitBinary(board.castling_rights, 0) << GetBitBinary(board.castling_rights, 1) << GetBitBinary(board.castling_rights, 2) << GetBitBinary(board.castling_rights, 3) <<
    " | EP: " << IToSq((int)board.en_passant_square) << " | HMC: " << board.halfmove_clock << " | FMN: " << board.fullmove_number << "\n-----------------------------------------------\n";

    for(int rk = 0; rk < 8; rk++){
        std::cout << "|  ";
        for(int fl = 0; fl < 8; fl++){
            int square = (rk * 8) + fl;
            if(!GetBit(board.total_occ, square)) std::cout << "-  ";
            else if(GetBit(board.pieces[0][0], square)) std::cout << "P  ";
            else if(GetBit(board.pieces[0][1], square)) std::cout << "N  ";
            else if(GetBit(board.pieces[0][2], square)) std::cout << "B  ";
            else if(GetBit(board.pieces[0][3], square)) std::cout << "R  ";
            else if(GetBit(board.pieces[0][4], square)) std::cout << "Q  ";
            else if(GetBit(board.pieces[0][5], square)) std::cout << "K  ";
            else if(GetBit(board.pieces[1][0], square)) std::cout << "p  ";
            else if(GetBit(board.pieces[1][1], square)) std::cout << "n  ";
            else if(GetBit(board.pieces[1][2], square)) std::cout << "b  ";
            else if(GetBit(board.pieces[1][3], square)) std::cout << "r  ";
            else if(GetBit(board.pieces[1][4], square)) std::cout << "q  ";
            else if(GetBit(board.pieces[1][5], square)) std::cout << "k  ";
            else { std::cout << "?  "; }
        }
        std::cout << "|\n";
    }
    std::cout << "-----------------------------------------------\n";
    std::cout << "\n";
}

Colour CharToColour(char c){
    if(c >= 'a' && c <= 'z') return Colour::black;
    else if(c >= 'A' && c <= 'Z') return Colour::white;

    // Unreachable if this function is used correctly
    else{ return Colour::white; }
}

Piece CharToPiece(char c){
    switch(c){
        case 'p': return Piece::pawn;
        case 'n': return Piece::knight;
        case 'b': return Piece::bishop;
        case 'r': return Piece::rook;
        case 'q': return Piece::queen;
        case 'k': return Piece::king;
        case 'P': return Piece::pawn;
        case 'N': return Piece::knight;
        case 'B': return Piece::bishop;
        case 'R': return Piece::rook;
        case 'Q': return Piece::queen;
        case 'K': return Piece::king;
        default: break;
    }

    // Unreachable if this function is used correctly
    return Piece::pawn;
}

void ParseFEN(std::string FEN){
    int pos = 0;
    int sq = 0;

    // The pieces
    while(sq < 64){
        char c = FEN[pos];

        // Piece
        if((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z')){
            //std::cout << "PIECE FOUND: " << c << "\n";
            SetBit(board.pieces[CharToColour(c)][CharToPiece(c)], sq);
            SetBit(board.colour_occ[CharToColour(c)], sq);
            SetBit(board.total_occ, sq);
            sq++;
            pos++;
            continue;
        }

        // Empty squares
        if(c >= '0' && c <= '9'){
            int offset = c - '0';
            sq += offset;
            pos++;
            continue;
        }

        // Next rank
        if(c == '/'){
            pos++;
            continue;
        }
    }

    pos++;

    // Side to move
    (FEN[pos] == 'w') ? (board.to_move = Colour::white) : (board.to_move = Colour::black);

    pos += 2;

    // Castling Rights
    while(FEN[pos] != ' '){
        switch(FEN[pos]){
            case 'K': { board.castling_rights += 1; break; }
            case 'Q': { board.castling_rights += 2; break; }
            case 'k': { board.castling_rights += 4; break; }
            case 'q': { board.castling_rights += 8; break; }
            default: break;
        }

        pos++;
    }

    pos++;

    // En passant square
    if(FEN[pos] != '-'){
        int fl = FEN[pos] - 'a';
        int rk = 8 - (FEN[pos + 1] - '0');
        board.en_passant_square = (8 * rk) + fl;
        pos += 2;
    } else{
        board.en_passant_square = Square::NO_SQUARE;
        pos++;
    }

    pos++;

    // Halfmove clock
    int halfmove_clock_end_index = FEN.find(' ', pos);
    board.halfmove_clock = std::stoi(FEN.substr(pos, halfmove_clock_end_index - pos));

    pos = halfmove_clock_end_index + 1;

    // Fullmove number
    int FEN_end_index = FEN.find('\0', pos);
    board.fullmove_number = std::stoi(FEN.substr(pos, FEN_end_index - pos));
}

void PrintMoveToTerminal(uint16_t move){
    uint16_t mask;
    mask = (1ULL << 4) - 1;
    std::cout << "flags: " << ((move >> 12) & mask) << " | ";

    mask = (1ULL << 6) - 1;
    std::cout << "to " << IToSq(((move >> 6) & mask)) << " ";

    mask = (1ULL << 6) - 1;
    std::cout << "from " << IToSq(((move >> 0) & mask));
    
    std::cout << "\n";
}

void PrintMoveListToTerminal(MoveList list){
    std::cout << "count: " << list.count << "\n";
    for(int i = 0; i < list.count; i++){
        uint16_t mask;

        std::cout << "([" << i << "] ";

        mask = (1ULL << 4) - 1;
        std::cout << "flags: " << ((list.list[i] >> 12) & mask) << " | ";

        mask = (1ULL << 6) - 1;
        std::cout << "to " << IToSq(((list.list[i] >> 6) & mask)) << " ";

        mask = (1ULL << 6) - 1;
        std::cout << "from " << IToSq(((list.list[i] >> 0) & mask));
        
        std::cout << ")  ";
    }
}

/*  Flags and move guide
    0  (0000) - quiet move
    1  (0001) - double pawn push
    2  (0010) - KS castle
    3  (0011) - QS castle
    4  (0100) - normal capture
    5  (0101) - en passant capture
    6  (0110) - 
    7  (0111) - 
    8  (1000) - quiet knight promotion
    9  (1001) - quiet bishop promotion
    10 (1010) - quiet rook promotion
    11 (1011) - quiet queen promotion
    12 (1100) - capture knight promotion
    13 (1101) - capture bishop promotion
    14 (1110) - capture rook promotion
    15 (1111) - capture queen promotion

    a move is of the form --> | 4 bit flags | 6 bit target | 6 bit source |
*/
uint16_t AssembleMove(int source, int target, int flag){
    return (source | (target << 6) | (flag << 12));
}

// |-----------------|
// | Move Generation |---------------------------------------------------------
// |-----------------|

void GeneratePseudoLegalMoves(MoveList& list){
    int flag = 0;
    u64 index = 0;
    list.count = 0;
    u64 bitboard;
    u64 O;
    u64 attacks;
    u64 total = board.total_occ;
    u64 friendly = board.colour_occ[board.to_move];
    u64 enemy = board.colour_occ[!board.to_move];
    int target;

    // White pawns
    if(board.to_move == Colour::white){
        bitboard = board.pieces[Colour::white][Piece::pawn];

        u64 EP_bitboard = (board.en_passant_square == Square::NO_SQUARE) ? 0ULL : (1ULL << board.en_passant_square);

        // The following attack boards must be disjoint
        // Single pushes (excluding promotions)
        u64 single_pushes = ((bitboard & NOT_RANK_7) >> 8) & ~total;

        // Quiet promotions
        u64 quiet_promotions = ((bitboard & RANK_7) >> 8) & ~total;

        // Double pushes
        u64 double_pushes = ((single_pushes & RANK_3) >> 8) & ~total;

        // Strictly normal captures
        u64 normal_captures_left = ((bitboard & NOT_FILE_A & NOT_RANK_7) >> 9) & enemy;
        u64 normal_captures_right = ((bitboard & NOT_FILE_H & NOT_RANK_7) >> 7) & enemy;

        // En-passant captures
        u64 en_passant_captures_left = ((bitboard & NOT_FILE_A) >> 9) & EP_bitboard;
        u64 en_passant_captures_right = ((bitboard & NOT_FILE_H) >> 7) & EP_bitboard;

        // Promotion captures
        u64 promotion_captures_left = ((bitboard & NOT_FILE_A & RANK_7) >> 9) & enemy;
        u64 promotion_captures_right = ((bitboard & NOT_FILE_H & RANK_7) >> 7) & enemy;

        // Assemble moves from each
        while(single_pushes){ target = GetLSBitIndex(single_pushes); index = target + 8;
            list.list[list.count++] = AssembleMove(index, target, MoveFlag::quiet_move);
        single_pushes &= single_pushes - 1; }

        while(quiet_promotions){ target = GetLSBitIndex(quiet_promotions); index = target + 8;
            list.list[list.count++] = AssembleMove(index, target, MoveFlag::quiet_knight_promo);
            list.list[list.count++] = AssembleMove(index, target, MoveFlag::quiet_bishop_promo);
            list.list[list.count++] = AssembleMove(index, target, MoveFlag::quiet_rook_promo);
            list.list[list.count++] = AssembleMove(index, target, MoveFlag::quiet_queen_promo);
        quiet_promotions &= quiet_promotions - 1; }

        while(double_pushes){ target = GetLSBitIndex(double_pushes); index = target + 16;
            list.list[list.count++] = AssembleMove(index, target, MoveFlag::double_pawn_push);
        double_pushes &= double_pushes - 1; }

        while(normal_captures_left){ target = GetLSBitIndex(normal_captures_left); index = target + 9;
            list.list[list.count++] = AssembleMove(index, target, MoveFlag::normal_capture);
        normal_captures_left &= normal_captures_left - 1; }

        while(normal_captures_right){ target = GetLSBitIndex(normal_captures_right); index = target + 7;
            list.list[list.count++] = AssembleMove(index, target, MoveFlag::normal_capture);
        normal_captures_right &= normal_captures_right - 1; }

        while(en_passant_captures_left){ target = GetLSBitIndex(en_passant_captures_left); index = target + 9;
            list.list[list.count++] = AssembleMove(index, target, MoveFlag::EP_capture);
        en_passant_captures_left &= en_passant_captures_left - 1; }
    
        while(en_passant_captures_right){ target = GetLSBitIndex(en_passant_captures_right); index = target + 7;
            list.list[list.count++] = AssembleMove(index, target, MoveFlag::EP_capture);
        en_passant_captures_right &= en_passant_captures_right - 1; }

        while(promotion_captures_left){ target = GetLSBitIndex(promotion_captures_left); index = target + 9;
            list.list[list.count++] = AssembleMove(index, target, MoveFlag::capture_knight_promo);
            list.list[list.count++] = AssembleMove(index, target, MoveFlag::capture_bishop_promo);
            list.list[list.count++] = AssembleMove(index, target, MoveFlag::capture_rook_promo);
            list.list[list.count++] = AssembleMove(index, target, MoveFlag::capture_queen_promo);
        promotion_captures_left &= promotion_captures_left - 1; }

        while(promotion_captures_right){ target = GetLSBitIndex(promotion_captures_right); index = target + 7;
            list.list[list.count++] = AssembleMove(index, target, MoveFlag::capture_knight_promo);
            list.list[list.count++] = AssembleMove(index, target, MoveFlag::capture_bishop_promo);
            list.list[list.count++] = AssembleMove(index, target, MoveFlag::capture_rook_promo);
            list.list[list.count++] = AssembleMove(index, target, MoveFlag::capture_queen_promo);
        promotion_captures_right &= promotion_captures_right - 1; }
    }

    // Black pawns
    else{
        bitboard = board.pieces[Colour::black][Piece::pawn];

        u64 EP_bitboard = (board.en_passant_square == Square::NO_SQUARE) ? 0ULL : (1ULL << board.en_passant_square);

        // The following attack boards must be disjoint
        // Single pushes (excluding promotions)
        u64 single_pushes = ((bitboard & NOT_RANK_2) << 8) & ~total;

        // Quiet promotions
        u64 quiet_promotions = ((bitboard & RANK_2) << 8) & ~total;

        // Double pushes
        u64 double_pushes = ((single_pushes & RANK_6) << 8) & ~total;

        // Strictly normal captures
        u64 normal_captures_left = ((bitboard & NOT_FILE_A & NOT_RANK_2) << 7) & enemy;
        u64 normal_captures_right = ((bitboard & NOT_FILE_H & NOT_RANK_2) << 9) & enemy;

        // En-passant captures
        u64 en_passant_captures_left = ((bitboard & NOT_FILE_A) << 7) & EP_bitboard;
        u64 en_passant_captures_right = ((bitboard & NOT_FILE_H) << 9) & EP_bitboard;

        // Promotion captures
        u64 promotion_captures_left = ((bitboard & NOT_FILE_A & RANK_2) << 7) & enemy;
        u64 promotion_captures_right = ((bitboard & NOT_FILE_H & RANK_2) << 9) & enemy;

        // Assemble moves from each
        while(single_pushes){ target = GetLSBitIndex(single_pushes); index = target - 8;
            list.list[list.count++] = AssembleMove(index, target, MoveFlag::quiet_move);
        single_pushes &= single_pushes - 1; }

        while(quiet_promotions){ target = GetLSBitIndex(quiet_promotions); index = target - 8;
            list.list[list.count++] = AssembleMove(index, target, MoveFlag::quiet_knight_promo);
            list.list[list.count++] = AssembleMove(index, target, MoveFlag::quiet_bishop_promo);
            list.list[list.count++] = AssembleMove(index, target, MoveFlag::quiet_rook_promo);
            list.list[list.count++] = AssembleMove(index, target, MoveFlag::quiet_queen_promo);
        quiet_promotions &= quiet_promotions - 1; }

        while(double_pushes){ target = GetLSBitIndex(double_pushes); index = target - 16;
            list.list[list.count++] = AssembleMove(index, target, MoveFlag::double_pawn_push);
        double_pushes &= double_pushes - 1; }

        while(normal_captures_left){ target = GetLSBitIndex(normal_captures_left); index = target - 7;
            list.list[list.count++] = AssembleMove(index, target, MoveFlag::normal_capture);
        normal_captures_left &= normal_captures_left - 1; }

        while(normal_captures_right){ target = GetLSBitIndex(normal_captures_right); index = target - 9;
            list.list[list.count++] = AssembleMove(index, target, MoveFlag::normal_capture);
        normal_captures_right &= normal_captures_right - 1; }

        while(en_passant_captures_left){ target = GetLSBitIndex(en_passant_captures_left); index = target - 7;
            list.list[list.count++] = AssembleMove(index, target, MoveFlag::EP_capture);
        en_passant_captures_left &= en_passant_captures_left - 1; }
    
        while(en_passant_captures_right){ target = GetLSBitIndex(en_passant_captures_right); index = target - 9;
            list.list[list.count++] = AssembleMove(index, target, MoveFlag::EP_capture);
        en_passant_captures_right &= en_passant_captures_right - 1; }

        while(promotion_captures_left){ target = GetLSBitIndex(promotion_captures_left); index = target - 7;
            list.list[list.count++] = AssembleMove(index, target, MoveFlag::capture_knight_promo);
            list.list[list.count++] = AssembleMove(index, target, MoveFlag::capture_bishop_promo);
            list.list[list.count++] = AssembleMove(index, target, MoveFlag::capture_rook_promo);
            list.list[list.count++] = AssembleMove(index, target, MoveFlag::capture_queen_promo);
        promotion_captures_left &= promotion_captures_left - 1; }

        while(promotion_captures_right){ target = GetLSBitIndex(promotion_captures_right); index = target - 9;
            list.list[list.count++] = AssembleMove(index, target, MoveFlag::capture_knight_promo);
            list.list[list.count++] = AssembleMove(index, target, MoveFlag::capture_bishop_promo);
            list.list[list.count++] = AssembleMove(index, target, MoveFlag::capture_rook_promo);
            list.list[list.count++] = AssembleMove(index, target, MoveFlag::capture_queen_promo);
        promotion_captures_right &= promotion_captures_right - 1; }   
    }

    // Knights
    bitboard = board.pieces[board.to_move][Piece::knight];

    while(bitboard){
        index = GetLSBitIndex(bitboard);
        attacks = knight_attacks[index] & ~friendly;

        while(attacks){
            target = GetLSBitIndex(attacks);
            
            // Capture
            if(GetBit(enemy, target)){ list.list[list.count++] = AssembleMove(index, target, MoveFlag::normal_capture); }
            // Quiet move
            else{ list.list[list.count++] = AssembleMove(index, target, MoveFlag::quiet_move); }
            
            // VVV This trick always pops the least significant bit
            attacks &= attacks - 1;
        }

        bitboard &= bitboard - 1;
    }

    // Bishops
    bitboard = board.pieces[board.to_move][Piece::bishop];

    while(bitboard){
        index = GetLSBitIndex(bitboard);
        O = bishop_rays_no_edges[index] & total;
        attacks = bishop_attacks[index][HashBishopOccConfig(index, O)] & ~friendly;

        while(attacks){
            target = GetLSBitIndex(attacks);

            // Capture
            if(GetBit(enemy, target)){ list.list[list.count++] = AssembleMove(index, target, MoveFlag::normal_capture); }
            // Quiet move
            else{ list.list[list.count++] = AssembleMove(index, target, MoveFlag::quiet_move); }

            attacks &= attacks - 1;
        }

        bitboard &= bitboard - 1;
    }

    // Rooks
    bitboard = board.pieces[board.to_move][Piece::rook];

    while(bitboard){
        index = GetLSBitIndex(bitboard);
        O = rook_rays_no_edges[index] & total;
        attacks = rook_attacks[index][HashRookOccConfig(index, O)] & ~friendly;

        while(attacks){
            target = GetLSBitIndex(attacks);

            // Capture
            if(GetBit(enemy, target)){ list.list[list.count++] = AssembleMove(index, target, MoveFlag::normal_capture); }
            // Quiet move
            else{ list.list[list.count++] = AssembleMove(index, target, MoveFlag::quiet_move); }

            attacks &= attacks - 1;
        }

        bitboard &= bitboard - 1;
    }

    // Queens
    bitboard = board.pieces[board.to_move][Piece::queen];

    while(bitboard){
        index = GetLSBitIndex(bitboard);
        O = bishop_rays_no_edges[index] & total;
        u64 attacksB = bishop_attacks[index][HashBishopOccConfig(index, O)] & ~friendly;
        O = rook_rays_no_edges[index] & total;
        u64 attacksR = rook_attacks[index][HashRookOccConfig(index, O)] & ~friendly;
        attacks = attacksB | attacksR;

        while(attacks){
            target = GetLSBitIndex(attacks);

            // Capture
            if(GetBit(enemy, target)){ list.list[list.count++] = AssembleMove(index, target, MoveFlag::normal_capture); }
            // Quiet move
            else{ list.list[list.count++] = AssembleMove(index, target, MoveFlag::quiet_move); }
        
            attacks &= attacks - 1;
        }

        bitboard &= bitboard - 1;
    }

    // King: normal 1-square moves
    bitboard = board.pieces[board.to_move][Piece::king];

    while(bitboard){
        index = GetLSBitIndex(board.pieces[board.to_move][Piece::king]);
        attacks = king_attacks[index] & ~friendly;

        while(attacks){
            target = GetLSBitIndex(attacks);

            // Capture
            if(GetBit(enemy, target)){ list.list[list.count++] = AssembleMove(index, target, MoveFlag::normal_capture); }
            // Quiet move
            else{ list.list[list.count++] = AssembleMove(index, target, MoveFlag::quiet_move); }

            attacks &= attacks - 1;
        }

        bitboard &= bitboard - 1;
    }

    // White king: castling
    if(board.to_move == Colour::white){

        // KS
        if(
            (board.castling_rights & 0b00000001) &&
            !(CASTLE_WHITE_KS_EMPTY_SQUARES & total) &&
            !board.SquareAttackedBy(Square::e1, Colour::black) &&
            !board.SquareAttackedBy(Square::f1, Colour::black) &&
            !board.SquareAttackedBy(Square::g1, Colour::black)
        ){
            list.list[list.count++] = AssembleMove(Square::e1, Square::g1, MoveFlag::KS_castle);
        }

        // QS
        if(
            (board.castling_rights & 0b00000010) &&
            !(CASTLE_WHITE_QS_EMPTY_SQUARES & total) &&
            !board.SquareAttackedBy(Square::e1, Colour::black) &&
            !board.SquareAttackedBy(Square::d1, Colour::black) &&
            !board.SquareAttackedBy(Square::c1, Colour::black)
        ){
            list.list[list.count++] = AssembleMove(Square::e1, Square::c1, MoveFlag::QS_castle);
        }
    }

    // Black king: castling
    else{

        // KS
        if(
            (board.castling_rights & 0b00000100) &&
            !(CASTLE_BLACK_KS_EMPTY_SQUARES & total) &&
            !board.SquareAttackedBy(Square::e8, Colour::white) &&
            !board.SquareAttackedBy(Square::f8, Colour::white) &&
            !board.SquareAttackedBy(Square::g8, Colour::white)
        ){
            list.list[list.count++] = AssembleMove(Square::e8, Square::g8, MoveFlag::KS_castle);
        }

        // QS
        if(
            (board.castling_rights & 0b00001000) &&
            !(CASTLE_BLACK_QS_EMPTY_SQUARES & total) &&
            !board.SquareAttackedBy(Square::e8, Colour::white) &&
            !board.SquareAttackedBy(Square::d8, Colour::white) &&
            !board.SquareAttackedBy(Square::c8, Colour::white)
        ){
            list.list[list.count++] = AssembleMove(Square::e8, Square::c8, MoveFlag::QS_castle);
        }
    }
}

// |---------|
// | Testing |-----------------------------------------------------------------
// |---------|

void perft(int depth){
    if(depth == 0){
        nodes++; return;
    }

    MoveList list; GeneratePseudoLegalMoves(list);
    for(int i = 0; i < list.count; i++){
        UnmakeMoveGameState IrrInfo = board.MakeMove(list.list[i], board.to_move);
        if(board.InCheck(static_cast<Colour>(!board.to_move))){ board.UnmakeMove(list.list[i], board.to_move, IrrInfo); continue; }
        perft(depth - 1);
        board.UnmakeMove(list.list[i], board.to_move, IrrInfo);
    }

    return;
}