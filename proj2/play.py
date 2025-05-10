from common import globals
from setup.utils import *
from setup.players import *
from coordinator import tournamentmanager
import sys
import logging
import asyncio
import signal

if __name__ == "__main__":

    
    unsupported_args = [arg for arg in sys.argv[1:] if arg not in globals.VALID_ARGS]
    if unsupported_args:
        print(f"Error: Unsupported argument(s): {', '.join(unsupported_args)}")
        print(globals.USAGE)
        sys.exit(1)

    if any(arg in sys.argv for arg in globals.CLEANING_ARGS):
        if all(arg in globals.CLEANING_ARGS for arg in sys.argv[1:]):
            if 'CLEAN_ALL' in sys.argv or 'CLEAN_LOGS' in sys.argv:
                clean_logs()
            
            if 'CLEAN_ALL' in sys.argv or 'CLEAN_PLAYERS' in sys.argv:
                clean_players_directory()
            
            sys.exit(0) 

    if not any(game in sys.argv for game in globals.AVAILABLE_GAMES):
        print("Error: No game specified")
        print(globals.USAGE)
        sys.exit(1)
    
    # Set the logging level
    if 'DEBUG' in sys.argv:
        globals.logger.setLevel(logging.DEBUG)
        globals.console_handler.setLevel(logging.DEBUG)
    else:
        globals.logger.setLevel(logging.INFO)

    # Set the base directory
    set_base_dir(__file__)

    # Validate Environment
    test_env()
 
    move_log_files()

    if 'CLEAN_ALL' in sys.argv or 'CLEAN_LOGS' in sys.argv:
        clean_logs()
    
    if 'CLEAN_ALL' in sys.argv or 'CLEAN_PLAYERS' in sys.argv:
        clean_players_directory()

    # Set Config
    load_config()

    globals.logger.info(f"{globals.NUM_PLAYERS} Players found and loaded")

    # PICKUP was used as a means to host tournaments.
    # This will be replaced with "TOURNAMENT" flag eventually.

    # if 'PICKUP' in sys.argv:
    #     compile_and_move_players()
    #     globals.logger.debug("NOW IN PICKUP MODE")
    #     set_results_csv()
    #     find_incomplete_matches()
    #
    #     if len(globals.round_robin_pairs) == 0:
    #         globals.logger.error("Nothing to pick up from.")
    #         exit(1)

    # Fetch All Players From Directory
    if not load_players():
        globals.logger.error("Exiting...")
        sys.exit()

    init_results_csv()
    
    gen_round_robin_pairs()

    compile_and_move_players()

    # Start Up Server
    try:
        loop = asyncio.new_event_loop()
        asyncio.set_event_loop(loop)
        loop.run_until_complete(tournamentmanager.play())
    except KeyboardInterrupt:
        print("Keyboard interrupt received, shutting down...")

    move_log_files()

