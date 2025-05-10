/************************************************************************
 *
 *  This is a skeleton to guide development of Othello engines that is intended
 *  to be used with the Ingenious Framework.
 *
 *  The skeleton has a simple random strategy that can be used as a starting
 *  point. Currently the master thread (rank 0) runs the random strategy and
 *  handles the communication with the referee, and the worker threads
 *  do nothing. Some form of backtracking algorithm, minimax, negamax,
 *  alpha-beta pruning etc. in parallel should be implemented.
 *
 *  Therfore, the skeleton code provided can be modified and altered to implement
 *  different strategies for the Othello game. However, the flow of
 *  communication with the referee, relies on the Ingenious Framework and should
 *  not be changed.
 *
 *  Each engine is wrapped in a process which communicates with the referee, by
 *  sending and receiving messages via the server hosted by the Ingenious
 *  Framework.
 *
 *  The communication enumes are defined in comms.h and are as follows:
 *      - GENERATE_MOVE: Referee is asking for a move to be made.
 *      - PLAY_MOVE: Referee is forwarding the opponent's move. For this engine
 *        to update the board state.
 *     - MATCH_RESET: Referee is asking for the board to be reset. Likely, for
 *        another game.
 *     - GAME_TERMINATION: Referee is asking for the game to be terminated.
 *
 *  IMPORTANT NOTE FOR DEBBUGING:
 *      - Print statements to stdout will most likely not be visible when
 *        running the engine with the Ingenious Framework. Therefore, it is
 *        recommended to print to a log file instead. The pointer to the log
 *        file is passed to the initialise_master function.
 *
 ************************************************************************/
#include "comms.h"
#include <arpa/inet.h>
#include <mpi.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#define BOARD_SIZE 8
#define EMPTY -1
#define BLACK 0
#define WHITE 1

#define MAX_MOVES 64

const char *PLAYER_NAME_LOG = "random_player.log";

void run_master(int, char *[]);
int initialise_master(int, char *[], int *, int *, FILE **);

void initialise_board(void);
void free_board(void);
void print_board(FILE *);
void reset_board(FILE *);

void run_worker(int);

int random_strategy(int, FILE *);
void legal_moves(int *, int *, int);
int check_direction(int, int, int, int, int, int);
void make_move(int, int);
void flip_direction(int, int, int, int, int);

int *board;

int main(int argc, char *argv[]) {
    int rank;

    if (argc != 5) {
        printf("Usage: %s <inetaddress> <port> <time_limit> <player_colour>\n",
               argv[0]);
        return 1;
    }

    MPI_Init(&argc, &argv);
    MPI_Comm_rank(MPI_COMM_WORLD, &rank);

    /* each process initialises their own board */
    initialise_board();

    if (rank == 0) {
        run_master(argc, argv);
    } else {
        run_worker(rank);
    }

    free_board();

    MPI_Finalize();
    return 0;
}

/**
 * Runs the master process.
 *
 * @param argc command line argument count
 * @param argv command line argument vector
 */
void run_master(int argc, char *argv[]) {
    int msg_type, time_limit, my_colour, my_move, opp_move, running;
    FILE *fp;
    char *move;

    running = initialise_master(argc, argv, &time_limit, &my_colour, &fp);

    while (running) {

        msg_type = receive_message(&opp_move);
        if (msg_type == GENERATE_MOVE) { /* referee is asking for a move */

            my_move = random_strategy(my_colour, fp);

            if (my_move != -1) {
                make_move(my_move, my_colour);
                fprintf(fp, "\nPlacing piece in row: %d, column %d\n",
                        my_move / BOARD_SIZE, my_move % BOARD_SIZE);
            } else {
                fprintf(fp, "\n Only move is to pass\n");
            }

            /* convert move to char */
            move = malloc(sizeof(char) * 10);
            sprintf(move, "%d\n", my_move);
            send_move(move);
            free(move);

        } else if (msg_type ==
                   PLAY_MOVE) { /* referee is forwarding opponents move */

            fprintf(fp, "\nOpponent placing piece in row: %d, column %d\n",
                    opp_move / BOARD_SIZE, opp_move % BOARD_SIZE);

            if (opp_move < 0) {
                fprintf(fp, "\nOpponent had no moves, therefore passed.");
                continue;
            }

            make_move(opp_move, (my_colour + 1) % 2);

        } else if (msg_type == GAME_TERMINATION) {
            fprintf(fp, "Game terminated.\n");
            fflush(fp);
            running = 0;
        } else if (msg_type == MATCH_RESET) {
            fprintf(fp, "Match reset.\n");
            my_colour = (my_colour + 1) % 2;
            reset_board(fp);
        } else if (msg_type == UNKNOWN) {
            fprintf(fp, "Received unknown message type from referee.\n");
            fflush(fp);
            running = 0;
        }

        if (msg_type == GENERATE_MOVE || msg_type == PLAY_MOVE ||
            msg_type == MATCH_RESET) {
            print_board(fp);
            fprintf(fp, "message type: %d\n", msg_type);
            fflush(fp);
        };
    }
}

/**
 * Runs the worker process.
 *
 * @param rank rank of the worker process
 */
void run_worker(int rank) {
    /*
    int running;

    while (running) {

    }

    */
}

/**
 * Resets the board to the initial state.
 *
 * @param fp pointer to the log file
 */
void reset_board(FILE *fp) {

    int mid = BOARD_SIZE / 2;
    memset(board, EMPTY, sizeof(int) * BOARD_SIZE * BOARD_SIZE);

    // Set up the initial four pieces in the middle
    board[mid * BOARD_SIZE + mid] = WHITE;
    board[(mid - 1) * BOARD_SIZE + (mid - 1)] = WHITE;
    board[mid * BOARD_SIZE + (mid - 1)] = BLACK;
    board[(mid - 1) * BOARD_SIZE + mid] = BLACK;

    fprintf(fp, "~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~\n");
    fprintf(fp, "~~~~~~~~~~~~~ NEW MATCH ~~~~~~~~~~~~\n");
    fprintf(fp, "~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~\n");

    fprintf(fp, "New board state:\n");
}

/**
 * Runs a random strategy. Chooses a random legal move and applies it to the
 * board, then returns the move in the form of an integer (0-361).
 *
 * @param my_colour colour of the player
 * @param fp pointer to the log file
 */
int random_strategy(int my_colour, FILE *fp) {
    int number_of_moves;
    int *moves = malloc(sizeof(int) * MAX_MOVES);

    /* get all legal moves */
    legal_moves(moves, &number_of_moves, my_colour);

    /* check for pass */
    if (number_of_moves <= 0 || moves[0] == -1) {
        fprintf(fp, "\nNo legal moves, passing.\n");
        free(moves);
        return -1;
    }

    /* choose a random move */
    srand((unsigned int)time(NULL));
    int random_index = rand() % number_of_moves;
    int move = moves[random_index];

    free(moves);
    return move;
}

void flip_direction(int x, int y, int dx, int dy, int my_colour) {
    int i = x + dx;
    int j = y + dy;

    // Move along the direction and flip pieces until we hit a piece of
    // my_colour
    while (i >= 0 && i < BOARD_SIZE && j >= 0 && j < BOARD_SIZE &&
           board[i * BOARD_SIZE + j] != my_colour) {
        board[i * BOARD_SIZE + j] = my_colour;
        i += dx;
        j += dy;
    }
}

/**
 * Applies the given move to the board.
 *
 * @param move move to apply
 * @param my_colour colour of the player
 */
void make_move(int move, int colour) {
    int row = move / BOARD_SIZE;
    int col = move % BOARD_SIZE;
    int opp_colour = (colour == WHITE) ? BLACK : WHITE;
    board[row * BOARD_SIZE + col] = colour;

    // Check and flip in all 8 directions
    for (int dx = -1; dx <= 1; dx++) {
        for (int dy = -1; dy <= 1; dy++) {
            if (dx == 0 && dy == 0)
                continue; // Skip the current cell

            int i = row + dx;
            int j = col + dy;
            int found_opp = 0;

            // Move in the direction and check for opponent's pieces followed by
            // my piece
            while (i >= 0 && i < BOARD_SIZE && j >= 0 && j < BOARD_SIZE) {
                if (board[i * BOARD_SIZE + j] == opp_colour) {
                    found_opp = 1;
                    i += dx;
                    j += dy;
                } else if (board[i * BOARD_SIZE + j] == colour && found_opp) {
                    flip_direction(row, col, dx, dy, colour);
                    break; // Stop checking this direction as we've found a
                           // valid line
                } else {
                    break; // No valid line in this direction
                }
            }
        }
    }
}

/**
 * Checks if the given direction is valid. A direction is valid if it sandwiches
 * at least one of the opponent's pieces between the piece being placed and
 * another piece of the player's colour.
 *
 * @param x x-coordinate of the piece being placed
 * @param y y-coordinate of the piece being placed
 * @param dx x-direction to check
 * @param dy y-direction to check
 * @param my_colour colour of the player
 * @param opp_colour colour of the opponent
 * @return 1 if the direction is valid, 0 otherwise
 */
int check_direction(int x, int y, int dx, int dy, int my_colour,
                    int opp_colour) {
    int i = x + dx;
    int j = y + dy;
    int found_opp = 0; // Flag to check if at least one opponent piece is found

    while (i >= 0 && i < BOARD_SIZE && j >= 0 && j < BOARD_SIZE) {
        if (board[i * BOARD_SIZE + j] == opp_colour) {
            found_opp = 1;
            i += dx;
            j += dy;
        } else if (board[i * BOARD_SIZE + j] == my_colour && found_opp) {
            return 1; // Valid direction as it sandwiches opponent's pieces
        } else {
            return 0; // Either empty or own piece without sandwiching
                      // opponent's pieces
        }
    }
    return 0;
}

/**
 * Gets a list of legal moves for the current board, and stores them in the
 * moves array followed by a -1. Also stores the number of legal moves in the
 * number_of_moves variable.
 *
 * What is a legal move? A legal move is a move that results in at least one of
 * the opponent's pieces being flipped. That is if there is at least one piece
 * of the opponent's colour between the piece being placed and another piece of
 * the player's colour.
 *
 * @param moves array to store the legal moves in
 * @param number_of_moves variable to store the number of legal moves in
 */
void legal_moves(int *moves, int *number_of_moves, int my_colour) {
    int opp_colour = (my_colour == WHITE) ? BLACK : WHITE;
    *number_of_moves = 0;

    for (int i = 0; i < BOARD_SIZE; i++) {
        for (int j = 0; j < BOARD_SIZE; j++) {
            if (board[i * BOARD_SIZE + j] != EMPTY)
                continue;

            int moveFound =
                0; // Flag to indicate if a legal move is found for the cell

            // Check all 8 directions from the current cell
            for (int dx = -1; dx <= 1 && !moveFound; dx++) {
                for (int dy = -1; dy <= 1 && !moveFound; dy++) {
                    if (dx == 0 && dy == 0)
                        continue; // Skip checking the current cell

                    if (check_direction(i, j, dx, dy, my_colour, opp_colour)) {
                        moves[(*number_of_moves)++] = i * BOARD_SIZE + j;
                        moveFound = 1; // A legal move is found, no need to
                                       // check other directions
                    }
                }
            }
        }
    }

    moves[*number_of_moves] = -1; // End of moves
}

/**
 * Initialises the board for the game.
 */
void initialise_board(void) {
    int mid = BOARD_SIZE / 2;

    board = malloc(sizeof(int) * BOARD_SIZE * BOARD_SIZE);
    memset(board, EMPTY, sizeof(int) * BOARD_SIZE * BOARD_SIZE);

    /* plave initial pieces */
    board[mid * BOARD_SIZE + mid] = WHITE;
    board[(mid - 1) * BOARD_SIZE + (mid - 1)] = WHITE;
    board[mid * BOARD_SIZE + (mid - 1)] = BLACK;
    board[(mid - 1) * BOARD_SIZE + mid] = BLACK;
}

/**
 * Prints the board to the given file with improved aesthetics.
 *
 * @param fp pointer to the file to print to
 */
void print_board(FILE *fp) {
    if (fp == NULL) {
        return; // File pointer is not valid
    }

    fprintf(fp, "  ");
    for (int i = 0; i < BOARD_SIZE; ++i) {
        fprintf(fp, "%d ", i); // Print column numbers
    }
    fprintf(fp, "\n");

    for (int i = 0; i < BOARD_SIZE; ++i) {
        fprintf(fp, "%d ", i); // Print row numbers
        for (int j = 0; j < BOARD_SIZE; ++j) {
            if (board[i * BOARD_SIZE + j] == EMPTY) {
                fprintf(fp, ". "); // Print a dot for empty spaces
            } else if (board[i * BOARD_SIZE + j] == BLACK) {
                fprintf(fp, "B "); // Print B for Black pieces
            } else if (board[i * BOARD_SIZE + j] == WHITE) {
                fprintf(fp, "W "); // Print W for White pieces
            }
        }
        fprintf(fp, "\n");
    }
}

/**
 * Frees the memory allocated for the board.
 */
void free_board(void) { free(board); }

/**
 * Initialises the master process for communication with the IF wrapper and set
 * up the log file.
 * @param argc command line argument count
 * @param argv command line argument vector
 * @param time_limit time limit for the game
 * @param my_colour colour of the player
 * @param fp pointer to the log file
 * @return 1 if initialisation was successful, 0 otherwise
 */
int initialise_master(int argc, char *argv[], int *time_limit, int *my_colour,
                      FILE **fp) {
    unsigned long int ip = inet_addr(argv[1]);
    int port = atoi(argv[2]);
    *time_limit = atoi(argv[3]);
    *my_colour = atoi(argv[4]);

    /* open file for logging */
    *fp = fopen(PLAYER_NAME_LOG, "w");

    if (*fp == NULL) {
        printf("Could not open log file\n");
        return 0;
    }

    fprintf(*fp, "Initialising communication.\n");

    /* initialise comms to IF wrapper */
    if (!initialise_comms(ip, port)) {
        printf("Could not initialise comms\n");
        return 0;
    }

    fprintf(*fp, "Communication initialised \n");

    fprintf(*fp, "Let the game begin...\n");
    fprintf(*fp, "My name: %s\n", PLAYER_NAME_LOG);
    fprintf(*fp, "My colour: %d\n", *my_colour);
    fprintf(*fp, "Board size: %d\n", BOARD_SIZE);
    fprintf(*fp, "Time limit: %d\n", *time_limit);
    fprintf(*fp, "-----------------------------------\n");
    print_board(*fp);

    fflush(*fp);

    return 1;
}
