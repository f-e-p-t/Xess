#include "mask.h"
#include <iostream>

std::string FEN = "8/8/8/8/8/8/8/8 w - - 0 1";

enum Piece { pawn, knight, bishop, rook, queen, king };
enum Colour { white, black };
enum Square { // Top-left ---> bottom-right as read
    a8, b8, c8, d8, e8, f8, g8, h8,
    a7, b7, c7, d7, e7, f7, g7, h7,
    a6, b6, c6, d6, e6, f6, g6, h6,
    a5, b5, c5, d5, e5, f5, g5, h5,
    a4, b4, c4, d4, e4, f4, g4, h4,
    a3, b3, c3, d3, e3, f3, g3, h3,
    a2, b2, c2, d2, e2, f2, g2, h2,
    a1, b1, c1, d1, e1, f1, g1, h1,
    NO_SQUARE
};
enum CastlingRights { white_ks = 1, white_qs = 2, black_ks = 4, black_qs = 8 };
enum MoveFlag { 
    quiet_move, double_pawn_push, KS_castle, QS_castle, normal_capture, EP_capture, quiet_knight_promo, six, seven, quiet_bishop_promo, quiet_rook_promo, quiet_queen_promo,
    capture_knight_promo, capture_bishop_promo, capture_rook_promo, capture_queen_promo
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

void GenerateMoves(MoveList& list){
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

        //while(bitboard){
        //    
        //}
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

    // Magic bitboards:
    // Pregenerate a [64] array of masks containing the attack squares for a piece on each square. for bishops these masks are 1 along the diagonals 0 elsewhere.
    // make sure to exclude the edges (we assume the piece can take any piece to begin with, then later filter out the friendly captures with bitwise operations)
    // apply the mask: O = bishop_diagonals[sq] & total_occ, giving what the bishop can see, colour-agnostic
    // hash via: (O x magic) >> (64 - n)
    // index into a lookup table to retrieve the legal moves
    // filter out the friendly captures
    // What is n? for a square sq on the board, n is the number of squares a bishop at that square can attack (excluding edges)
    // What is magic? for the square sq, we find n first. magic is a u64 st multiplication by magic is a perfect hash, i.e. injective.
    // Why not just use O as a key in a hashmap? too much overhead. magic centralises and packs the possibilities of O injectively into range 0 --> 2^n - 1
    // after a right bitshift by 64 - n. How does it do this? Its existence is proven, and it is calculated by brute force on initialisation

}