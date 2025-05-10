Othello = {
    "boardSize": 8,                     # do not change.
    "turnLength": 5000,
    "numPlayers": 2,                    # do not change.
    "numMatches": 2,                    # do not change.
    "threads": 4,
    "player1Path": "",
    "player2Path": ""
}

Gomuku = {
        "mnk_height": 15,               # height and width must be the same.
        "mnk_width": 15,
        "mnk_k": 5,                     # number of pieces in a row.
        "turnLength": 4000,
        "perfectInformation": True,     # do not change.
        "numPlayers": 2,                # do not change.
        "numMatches": 2,
        "threads": 4,
        "player1Path": "",              # these two player paths will be overwritten by the runner
        "player2Path": ""
}

Nim = {
        "boardSize": 5,
        "turnLength": 3000,
        "numPlayers": 2,
        "numMatches": 2,
        "threads": 4,
        "player1Path": "players/my_player",
        "player2Path": "players/random_player"
}

