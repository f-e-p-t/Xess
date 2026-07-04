#include "mask.h"
#include <iostream>

std::string FEN = "8/pppppppp/8/8/8/8/8/8 b - - 0 1";

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
    std::cout << "\n-----------------------------------------------\n" << "to move: " << board.to_move << " | CR: " << (int)board.castling_rights << " | EP: "
    << (int)board.en_passant_square << " | HMC: " << board.halfmove_clock << " | FMN: " << board.fullmove_number << "\n-----------------------------------------------\n";

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
            case 'K': { board.castling_rights |= CastlingRights::white_ks; break; }
            case 'Q': { board.castling_rights |= CastlingRights::white_qs; break; }
            case 'k': { board.castling_rights |= CastlingRights::black_ks; break; }
            case 'q': { board.castling_rights |= CastlingRights::black_qs; break; }
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
    u64 list[256];
    int count;
};

void PrintMoveListToTerminal(MoveList list){
    std::cout << "count: " << list.count << "\n";
    for(int i = 0; i < list.count; i++){
        uint16_t mask;

        std::cout << "(";

        mask = (1ULL << 4) - 1;
        std::cout << "flags: " << ((list.list[i] >> 12) & mask) << " | ";

        mask = (1ULL << 6) - 1;
        std::cout << "to " << ((list.list[i] >> 6) & mask) << " ";

        mask = (1ULL << 6) - 1;
        std::cout << "from " << ((list.list[i] >> 0) & mask);
        
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

    // Pawns (different cases for white and black)
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
}