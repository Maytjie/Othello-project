#include <stdio.h>
#include <string.h>
#include "local_opponent.h"
#include "comms.h"

const int BL_PL = 1;
const int WH_PL = 2;

const int GAME_OVER = 3;
static int status = BL_PL;

int comms_init_network(int* my_colour, unsigned long ip, int port) {
    //In the local (no server) version ip is the colour of my_player
    if (ip == 1.0) {
	*my_colour = BL_PL;
        opponent_initialise(WH_PL);
	printf("Player colour = black\nOpponent colour = white\n");
    } else {
	*my_colour = WH_PL;
        opponent_initialise(BL_PL);
	printf("Player colour = white\nOpponent colour = black\n");
    }
    return SUCCESS;
}

int comms_get_cmd(char cmd[], char move[]) {
    if (status == BL_PL) {
        strncpy(cmd, "gen_move", CMDBUFSIZE);
    } else if (status == WH_PL) {
        opponent_gen_move(move);
        if (strncmp(move, "pass\n", MOVEBUFSIZE) == 0) {
            strncpy(cmd, "game_over", CMDBUFSIZE);
            status = GAME_OVER;
        } else {
            strncpy(cmd, "play_move", CMDBUFSIZE);
            status = BL_PL;
        }
    } else if (status == GAME_OVER) {
        strncpy(cmd, "game_over", CMDBUFSIZE);
    }
    return SUCCESS;
}

int comms_send_move(char player_move[]) {
    if (strncmp(player_move, "pass", MOVEBUFSIZE) == 0) {
        status = GAME_OVER;
    } else {
        opponent_apply_move(player_move);
        status = WH_PL;
    }
    return SUCCESS;
}
