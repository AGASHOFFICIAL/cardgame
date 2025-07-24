This project implements a turn-based multiplayer card game as a custom system call within the xv6 operating system. It simulates 4 virtual players, each receiving cards and taking turns across multiple rounds with game mechanics such as betting, trump suits, and special card powers.

Key functionalities include:

    A 52-card deck with suits and ranks, including special cards like:

        Aces → Double Points

        Kings → Swap Hands

        Queens → See Future Trump

    Game logic includes:

        Deck initialization and shuffling

        Card dealing to players

        Betting round with raise/fold/call decisions

        Round-based gameplay with a trump suit

        Winner determination based on card value and special effects

    The game uses spinlocks for synchronizing access to shared resources like the deck and pot.

    A random number generator simulates card shuffling and player decisions.

Players are simulated internally using structures, and the entire game runs through a custom system call (sys_philosopher) from the kernel space.
