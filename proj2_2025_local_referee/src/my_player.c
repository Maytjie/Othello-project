/************************************************************************
 *
 *  This is a skeleton to guide development of Othello engines that is intended
 *  to be used with the Ingenious Framework.
 *
 *  The skeleton has a simple random strategy that can be used as a starting
 *  point. The master thread (rank 0) currently runs the random strategy and
 *  handles the communication with the referee, and the worker threads currently
 *  do nothing. Some form of backtracking algorithm, minimax, negamax,
 *  alpha-beta pruning etc. in parallel should be implemented.
 *
 *  Therfore, skeleton code provided can be modified and altered to implement
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
#define EMPTY 0
#define BLACK 1
#define WHITE 2

#define MAX_MOVES 64

const char *PLAYER_NAME_LOG = "my_player.log";

clock_t start;
double time_limit;

void run_master(int, char *[]);
int initialise_master(int, char *[], int *, int *, FILE **);

void initialise_board(void);
void free_board(void);
void print_board(FILE *);
void reset_board(FILE *);
void restore_board(int *);
void terminate_workers();
char* format_move(int, char *);
int evaluate_board_state(int);
int evaluate_moves(int, int, int, int, int);
int *copy_curr_board();
int get_loc(char *);

void run_worker(int);

int random_strategy(int, FILE *);
int minimax_strategy(int, int, FILE *);
int minimax(int, int, int, bool, int, int);
void legal_moves(int *, int *, int);
int check_direction(int, int, int, int, int, int);
void make_move(int, int);
int make_temp_move(int, int);
void flip_direction(int, int, int, int, int);
int best_legal_move(int, int, int, int, int);

int check_if_time_up();

int *board;

typedef struct {
    int move;
    int player_colour;
    int depth;
    int alpha;
    int beta;
} MoveTask;

typedef struct {
    int move;
    int score;
} MoveResult;

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
    char cmd[CMDBUFSIZE];
    char *my_move;
    char opponent_move[MOVEBUFSIZE];
    int time_limit;
    int my_colour, move, opp_move;
    int running = 0;
    FILE *fp = NULL;
    int is_workers_terminated = 0;

    if (initialise_master(argc, argv, &time_limit, &my_colour, &fp) != FAILURE) {
        running = 1;
    }
    
    if (my_colour == EMPTY)
        my_colour = BLACK;
    // Broadcast my_colour

    while (running == 1) {
        /* Receive next command from referee */
        if (comms_get_cmd(cmd, opponent_move) == FAILURE) {
            fprintf(fp, "Error getting cmd\n");
            fflush(fp);
            running = 0;
            break;
        }

        opp_move = get_loc(opponent_move);

        /* Received game_over message */
        if (strcmp(cmd, "game_over") == 0) {
            fprintf(fp, "Game terminated.\n");
            fflush(fp);
            running = 0;

            /* game termination logic goes here */
            if (!is_workers_terminated) {
                int size;
                MPI_Comm_size(MPI_COMM_WORLD, &size);

                for (int worker = 1; worker < size; worker++) {
                    int zero_moves = 0;
                    MPI_Send(&zero_moves, 1, MPI_INT, worker, 0, MPI_COMM_WORLD);

                    int command = -1;
                    MPI_Send(&command, 1, MPI_INT, worker, 1, MPI_COMM_WORLD);
                }
                is_workers_terminated = 1;
            }

            MPI_Abort(MPI_COMM_WORLD, EXIT_SUCCESS);
            return;

        /* Received gen_move message */
        } else if (strcmp(cmd, "gen_move") == 0) {
            /* generate move logic goes here */
            move = minimax_strategy(my_colour, time_limit, fp);

            if (move != -1) {
                /* apply move */
                make_move(move, my_colour);
                fprintf(fp, "\nPlacing piece in row: %d, column: %d\n", move / BOARD_SIZE, move % BOARD_SIZE);
            } else {
                fprintf(fp, "\n Only move is to pass\n");

                int size;
                MPI_Comm_size(MPI_COMM_WORLD, &size);

                for (int worker = 1; worker < size; worker++) {
                    int zero_moves = 0;
                    MPI_Send(&zero_moves, 1, MPI_INT, worker, 0, MPI_COMM_WORLD);

                    int command = 0;
                    MPI_Send(&command, 1, MPI_INT, worker, 1, MPI_COMM_WORLD);
                }
            }

            /* convert move to char */
            my_move = malloc(sizeof(char) * 10);
            format_move(move, my_move);

            if (comms_send_move(my_move) == FAILURE) {
                running = 0;
                fprintf(fp, "Move send failed\n");
                fflush(fp);

                int size;
                MPI_Comm_size(MPI_COMM_WORLD, &size);

                for (int worker = 1; worker < size; worker++) {
                    int zero_moves = 0;
                    MPI_Send(&zero_moves, 1, MPI_INT, worker, 0, MPI_COMM_WORLD);

                    int command = -1;
                    MPI_Send(&command, 1, MPI_INT, worker, 1, MPI_COMM_WORLD);
                }

                free(my_move);
                break;
            }

            free(my_move);
            print_board(fp);

        /* Received opponent's move (play_move mesage) */
        } else if (strcmp(cmd, "play_move") == 0) {
            if (opp_move < 0) {
                fprintf(fp, "\nOpponent had no moves, therefore passed.");
                continue;
            }

            fprintf(fp, "\nOpponent placing piece in row: %d, column: %d\n", opp_move / BOARD_SIZE, opp_move % BOARD_SIZE);
            make_move(opp_move, (my_colour + 1) % 2);
            print_board(fp);

            /* Received unknown message */
        } else {
            fprintf(fp, "Received unknown command from referee\n");
        }
        
    }
    // END OF GAME
    if (!is_workers_terminated) {
        terminate_workers();
    }
}

/**
 * Runs the worker process.
 *
 * @param rank rank of the worker process
 */
void run_worker(int rank) { 
    MPI_Status status;
    int running = 1;

    while (running) {
        int number_of_moves;
        MPI_Recv(&number_of_moves, 1, MPI_INT, 0, 0, MPI_COMM_WORLD, &status);

        int command;
        MPI_Recv(&command, 1, MPI_INT, 0, 1, MPI_COMM_WORLD, &status);

        if (command == -1) {
            running = 0;
            continue;
        }

        if (number_of_moves <= 0) {
            continue;
        }

        MoveTask *tasks = malloc(sizeof(MoveTask) * number_of_moves);
        MoveResult *results = malloc(sizeof(MoveResult) * number_of_moves);

        for (int i = 0; i < number_of_moves; i++) {
            MPI_Recv(&tasks[i], sizeof(MoveTask), MPI_BYTE, 0, 2, MPI_COMM_WORLD, &status);
        }

        int compl_results = 0;

        for (int i = 0; i < number_of_moves; i++) {
            int flag;
            MPI_Iprobe(0, 5, MPI_COMM_WORLD, &flag, &status);

            if (flag) {
                int new_alpha;
                MPI_Recv(&new_alpha, 1, MPI_INT, 0, 5, MPI_COMM_WORLD, &status);

                if (new_alpha > tasks[i].alpha) {
                    tasks[i].alpha = new_alpha;
                }
            }

            int score = evaluate_moves(tasks[i].move, tasks[i].player_colour, tasks[i].depth, tasks[i].alpha, tasks[i].beta);

            results[compl_results].move = tasks[i].move;
            results[compl_results].score = score;
            compl_results++;
        }

        MPI_Send(&compl_results, 1, MPI_INT, 0, 3, MPI_COMM_WORLD);

        for (int i = 0; i < compl_results; i++) {
            MPI_Send(&results[i], sizeof(MoveResult), MPI_BYTE, 0, 4, MPI_COMM_WORLD);
        }

        free(tasks);
        free(results);

    }
        
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

int minimax_strategy(int my_player_colour, int time, FILE *fp) {
    int *moves_available = malloc(sizeof(int) * MAX_MOVES);
    int number_of_moves;
    int rank, size;

    MPI_Comm_rank(MPI_COMM_WORLD, &rank);
    MPI_Comm_size(MPI_COMM_WORLD, &size);

    legal_moves(moves_available, &number_of_moves, my_player_colour);

    if (number_of_moves <= 0) {
        free(moves_available);
        return -1;
    }

    start = clock();
    time_limit = (double) time;

    int best_possible_move = moves_available[0];
    int max_depth_compl = 0;

    for (int depth = 1; depth < 10; depth++) {
        if (check_if_time_up()) {
            break;
        }

        int number_of_workers = size - 1;
        
        if (number_of_workers <= 0) {
            best_possible_move = best_legal_move(my_player_colour, depth, -999999, 999999, time_limit);

            if (!check_if_time_up()) {
                max_depth_compl = depth;
            } else {
                break;
            }
            continue;
        }

        int alpha = -999999;
        int beta = 999999;

        int tasks_per_worker = (number_of_moves + number_of_workers - 1) / number_of_workers;

        int num_tasks_sent = 0;
        
        for (int worker = 1; worker <= number_of_workers && num_tasks_sent < number_of_moves; worker++) {
            int moves_to_be_sent = (num_tasks_sent + tasks_per_worker <= number_of_moves) ? tasks_per_worker : (number_of_moves - num_tasks_sent);

            if (moves_to_be_sent <= 0) continue;

            MPI_Send(&moves_to_be_sent, 1, MPI_INT, worker, 0, MPI_COMM_WORLD);

            int command = 0;
            MPI_Send(&command, 1, MPI_INT, worker, 1, MPI_COMM_WORLD);

            for (int j = 0; j < moves_to_be_sent; j++) {
                MoveTask task;
                task.move = moves_available[num_tasks_sent + j];
                task.player_colour = my_player_colour;
                task.depth = depth;
                task.alpha = alpha;
                task.beta = beta;

                MPI_Send(&task, sizeof(MoveTask), MPI_BYTE, worker, 2, MPI_COMM_WORLD);
            }

            num_tasks_sent += moves_to_be_sent;
        }
        
        int curr_best_move = -1;
        int best_possible_score = -999999;
        int tasks_compl = 0;

        while (tasks_compl < num_tasks_sent) {
            if (check_if_time_up()) {
                break;
            }

            int flag = 0;
            MPI_Status status;
            MPI_Iprobe(MPI_ANY_SOURCE, 3, MPI_COMM_WORLD, &flag, &status);

            if (!flag) {
                continue;
            }

            int worker_id = status.MPI_SOURCE;
            int number_of_results;

            MPI_Recv(&number_of_results, 1, MPI_INT, worker_id, 3, MPI_COMM_WORLD, MPI_STATUS_IGNORE);

            for (int j = 0; j < number_of_results; j++) {
                MoveResult results;

                MPI_Recv(&results, sizeof(MoveResult), MPI_BYTE, worker_id, 4, MPI_COMM_WORLD, MPI_STATUS_IGNORE);

                if (results.score > best_possible_score) {
                    best_possible_score = results.score;
                    curr_best_move = results.move;

                    alpha = best_possible_score;

                    for (int k = 1; k <= number_of_workers; k++) {
                        MPI_Send(&alpha, 1, MPI_INT, k, 5, MPI_COMM_WORLD);
                    }
                }
                tasks_compl++;
            }
        }
        
        if (!check_if_time_up() && curr_best_move != -1) {
            best_possible_move = curr_best_move;
            max_depth_compl = depth;

        } else {
            break;
        }
        
    }
    

    for (int worker = 1; worker < size; worker++) {
        int zero_moves = 0;
        MPI_Send(&zero_moves, 1, MPI_INT, worker, 0, MPI_COMM_WORLD);

        int command = 0;
        MPI_Send(&command, 1, MPI_INT, worker, 1, MPI_COMM_WORLD);
    }

    free(moves_available);
    return best_possible_move;
}

int minimax(int depth, int alpha, int beta, bool maximizing, int player_colour, int curr_colour) {

    if (depth <= 0 || depth > 10 || check_if_time_up()) {
        return evaluate_board_state(player_colour);
    }

    int *moves_available = malloc(sizeof(int) * MAX_MOVES);
    int number_of_moves;

    legal_moves(moves_available, &number_of_moves, curr_colour);

    if (number_of_moves <= 0) {
        int opp_colour = (curr_colour + 1) % 2;
        int *opp_moves_available = malloc(sizeof(int) * MAX_MOVES);
        int opp_number_of_moves;

        legal_moves(opp_moves_available, &opp_number_of_moves, opp_colour);

        if (opp_number_of_moves <= 0) {
            free(opp_moves_available);
            free(moves_available);

            return evaluate_board_state(player_colour);
        }

        int score = minimax(depth - 1, alpha, beta, !maximizing, player_colour, opp_colour);
        
        free(opp_moves_available);
        free(moves_available);

        return score;
    }

    int best_possible_score;

    if (maximizing) {
        best_possible_score = -9999999;

        for (int i = 0; i < number_of_moves; i++) {
            int *curr_board_copy = copy_curr_board();

            make_temp_move(moves_available[i], curr_colour);

            int score = minimax(depth - 1, alpha, beta, false, player_colour, (curr_colour + 1) % 2);

            restore_board(curr_board_copy);

            if (score > best_possible_score) {
                best_possible_score = score;
            }

            if (alpha < best_possible_score) {
                alpha = best_possible_score;
            }

            if (beta <= alpha) {
                break;
            }
        }

        free(moves_available);

        return best_possible_score;

    } else {
        best_possible_score = 9999999;
        
        for (int i = 0; i < number_of_moves; i++) {
            int *curr_board_copy = copy_curr_board();
            
            make_temp_move(moves_available[i], curr_colour);

            int score = minimax(depth - 1, alpha, beta, true, player_colour, (curr_colour + 1) % 2);

            restore_board(curr_board_copy);

            if (score < best_possible_score) {
                best_possible_score = score;
            }

            if (beta > best_possible_score) {
                beta = best_possible_score;
            }

            if (beta <= alpha) {
                break;
            }
        }

        free(moves_available);

        return best_possible_score;
    }
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
    if (!comms_init_network(my_colour, ip, port)) {
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

int evaluate_board_state(int my_player_colour) {
    int my_player_pieces = 0;
    int opponent_pieces = 0;

    int opponent_colour = (my_player_colour + 1) % 2;
    
    int score = 0;

    int my_player_edges = 0;
    int opponent_edges = 0;

    int my_player_corners = 0;
    int opponent_corners = 0;

    for (int i = 0; i < BOARD_SIZE; i++) {
        for (int j = 0; j < BOARD_SIZE; j++) {
            if (board[i * BOARD_SIZE + j] == my_player_colour) {
                my_player_pieces++;

                if ((i == 0 || i == BOARD_SIZE - 1) && (j == 0 || j == BOARD_SIZE - 1)) {
                    my_player_corners++;
                } else if (i == 0 || j == 0 || i == BOARD_SIZE - 1 || j == BOARD_SIZE - 1) {
                    my_player_edges++;
                }

            } else if (board[i * BOARD_SIZE + j] == opponent_colour) {
                opponent_pieces++;

                if ((i == 0 || i == BOARD_SIZE - 1) && (j == 0 || j == BOARD_SIZE - 1)) {
                    opponent_corners++;
                } else if (i == 0 || j == 0 || i == BOARD_SIZE - 1 || j == BOARD_SIZE - 1) {
                    opponent_edges++;
                }

            }
        }
    }

    int piece_score = my_player_pieces - opponent_pieces;
    int edge_score = my_player_edges - opponent_edges;
    int corner_score = my_player_corners - opponent_corners;

    int *my_player_moves = malloc(sizeof(int) * MAX_MOVES);
    int *opponent_moves = malloc(sizeof(int) * MAX_MOVES);
    int my_player_num_moves, opponent_num_moves;

    legal_moves(my_player_moves, &my_player_num_moves, my_player_colour);
    legal_moves(opponent_moves, &opponent_num_moves, opponent_colour);

    int move_score = my_player_num_moves - opponent_num_moves;

    int num_spaces = BOARD_SIZE * BOARD_SIZE;
    int num_empty_spaces = num_spaces - my_player_pieces - opponent_pieces;
    
    if (num_empty_spaces > 2 * num_spaces / 3) {
        piece_score = piece_score * 0.1;
        edge_score = edge_score * 5;
        corner_score = corner_score * 25;
        move_score = move_score * 3;
    } else if (num_empty_spaces > 1 * num_spaces / 3) {
        piece_score = piece_score * 0.5;
        edge_score = edge_score * 3;
        corner_score = corner_score * 15;
        move_score = move_score * 2;
    } else {
        piece_score = piece_score * 2;
        edge_score = edge_score * 1.5;
        corner_score = corner_score * 10;
        move_score = move_score * 0.3;
    }

    score = piece_score + edge_score + corner_score + move_score;

    free(my_player_moves);
    free(opponent_moves);

    return score;
}

int evaluate_moves(int move, int player_colour, int depth, int alpha, int beta) {
    fprintf(stderr, "evaluate_moves: Starting for move %d at depth %d\n", move, depth); // Debug point N
    
    int *board_copy = copy_curr_board();

    make_temp_move(move, player_colour);

    fprintf(stderr, "evaluate_moves: Calling minimax for move %d at depth %d\n", move, depth); // Debug point O
   
    int possible_score = minimax(depth - 1, alpha, beta, false, player_colour, (player_colour + 1) % 2);
    
    fprintf(stderr, "evaluate_moves: minimax returned %d for move %d\n", possible_score, move); // Debug point P
    
    restore_board(board_copy);

    return possible_score;
}

int make_temp_move(int temp_move, int player_colour) {
    if (temp_move >= 0) {
        make_move(temp_move, player_colour);
        return 1;
    }

    return 0;
}

int *copy_curr_board() {
    int *curr_board_copy = malloc(sizeof(int) * BOARD_SIZE * BOARD_SIZE);
    memcpy(curr_board_copy, board, sizeof(int) * BOARD_SIZE * BOARD_SIZE);
    
    return curr_board_copy;
}

void restore_board(int *curr_board_copy) {
    memcpy(board, curr_board_copy, sizeof(int) * BOARD_SIZE * BOARD_SIZE);
    free(curr_board_copy);
}

int best_legal_move(int my_player_colour, int depth, int alpha, int beta, int time_limit) {
    int *moves_available = malloc(sizeof(int) * MAX_MOVES);
    int number_of_moves;

    legal_moves(moves_available, &number_of_moves, my_player_colour);

    if (number_of_moves <= 0) {
        free(moves_available);
        return -1;
    }

    int best_possible_move = moves_available[0];
    int best_possible_score = -9999999;

    for (int i = 0; i < number_of_moves; i++) {
        if (check_if_time_up()) {
            break;
        }

        int *curr_board_copy = copy_curr_board();

        make_temp_move(moves_available[i], my_player_colour);

        int score = minimax(depth - 1, alpha, beta, false, my_player_colour, (my_player_colour + 1) % 2);

        restore_board(curr_board_copy);

        if (score > best_possible_score) {
            best_possible_score = score;
            best_possible_move = moves_available[i];

            if (alpha < best_possible_score) {
                alpha = best_possible_score;
            }
        }
    }

    free(moves_available);

    return best_possible_move;
}

int check_if_time_up() {
    clock_t curr_time = clock();
    double elapsed_time = ((double)(curr_time - start)) / CLOCKS_PER_SEC;

    return elapsed_time > (time_limit - 1.0);
}

int get_loc(char *movestring) {
    // Check for "pass" move
    if (movestring[0] == 'p') {
        return -1;
    }
    
    // Parse the move string from the opponent (1-based)
    int row = movestring[0] - '0';
    int col = movestring[1] - '0';
    
    // Convert to 0-based for your board
    row = row - 1;
    col = col - 1;
    
    // Ensure valid range
    if (row < 0 || row >= BOARD_SIZE || col < 0 || col >= BOARD_SIZE) {
        fprintf(stderr, "Warning: Received invalid move coordinates: %d,%d\n", row, col);
        return -1;
    }
    
    // Return position in your board representation
    return (row * BOARD_SIZE) + col;
}

char* format_move(int position, char *move_str) {
    if (position == -1) {
        strcpy(move_str, "pass");
        return move_str;
    }
    
    // Convert to row and column (0-based)
    int row = position / BOARD_SIZE;
    int col = position % BOARD_SIZE;
    
    // Convert to 1-based for opponent
    row = row + 1;
    col = col + 1;
    
    // Format as a string
    sprintf(move_str, "%d%d", row, col);
    
    return move_str;
}

void terminate_workers() {
    int size;
    MPI_Comm_size(MPI_COMM_WORLD, &size);

    if (size > 1) {
        for (int worker = 1; worker < size; worker++) {
            int zero_moves = 0;
            MPI_Send(&zero_moves, 1, MPI_INT, worker, 0, MPI_COMM_WORLD);

            int command = -1;
            MPI_Send(&command, 1, MPI_INT, worker, 1, MPI_COMM_WORLD);
        }
    }
}