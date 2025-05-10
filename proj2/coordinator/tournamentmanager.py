from coordinator import server
from coordinator import lobby
from coordinator import doublereleaselock
from coordinator import doublereleasesemaphore
import uuid
from common import globals
import asyncio
import csv
from collections import defaultdict


async def play():
    # Start server
    server_task = asyncio.create_task(server.get_server_process())
    
    await asyncio.sleep(2)
    
    sem = doublereleasesemaphore.DoubleReleaseSemaphore(globals.CONCURRENT_LOBBIES)
    double_lock = doublereleaselock.DoubleReleaseLock()
    lobby_tasks = []
    
    for player1, player2 in globals.round_robin_pairs:
        lobby_id = str(uuid.uuid4())
        # Need to await lobby start before players can connect.
        await lobby.start_lobby(lobby_id, player1, player2, sem, double_lock)
        task = asyncio.create_task(lobby.connect_players(lobby_id, sem, double_lock))
        lobby_tasks.append(task)
    
    if lobby_tasks:
        await asyncio.gather(*lobby_tasks)
        
    # await server_task

    if not globals.GAME_CRASH:
        update_winners_and_get_stats(globals.RESULTS_CSV_PATH)
    else:
        err_msg = "\n\n" + "=" * 60 + "\n"
        err_msg += " " * 16 + "Crash Detected\n\n"  # Adjusting indentation for the title
        err_msg += "Details: Client crashed the game with an error message below. "
        err_msg += "Refer to the latest .log files in `IngeniousFrame/Logs` for more info.\n\n"
        err_msg += f"{globals.GAME_CRASH_MESSAGE}\n"  # Incorporating the specific error message
        err_msg += "=" * 60 + "\n"
        globals.logger.error(err_msg)

def update_winners_and_get_stats(csv_file):
    with open(csv_file, 'r') as file:
        reader = csv.DictReader(file)
        rows = list(reader)

    win_counts = defaultdict(int)
    game_win_counts = defaultdict(lambda: [0, 0])  # [Game 1 wins, Game 2 wins]
    total_chips = defaultdict(int)

    for row in rows:
        player1 = row['Player 1']
        player2 = row['Player 2']

        player1_wins = 0
        player2_wins = 0

        for game in ['Game 1', 'Game 2']:
            player1_chips = float(row[f'Player 1 Chips {game}']) if row[f'Player 1 Chips {game}'] else 0
            player2_chips = float(row[f'Player 2 Chips {game}']) if row[f'Player 2 Chips {game}'] else 0

            total_chips[player1] += player1_chips
            total_chips[player2] += player2_chips

            if player1_chips > player2_chips:
                player1_wins += 1
                game_win_counts[player1][int(game.split()[-1]) - 1] += 1
            elif player2_chips > player1_chips:
                player2_wins += 1
                game_win_counts[player2][int(game.split()[-1]) - 1] += 1

        if player1_wins > player2_wins:
            row['Winner'] = player1
            win_counts[player1] += 1
        elif player2_wins > player1_wins:
            row['Winner'] = player2
            win_counts[player2] += 1
        else:
            row['Winner'] = 'Tie'

    with open(csv_file, 'w', newline='') as file:
        fieldnames = ['Lobby ID', 'Player 1', 'Player 2', 'Player 1 Chips Game 1', 'Player 2 Chips Game 1',
                      'Player 1 Chips Game 2', 'Player 2 Chips Game 2', 'Winner']
        writer = csv.DictWriter(file, fieldnames=fieldnames)
        writer.writeheader()
        writer.writerows(rows)

    globals.logger.info(("-" * 9) + " Game Statistics " + ("-" * 9))
    globals.logger.info(f"Total matchups made: {len(rows)}")

    if win_counts:
        most_wins_player = max(win_counts, key=win_counts.get)
        most_wins_count = win_counts[most_wins_player]
        globals.logger.info(f"Player with the most overall wins: {most_wins_player} ({most_wins_count} wins)")

        for player in game_win_counts:
            game1_wins = game_win_counts[player][0]
            game2_wins = game_win_counts[player][1]
            globals.logger.info(f"Statistics for {player}:")
            globals.logger.info(f">> Game 1 wins: {game1_wins}")
            globals.logger.info(f">> Game 2 wins: {game2_wins}")
            globals.logger.info(f">> Total points earned: {total_chips[player]}")
    else:
        globals.logger.info("Tournament resulted in a Tie")
    
    globals.logger.info("-" * 35)

