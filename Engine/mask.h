#include <iostream>
#include <string>
#include <cstdint>
#include <bitset>
#include <random>
#include <vector>
#include <algorithm>

typedef std::uint64_t u64;

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
enum CastlingRights { white_ks, white_qs, black_ks, black_qs };
enum MoveFlag { 
    quiet_move, double_pawn_push, KS_castle, QS_castle, normal_capture, EP_capture, six, seven, quiet_knight_promo, quiet_bishop_promo, quiet_rook_promo, quiet_queen_promo,
    capture_knight_promo, capture_bishop_promo, capture_rook_promo, capture_queen_promo
};

// For pregenerating attack tables
const u64 FILE_A = 0b0000000100000001000000010000000100000001000000010000000100000001;
const u64 FILE_B = 0b0000001000000010000000100000001000000010000000100000001000000010;
const u64 FILE_G = 0b0100000001000000010000000100000001000000010000000100000001000000;
const u64 FILE_H = 0b1000000010000000100000001000000010000000100000001000000010000000;
const u64 NOT_FILE_A = ~FILE_A;
const u64 NOT_FILE_AB = ~(FILE_A | FILE_B);
const u64 NOT_FILE_H = ~FILE_H;
const u64 NOT_FILE_GH = ~(FILE_G | FILE_H);
const u64 RANK_1 = 0b1111111100000000000000000000000000000000000000000000000000000000;
const u64 RANK_2 = 0b0000000011111111000000000000000000000000000000000000000000000000;
const u64 RANK_3 = 0b0000000000000000111111110000000000000000000000000000000000000000;
const u64 RANK_6 = 0b0000000000000000000000000000000000000000111111110000000000000000;
const u64 RANK_7 = 0b0000000000000000000000000000000000000000000000001111111100000000;
const u64 RANK_8 = 0b0000000000000000000000000000000000000000000000000000000011111111;
const u64 NOT_RANK_1 = ~RANK_1;
const u64 NOT_RANK_2 = ~RANK_2;
const u64 NOT_RANK_7 = ~RANK_7;
const u64 NOT_RANK_8 = ~RANK_8;

// For generating castling moves
const u64 CASTLE_WHITE_KS_EMPTY_SQUARES = 0b0110000000000000000000000000000000000000000000000000000000000000;
const u64 CASTLE_WHITE_QS_EMPTY_SQUARES = 0b0000111000000000000000000000000000000000000000000000000000000000;
const u64 CASTLE_BLACK_KS_EMPTY_SQUARES = 0b0000000000000000000000000000000000000000000000000000000001100000;
const u64 CASTLE_BLACK_QS_EMPTY_SQUARES = 0b0000000000000000000000000000000000000000000000000000000000001110;

// For making moves
const u64 CASTLE_WHITE_KS_KING_XOR = 0b0101000000000000000000000000000000000000000000000000000000000000;
const u64 CASTLE_WHITE_KS_ROOK_XOR = 0b1010000000000000000000000000000000000000000000000000000000000000;
const u64 CASTLE_WHITE_KS_OCC_XOR = 0b1111000000000000000000000000000000000000000000000000000000000000;
const u64 CASTLE_WHITE_QS_KING_XOR = 0b0001010000000000000000000000000000000000000000000000000000000000;
const u64 CASTLE_WHITE_QS_ROOK_XOR = 0b0000100100000000000000000000000000000000000000000000000000000000;
const u64 CASTLE_WHITE_QS_OCC_XOR = 0b0001110100000000000000000000000000000000000000000000000000000000;
const u64 CASTLE_BLACK_KS_KING_XOR = 0b0000000000000000000000000000000000000000000000000000000001010000;
const u64 CASTLE_BLACK_KS_ROOK_XOR = 0b0000000000000000000000000000000000000000000000000000000010100000;
const u64 CASTLE_BLACK_KS_OCC_XOR = 0b0000000000000000000000000000000000000000000000000000000011110000;
const u64 CASTLE_BLACK_QS_KING_XOR = 0b0000000000000000000000000000000000000000000000000000000000010100;
const u64 CASTLE_BLACK_QS_ROOK_XOR = 0b0000000000000000000000000000000000000000000000000000000000001001;
const u64 CASTLE_BLACK_QS_OCC_XOR = 0b0000000000000000000000000000000000000000000000000000000000011101;

// Bitboard utilities
#define GetBitBinary(bitboard, square) (!!((bitboard) & (1ULL << (square))))

void SetBit(u64& bitboard, int square){ // To 1
    ((bitboard) |= (1ULL << (square)));
}

u64 GetBit(u64 bitboard, int square){
    return ((bitboard) & (1ULL << (square)));
}

void PopBit(u64& bitboard, int square){ // To 0
    ((bitboard) &= ~(1ULL << (square)));
}

int GetLSBitIndex(u64 bitboard){
    return __builtin_ctzll(bitboard);
}

int NumberOfNonZeroBits(u64 bitboard){
    int count = 0;

    while(bitboard){
        count++;
        GetLSBitIndex(bitboard);
        bitboard &= bitboard - 1;
    }

    return count;
}

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
        default: { return "-"; }
    }
}

// ---

// Attacks (diagonally) only
u64 pawn_attacks[2][64];

// ---

u64 knight_attacks[64];

// ---

u64 bishop_rays_no_edges[64];
u64 bishop_rays_no_edges_occ_configs[64][512];
u64 bishop_magic_numbers[64] = {
    0x40040844404084ULL,
    0x2004208a004208ULL,
    0x10190041080202ULL,
    0x108060845042010ULL,
    0x581104180800210ULL,
    0x2112080446200010ULL,
    0x1080820820060210ULL,
    0x3c0808410220200ULL,
    0x4050404440404ULL,
    0x21001420088ULL,
    0x24d0080801082102ULL,
    0x1020a0a020400ULL,
    0x40308200402ULL,
    0x4011002100800ULL,
    0x401484104104005ULL,
    0x801010402020200ULL,
    0x400210c3880100ULL,
    0x404022024108200ULL,
    0x810018200204102ULL,
    0x4002801a02003ULL,
    0x85040820080400ULL,
    0x810102c808880400ULL,
    0xe900410884800ULL,
    0x8002020480840102ULL,
    0x220200865090201ULL,
    0x2010100a02021202ULL,
    0x152048408022401ULL,
    0x20080002081110ULL,
    0x4001001021004000ULL,
    0x800040400a011002ULL,
    0xe4004081011002ULL,
    0x1c004001012080ULL,
    0x8004200962a00220ULL,
    0x8422100208500202ULL,
    0x2000402200300c08ULL,
    0x8646020080080080ULL,
    0x80020a0200100808ULL,
    0x2010004880111000ULL,
    0x623000a080011400ULL,
    0x42008c0340209202ULL,
    0x209188240001000ULL,
    0x400408a884001800ULL,
    0x110400a6080400ULL,
    0x1840060a44020800ULL,
    0x90080104000041ULL,
    0x201011000808101ULL,
    0x1a2208080504f080ULL,
    0x8012020600211212ULL,
    0x500861011240000ULL,
    0x180806108200800ULL,
    0x4000020e01040044ULL,
    0x300000261044000aULL,
    0x802241102020002ULL,
    0x20906061210001ULL,
    0x5a84841004010310ULL,
    0x4010801011c04ULL,
    0xa010109502200ULL,
    0x4a02012000ULL,
    0x500201010098b028ULL,
    0x8040002811040900ULL,
    0x28000010020204ULL,
    0x6000020202d0240ULL,
    0x8918844842082200ULL,
    0x4010011029020020ULL
};
const int bishop_magic_bitshifts[64] = {
    58, 59, 59, 59, 59, 59, 59, 58, 
    59, 59, 59, 59, 59, 59, 59, 59, 
    59, 59, 57, 57, 57, 57, 59, 59, 
    59, 59, 57, 55, 55, 57, 59, 59, 
    59, 59, 57, 55, 55, 57, 59, 59, 
    59, 59, 57, 57, 57, 57, 59, 59, 
    59, 59, 59, 59, 59, 59, 59, 59, 
    58, 59, 59, 59, 59, 59, 59, 58
};
u64 bishop_attacks[64][512];
u64 HashBishopOccConfig(int sq, u64 occ_config){
    return ((occ_config * bishop_magic_numbers[sq]) >> bishop_magic_bitshifts[sq]);
}

// ---

u64 rook_rays_no_edges[64];
u64 rook_rays_no_edges_occ_configs[64][4096];
u64 rook_magic_numbers[64] = {
    0x8a80104000800020ULL,
    0x140002000100040ULL,
    0x2801880a0017001ULL,
    0x100081001000420ULL,
    0x200020010080420ULL,
    0x3001c0002010008ULL,
    0x8480008002000100ULL,
    0x2080088004402900ULL,
    0x800098204000ULL,
    0x2024401000200040ULL,
    0x100802000801000ULL,
    0x120800800801000ULL,
    0x208808088000400ULL,
    0x2802200800400ULL,
    0x2200800100020080ULL,
    0x801000060821100ULL,
    0x80044006422000ULL,
    0x100808020004000ULL,
    0x12108a0010204200ULL,
    0x140848010000802ULL,
    0x481828014002800ULL,
    0x8094004002004100ULL,
    0x4010040010010802ULL,
    0x20008806104ULL,
    0x100400080208000ULL,
    0x2040002120081000ULL,
    0x21200680100081ULL,
    0x20100080080080ULL,
    0x2000a00200410ULL,
    0x20080800400ULL,
    0x80088400100102ULL,
    0x80004600042881ULL,
    0x4040008040800020ULL,
    0x440003000200801ULL,
    0x4200011004500ULL,
    0x188020010100100ULL,
    0x14800401802800ULL,
    0x2080040080800200ULL,
    0x124080204001001ULL,
    0x200046502000484ULL,
    0x480400080088020ULL,
    0x1000422010034000ULL,
    0x30200100110040ULL,
    0x100021010009ULL,
    0x2002080100110004ULL,
    0x202008004008002ULL,
    0x20020004010100ULL,
    0x2048440040820001ULL,
    0x101002200408200ULL,
    0x40802000401080ULL,
    0x4008142004410100ULL,
    0x2060820c0120200ULL,
    0x1001004080100ULL,
    0x20c020080040080ULL,
    0x2935610830022400ULL,
    0x44440041009200ULL,
    0x280001040802101ULL,
    0x2100190040002085ULL,
    0x80c0084100102001ULL,
    0x4024081001000421ULL,
    0x20030a0244872ULL,
    0x12001008414402ULL,
    0x2006104900a0804ULL,
    0x1004081002402ULL
};
const int rook_magic_bitshifts[64] = {
    52, 53, 53, 53, 53, 53, 53, 52,
    53, 54, 54, 54, 54, 54, 54, 53,
    53, 54, 54, 54, 54, 54, 54, 53,
    53, 54, 54, 54, 54, 54, 54, 53,
    53, 54, 54, 54, 54, 54, 54, 53,
    53, 54, 54, 54, 54, 54, 54, 53,
    53, 54, 54, 54, 54, 54, 54, 53,
    52, 53, 53, 53, 53, 53, 53, 52,
};
u64 rook_attacks[64][4096];
u64 HashRookOccConfig(int sq, u64 occ_config){
    return ((occ_config * rook_magic_numbers[sq]) >> rook_magic_bitshifts[sq]);
}

// ---

u64 king_attacks[64];

// ------------

void PrecomputePawnAttacksTable(){
    for(int sq = 0; sq < 64; sq++){
        u64 place = 1ULL << sq;
        pawn_attacks[Colour::white][sq] = 0ULL;
        pawn_attacks[Colour::white][sq] = 0ULL;
 
        // Left captures white
        pawn_attacks[Colour::white][sq] |= ((place & NOT_FILE_A) >> 9);

        // Right captures white
        pawn_attacks[Colour::white][sq] |= ((place & NOT_FILE_H) >> 7);
    
        // Left captures black
        pawn_attacks[Colour::black][sq] |= ((place & NOT_FILE_A) << 7);

        // Right captures black
        pawn_attacks[Colour::black][sq] |= ((place & NOT_FILE_H) << 9);
    }
}

// ---

void PrecomputeKnightAttacksTable(){
    for(int sq = 0; sq < 64; sq++){
        u64 place = 1ULL << sq;

        // Eight squares read top --> bottom, left --> right on the board, so RIGHT bitshifts take you UP the board
        knight_attacks[sq] = (
            ((place & NOT_FILE_A) >> 17) | ((place & NOT_FILE_H) >> 15) |
            ((place & NOT_FILE_AB) >> 10) | ((place & NOT_FILE_GH) >> 6) |
            ((place & NOT_FILE_AB) << 6) | ((place & NOT_FILE_GH) << 10) |
            ((place & NOT_FILE_A) << 15) | ((place & NOT_FILE_H) << 17)
        );
    }
}

// ---

void PrecomputeBishopRaysNoEdges(){
    for(int sq = 0; sq < 64; sq++){
        u64 place = 1ULL << sq;
        bishop_rays_no_edges[sq] = 0ULL;
        int difference;

        // Up left
        difference = 9;
        while(!(place & RANK_8) && !(place & FILE_A) && !((place >> difference) & RANK_8) && !((place >> difference) & FILE_A)){
            bishop_rays_no_edges[sq] |= (place >> difference);
            difference += 9;
        }

        // Up right
        difference = 7;
        while(!(place & RANK_8) && !(place & FILE_H) && !((place >> difference) & RANK_8) && !((place >> difference) & FILE_H)){
            bishop_rays_no_edges[sq] |= (place >> difference);
            difference += 7;
        }

        // Down left
        difference = 7;
        while(!(place & RANK_1) && !(place & FILE_A) && !((place << difference) & RANK_1) && !((place << difference) & FILE_A)){
            bishop_rays_no_edges[sq] |= (place << difference);
            difference += 7;
        }

        // Down right
        difference = 9;
        while(!(place & RANK_1) && !(place & FILE_H) && !((place << difference) & RANK_1) && !((place << difference) & FILE_H)){
            //std::cout << sq << ": " << difference << "\n";
            bishop_rays_no_edges[sq] |= (place << difference);
            difference += 9;
        }
    }
}

void PrecomputeBishopRaysNoEdgesOccConfigs(){
    PrecomputeBishopRaysNoEdges();
    u64 mask;

    for(int sq = 0; sq < 64; sq++){

        for(int ci = 0; ci < 512; ci++){
            mask = bishop_rays_no_edges[sq];

            // Prepare the ci-th occupancy config
            u64 entry = 0ULL;

            // Loop over the mask with an indexer
            int bit = 0;
            while(mask){
                int occ_square = GetLSBitIndex(mask);

                // This while-loop maps the bits of ci onto the bishop's rays (no edges). if the mask has less than 9 non-zero bits, there will be repetition
                if(GetBit(ci, bit)){ SetBit(entry, occ_square); }

                bit++;

                mask &= mask - 1;
            }

            // Testing purposes
            //std::bitset<64> x(entry);
            //std::cout << x << "\n";

            // Insert it
            bishop_rays_no_edges_occ_configs[sq][ci] = entry;
        }
    }
}

void PrecomputeBishopAttacksTable(){
    PrecomputeBishopRaysNoEdgesOccConfigs();

    for(int sq = 0; sq < 64; sq++){

        for(int ci = 0; ci < 512; ci++){
            u64 O = bishop_rays_no_edges_occ_configs[sq][ci];
            u64 hash = HashBishopOccConfig(sq, O);

            u64 place = 1ULL << sq;
            bishop_attacks[sq][hash] = 0ULL;
            int difference;

            // Up left
            difference = 9;
            while(!(place & RANK_8) && !(place & FILE_A)){
                place = place >> difference;
                bishop_attacks[sq][hash] |= place;
                if(GetBit(O, GetLSBitIndex(place))){ break; }
            }
            place = 1ULL << sq;

            // Up right
            difference = 7;
            while(!(place & RANK_8) && !(place & FILE_H)){
                place = place >> difference;
                bishop_attacks[sq][hash] |= place;
                if(GetBit(O, GetLSBitIndex(place))){ break; }
            }
            place = 1ULL << sq;

            // Down left
            difference = 7;
            while(!(place & RANK_1) && !(place & FILE_A)){
                place = place << difference;
                bishop_attacks[sq][hash] |= place;
                if(GetBit(O, GetLSBitIndex(place))){ break; }
            }
            place = 1ULL << sq;

            // Down right
            difference = 9;
            while(!(place & RANK_1) && !(place & FILE_H)){
                place = place << difference;
                bishop_attacks[sq][hash] |= place;
                if(GetBit(O, GetLSBitIndex(place))){ break; }
            }
            place = 1ULL << sq;
        }
    }
}

// ---

void PrecomputeRookRaysNoEdges(){
    for(int sq = 0; sq < 64; sq++){
        u64 place = 1ULL << sq;
        rook_rays_no_edges[sq] = 0ULL;
        int difference;

        // Up
        difference = 8;
        while(!(place & RANK_8) && !((place >> difference) & RANK_8)){
            rook_rays_no_edges[sq] |= (place >> difference);
            difference += 8;
        }

        // Left
        difference = 1;
        while(!(place & FILE_A) && !((place >> difference) & FILE_A)){
            rook_rays_no_edges[sq] |= (place >> difference);
            difference += 1;
        }

        // Right
        difference = 1;
        while(!(place & FILE_H) && !((place << difference) & FILE_H)){
            rook_rays_no_edges[sq] |= (place << difference);
            difference += 1;
        }

        // Down
        difference = 8;
        while(!(place & RANK_1) && !((place << difference) & RANK_1)){
            //std::cout << sq << ": " << difference << "\n";
            rook_rays_no_edges[sq] |= (place << difference);
            difference += 8;
        }
    }
}

void PrecomputeRookRaysNoEdgesOccConfigs(){
    PrecomputeRookRaysNoEdges();
    u64 mask;
    
    for(int sq = 0; sq < 64; sq++){

        for(int ci = 0; ci < 4096; ci++){
            mask = rook_rays_no_edges[sq];

            u64 entry = 0ULL;

            int bit = 0;
            while(mask){
                int occ_square = GetLSBitIndex(mask);

                if(GetBit(ci, bit)){ SetBit(entry, occ_square); }

                bit++;

                mask &= mask - 1;
            }

            rook_rays_no_edges_occ_configs[sq][ci] = entry;
        }
    }
}

void PrecomputeRookAttacksTable(){
    PrecomputeRookRaysNoEdgesOccConfigs();

    for(int sq = 0; sq < 64; sq++){

        for(int ci = 0; ci < 4096; ci++){
            u64 O = rook_rays_no_edges_occ_configs[sq][ci];
            u64 hash = HashRookOccConfig(sq, O);

            u64 place = 1ULL << sq;
            rook_attacks[sq][hash] = 0ULL;
            int difference;

            // Up
            difference = 8;
            while(!(place & RANK_8)){
                place = place >> difference;
                rook_attacks[sq][hash] |= place;
                if(GetBit(O, GetLSBitIndex(place))){ break; }
            }
            place = 1ULL << sq;

            // Left
            difference = 1;
            while(!(place & FILE_A)){
                place = place >> difference;
                rook_attacks[sq][hash] |= place;
                if(GetBit(O, GetLSBitIndex(place))){ break; }
            }
            place = 1ULL << sq;

            // Right
            difference = 1;
            while(!(place & FILE_H)){
                place = place << difference;
                rook_attacks[sq][hash] |= place;
                if(GetBit(O, GetLSBitIndex(place))){ break; }
            }
            place = 1ULL << sq;

            // Down
            difference = 8;
            while(!(place & RANK_1)){
                place = place << difference;
                rook_attacks[sq][hash] |= place;
                if(GetBit(O, GetLSBitIndex(place))){ break; }
            }
            place = 1ULL << sq;
        }
    }
}

// ---

void PrecomputeKingAttacksTable(){
    for(int sq = 0; sq < 64; sq++){
        u64 place = 1ULL << sq;

        king_attacks[sq] = (
            ((place & NOT_RANK_8 & NOT_FILE_A) >> 9) | ((place & NOT_RANK_8) >> 8) |
            ((place & NOT_RANK_8 & NOT_FILE_H) >> 7) | ((place & NOT_FILE_A) >> 1) |
            ((place & NOT_FILE_H) << 1) | ((place & NOT_FILE_A & NOT_RANK_1) << 7) |
            ((place & NOT_RANK_1) << 8) | ((place & NOT_RANK_1 & NOT_FILE_H) << 9)
        );
    }
}