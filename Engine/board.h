#include "mask.h"
#include <iostream>

std::string FEN = "r1bqkb1r/pppp1ppp/2n2n2/1B2p3/4P3/5N2/PPPP1PPP/RNBQK2R w KQkq - 0 1";

class Board {
public:
    u64 pieces[2][6] = {0};
    u64 colour_occ[2] = {0};
    u64 total_occ = 0;

    Colour to_move;
    uint8_t en_passant_square = Square::NO_SQUARE; // Can only be one square at a time
    bool castling_rights_white_ks = false; // Castling rights defaulted to false. Handled in FEN parser
    bool castling_rights_white_qs = false;
    bool castling_rights_black_ks = false;
    bool castling_rights_black_qs = false;
    int halfmove_clock = 0; // Since a pawn was advanced or a capture was made (for the 50 move rule)
    int fullmove_number = 0;

    Piece PieceAtSquare(int sq, Colour side){
        for(int i = Piece::pawn; i <= Piece::king; i++){
            if(pieces[side][i] & (1ULL << sq)){ return static_cast<Piece>(i); }
        }

        // Unreachable
        return Piece::pawn;
    }

    int MakeMove(uint16_t move, Colour side){
        int source = move & 0b0000000000111111;
        int target = (move & 0b0000111111000000) >> 6;
        int flag = (move & 0b1111000000000000) >> 12;

        Piece source_piece = PieceAtSquare(source, side);

        //std::cout << "\n\n" << source << " " << target << " " << flag << "\n";

        switch(flag){
            case MoveFlag::quiet_move:
            {
                //
            }
        }

        return 0;
    }
private:

};

Board board;

void PrintBitboardToTerminal(u64 bitboard){
    int index = 0;
    for(int i = 0; i < 8; i++){
        for(int j = 0; j < 8; j++){
            std::cout << GetBitBinary(bitboard, index) << "  ";
            index++;
        }
        std::cout << "\n";
    }
    std::cout << "\n";
}

void PrintBoardToTerminal(){
    std::cout << "\n-----------------------------------------------\n" << "to move: " << board.to_move << " | CR: " <<
    (int)board.castling_rights_white_ks << (int)board.castling_rights_white_qs << (int)board.castling_rights_black_ks << (int)board.castling_rights_black_qs <<
    " | EP: " << (int)board.en_passant_square << " | HMC: " << board.halfmove_clock << " | FMN: " << board.fullmove_number << "\n-----------------------------------------------\n";

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
            case 'K': { board.castling_rights_white_ks = true; break; }
            case 'Q': { board.castling_rights_white_qs = true; break; }
            case 'k': { board.castling_rights_black_ks = true; break; }
            case 'q': { board.castling_rights_black_qs = true; break; }
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

struct MoveList {
    uint16_t list[256];
    int count;
};

std::string IToSq(int index){
    switch(index){
        case 0: { return "a8"; } case 1: { return "b8"; } case 2: { return "c8"; } case 3: { return "d8"; }
        case 4: { return "e8"; } case 5: { return "f8"; } case 6: { return "g8"; } case 7: { return "h8"; }
        case 8: { return "a7"; } case 9: { return "b7"; } case 10: { return "c7"; } case 11: { return "d7"; }
        case 12: { return "e7"; } case 13: { return "f7"; } case 14: { return "g7"; } case 15: { return "h7"; }
        case 16: { return "a6"; } case 17: { return "b6"; } case 18: { return "c6"; } case 19: { return "d6"; }
        case 20: { return "e6"; } case 21: { return "f6"; } case 22: { return "g6"; } case 23: { return "h6"; }
        case 24: { return "a5"; } case 25: { return "b5"; } case 26: { return "c5"; } case 27: { return "d5"; }
        case 28: { return "e5"; } case 29: { return "f5"; } case 30: { return "g5"; } case 31: { return "h5"; }
        case 32: { return "a4"; } case 33: { return "b4"; } case 34: { return "c4"; } case 35: { return "d4"; }
        case 36: { return "e4"; } case 37: { return "f4"; } case 38: { return "g4"; } case 39: { return "h4"; }
        case 40: { return "a3"; } case 41: { return "b3"; } case 42: { return "c3"; } case 43: { return "d3"; }
        case 44: { return "e3"; } case 45: { return "f3"; } case 46: { return "g3"; } case 47: { return "h3"; }
        case 48: { return "a2"; } case 49: { return "b2"; } case 50: { return "c2"; } case 51: { return "d2"; }
        case 52: { return "e2"; } case 53: { return "f2"; } case 54: { return "g2"; } case 55: { return "h2"; }
        case 56: { return "a1"; } case 57: { return "b1"; } case 58: { return "c1"; } case 59: { return "d1"; }
        case 60: { return "e1"; } case 61: { return "f1"; } case 62: { return "g1"; } case 63: { return "h1"; }
        default: { return ""; }
    }
}

void PrintMoveListToTerminal(MoveList list){
    std::cout << "count: " << list.count << "\n";
    for(int i = 0; i < list.count; i++){
        uint16_t mask;

        std::cout << "(";

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

// Check if the given side is attacking the given square
bool SquareAttackedBy(int sq, Colour side){
    u64 O;
    u64 other = board.colour_occ[!side];
    u64 total = board.total_occ;

    // If the square can see the piece, the piece can see the square
    // By pawns
    if(pawn_attacks[!side][sq] & board.pieces[side][Piece::pawn]){ return true; }

    // By knights
    if(knight_attacks[sq] & board.pieces[side][Piece::knight]){ return true; }

    // By rooks (queens)
    O = rook_rays_no_edges[sq] & total;
    if((rook_attacks[sq][HashRookOccConfig(sq, O)] & ~other) & (board.pieces[side][Piece::rook] | board.pieces[side][Piece::queen])){ return true; }

    // By bishops (queens)
    O = bishop_rays_no_edges[sq] & total;
    if((bishop_attacks[sq][HashBishopOccConfig(sq, O)] & ~other) & (board.pieces[side][Piece::bishop] | board.pieces[side][Piece::queen])){ return true; }

    // By king
    if(king_attacks[sq] & board.pieces[side][Piece::king]){ return true; }

    // Not square is not attacked
    return false;
}

bool InCheck(Colour side){
    int king_sq = GetLSBitIndex(board.pieces[side][Piece::king]);
    return SquareAttackedBy(king_sq, static_cast<Colour>(!side));
}

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
        u64 en_passant_captures_left = ((bitboard & NOT_FILE_A) >> 9) & (1ULL << board.en_passant_square);
        u64 en_passant_captures_right = ((bitboard & NOT_FILE_H) >> 7) & (1ULL << board.en_passant_square);

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
        u64 en_passant_captures_left = ((bitboard & NOT_FILE_A) << 7) & (1ULL << board.en_passant_square);
        u64 en_passant_captures_right = ((bitboard & NOT_FILE_H) << 9) & (1ULL << board.en_passant_square);

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
            board.castling_rights_white_ks &&
            !(CASTLE_WHITE_KS_EMPTY_SQUARES & total) &&
            !SquareAttackedBy(Square::e1, Colour::black) &&
            !SquareAttackedBy(Square::f1, Colour::black) &&
            !SquareAttackedBy(Square::g1, Colour::black)
        ){
            list.list[list.count++] = AssembleMove(Square::e1, Square::g1, MoveFlag::KS_castle);
        }

        // QS
        if(
            board.castling_rights_white_qs &&
            !(CASTLE_WHITE_QS_EMPTY_SQUARES & total) &&
            !SquareAttackedBy(Square::e1, Colour::black) &&
            !SquareAttackedBy(Square::d1, Colour::black) &&
            !SquareAttackedBy(Square::c1, Colour::black)
        ){
            list.list[list.count++] = AssembleMove(Square::e1, Square::c1, MoveFlag::QS_castle);
        }
    }

    // Black king: castling
    else{

        // KS
        if(
            board.castling_rights_black_ks &&
            !(CASTLE_BLACK_KS_EMPTY_SQUARES & total) &&
            !SquareAttackedBy(Square::e8, Colour::white) &&
            !SquareAttackedBy(Square::f8, Colour::white) &&
            !SquareAttackedBy(Square::g8, Colour::white)
        ){
            list.list[list.count++] = AssembleMove(Square::e8, Square::g8, MoveFlag::KS_castle);
        }

        // QS
        if(
            board.castling_rights_black_qs &&
            !(CASTLE_BLACK_QS_EMPTY_SQUARES & total) &&
            !SquareAttackedBy(Square::e8, Colour::white) &&
            !SquareAttackedBy(Square::d8, Colour::white) &&
            !SquareAttackedBy(Square::c8, Colour::white)
        ){
            list.list[list.count++] = AssembleMove(Square::e8, Square::c8, MoveFlag::QS_castle);
        }
    }
}