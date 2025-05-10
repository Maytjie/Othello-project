/* vim: ai:sw=4:ts=4:sts:et */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <mpi.h>
#include <time.h>
#include <assert.h>
#include "local_opponent.h"

const int O_EMPTY = 0;
const int O_BLACK = 1;
const int O_WHITE = 2;
const int O_OUTER = 3;
const int O_MOVEBUFSIZE = 6;
const int O_ALLDIRECTIONS[8]={-11, -10, -9, -1, 1, 9, 10, 11};
const int O_BOARDSIZE=100;
const int O_LEGALMOVSBUFSIZE=65;
const char O_PIECENAMES[4] ={'.','b','w','?'};

void opponent_gen_move(char *move);
void opponent_play_move(char *move);
void opponent_game_over(void);
void opponent_initialise_board(void);
void opponent_free_board(void);

void opponent_legalmoves (int player, int *moves);
int opponent_legalp (int move, int player);
int opponent_validp (int move);
int opponent_wouldflip (int move, int dir, int player);
int opponent_opponent (int player);
int opponent_findbracketingpiece(int square, int dir, int player);
int opponent_randomstrategy(void);
void opponent_makemove (int move, int player);
void opponent_makeflips (int move, int dir, int player);
int opponent_get_loc(char* movestring);
void opponent_get_move_string(int loc, char *ms);
void opponent_printboard(void);
char opponent_nameof(int piece);
int opponent_count(int player, int* board);

int opponent_colour = O_WHITE;
int *opponent_board;
FILE *opponent_fp;

void opponent_initialise(int colour) {
    opponent_initialise_board();
    opponent_colour = colour;

    opponent_fp = fopen("log_opponent.txt", "w");
    fprintf(opponent_fp, "Opponent colour = ");
    if (opponent_colour == O_BLACK) fprintf(opponent_fp, "black\n");
    else fprintf(opponent_fp, "white\n");
    fflush(opponent_fp);
}

/*
    Called at the start of execution on all ranks
 */
void opponent_initialise_board(void){
    int i;
    opponent_board = (int *)malloc(O_BOARDSIZE * sizeof(int));
    for (i = 0; i<=9; i++) opponent_board[i]=O_OUTER;
    for (i = 10; i<=89; i++) {
        if (i%10 >= 1 && i%10 <= 8) opponent_board[i]=O_EMPTY; else opponent_board[i]=O_OUTER;
    }
    for (i = 90; i<=99; i++) opponent_board[i]=O_OUTER;
    opponent_board[44]=O_WHITE; opponent_board[45]=O_BLACK; opponent_board[54]=O_BLACK; opponent_board[55]=O_WHITE;
}

void opponent_free_board(void){
   free(opponent_board);
}

void opponent_gen_move(char *move){
    int loc;
    if (opponent_colour == O_EMPTY){
        opponent_colour = O_BLACK;
    }

    loc = opponent_randomstrategy();

    if (loc == -1){
        strncpy(move, "pass\n", O_MOVEBUFSIZE);
    } else {
        opponent_get_move_string(loc, move);
        opponent_makemove(loc, opponent_colour);
    }
    opponent_printboard();
}

/*
    Called when the other engine has made a move. The move is given in a
    string parameter of the form "xy", where x and y represent the row
    and column where the opponent's piece is placed, respectively.
 */
void opponent_play_move(char *move){
    int loc;
    if (opponent_colour == O_EMPTY){
        opponent_colour = O_WHITE;
    }
    if (strcmp(move, "pass") == 0){
        return;
    }
    loc = opponent_get_loc(move);
    opponent_makemove(loc, opponent_opponent(opponent_colour));
}

void opponent_game_over(void){
    opponent_free_board();
}

void opponent_get_move_string(int loc, char *ms){
    int row, col, new_loc;
    new_loc = loc - (9 + 2 * (loc / 10));
    row = new_loc / 8;
    col = new_loc % 8;
    ms[0] = row + '0';
    ms[1] = col + '0';
    ms[2] = '\n';
    ms[3] = 0;
}

int opponent_get_loc(char* movestring){
    int row, col;
    row = movestring[0] - '0';
    col = movestring[1] - '0';
    return (10 * (row)) + col;
}

void opponent_legalmoves (int player, int *moves) {
    int move, i;
    moves[0] = 0;
    i = 0;
    for (move=11; move<=88; move++)
        if (opponent_legalp(move, player)) {
            i++;
            moves[i]=move;
        }
    moves[0]=i;
}

int opponent_legalp (int move, int player) {
    int i;
    if (!opponent_validp(move)) return 0;
    if (opponent_board[move]==O_EMPTY) {
        i=0;
        while (i<=7 && !opponent_wouldflip(move, O_ALLDIRECTIONS[i], player)) i++;
        if (i==8) return 0; else return 1;
    }
    else return 0;
}

int opponent_validp (int move) {
    if ((move >= 11) && (move <= 88) && (move%10 >= 1) && (move%10 <= 8))
        return 1;
    else return 0;
}

int opponent_wouldflip (int move, int dir, int player) {
    int c;
    c = move + dir;
    if (opponent_board[c] == opponent_opponent(player))
        return opponent_findbracketingpiece(c+dir, dir, player);
    else return 0;
}

int opponent_findbracketingpiece(int square, int dir, int player) {
    while (opponent_board[square] == opponent_opponent(player)) square = square + dir;
    if (opponent_board[square] == player) return square;
    else return 0;
}

int opponent_opponent (int player) {
    if (player == O_WHITE) return O_BLACK;
    if (player == O_BLACK) return O_WHITE;
    printf("illegal player\n"); return O_EMPTY;
}

int opponent_randomstrategy(void) {
    int r;
    int *moves = (int *) malloc(O_LEGALMOVSBUFSIZE * sizeof(int));
    memset(moves, 0, O_LEGALMOVSBUFSIZE);

    opponent_legalmoves(opponent_colour, moves);
    if (moves[0] == 0){
        return -1;
    }
    srand (time(NULL));
    r = moves[(rand() % moves[0]) + 1];
    free(moves);
    return(r);
}

void opponent_makemove (int move, int player) {
    int i;
    opponent_board[move] = player;
    for (i=0; i<=7; i++) opponent_makeflips(move, O_ALLDIRECTIONS[i], player);
}

void opponent_makeflips (int move, int dir, int player) {
    int bracketer, c;
    bracketer = opponent_wouldflip(move, dir, player);
    if (bracketer) {
        c = move + dir;
        do {
            opponent_board[c] = player;
            c = c + dir;
        } while (c != bracketer);
    }
}

void opponent_printboard(void){
    int row, col;
    fprintf(opponent_fp,"   0 1 2 3 4 5 6 7 [%c=%d %c=%d]\n",
    opponent_nameof(O_BLACK), opponent_count(O_BLACK, opponent_board), opponent_nameof(O_WHITE), opponent_count(O_WHITE, opponent_board));
    for (row=1; row<=8; row++) {
        fprintf(opponent_fp,"%d  ", row);
        for (col=1; col<=8; col++)
            fprintf(opponent_fp,"%c ", opponent_nameof(opponent_board[col + (10 * row)]));
        fprintf(opponent_fp,"\n");
    }
    fflush(opponent_fp);
}



char opponent_nameof (int piece) {
    assert(0 <= piece && piece < 5);
    return(O_PIECENAMES[piece]);
}

int opponent_count (int player, int* board) {
    int i, cnt;
    cnt=0;
    for (i=1; i<=88; i++)
        if (board[i] == player) cnt++;
    return cnt;
}

void opponent_apply_move(char* move) {
    opponent_play_move(move);
}

void opponent_call_gen_move(char opponent_move[]) {
    memset(opponent_move, 0, O_MOVEBUFSIZE);
    opponent_gen_move(opponent_move);
}
