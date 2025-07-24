#include "types.h"
#include "riscv.h"
#include "spinlock.h"
#include "defs.h"
#include "param.h"
#include "proc.h"

#define NUM_PLAYERS 4
#define CARDS_PER_PLAYER 5
#define DECK_SIZE 52
#define NUM_ROUNDS 5
#define STARTING_CHIPS 100

struct spinlock gamelock;
const char *suits[4] = {"♥", "♦", "♣", "♠"};
const char *ranks[13] = {"A","2","3","4","5","6","7","8","9","10","J","Q","K"};

struct Card {
    int suit;
    int rank;
    int special; // 0=normal, 1=double points, 2=swap hands, 3=see future
};

struct Player {
    struct Card hand[CARDS_PER_PLAYER];
    int hand_size;
    int chips;
    int folded;
};

struct Card deck[DECK_SIZE];
struct Player players[NUM_PLAYERS];
int pot = 0;
int current_bet = 0;
unsigned int seed = 123456;

// Enhanced random number generator
unsigned int simple_rand() {
    seed = (1103515245 * seed + 12345) % (1 << 31);
    return seed;
}

void init_game() {
    initlock(&gamelock, "game");
    for(int i = 0; i < DECK_SIZE; i++) {
        deck[i].suit = i / 13;
        deck[i].rank = i % 13;
        deck[i].special = (i % 13 == 0) ? 1 :  // Aces double points
                         (i % 13 == 12) ? 2 : // Kings swap hands
                         (i % 13 == 11) ? 3 : 0; // Queens see future
    }
    for(int i = 0; i < NUM_PLAYERS; i++) {
        players[i].hand_size = 0;
        players[i].chips = STARTING_CHIPS;
        players[i].folded = 0;
    }
    pot = 0;
    current_bet = 0;
}

void shuffle_deck() {
    acquire(&gamelock);
    for(int i = DECK_SIZE-1; i > 0; i--) {
        int j = simple_rand() % (i+1);
        struct Card temp = deck[i];
        deck[i] = deck[j];
        deck[j] = temp;
    }
    release(&gamelock);
}

void deal_cards() {
    acquire(&gamelock);
    int card_index = 0;
    for(int round = 0; round < CARDS_PER_PLAYER; round++) {
        for(int player = 0; player < NUM_PLAYERS; player++) {
            if(card_index < DECK_SIZE) {
                players[player].hand[players[player].hand_size++] = deck[card_index++];
            }
        }
    }
    release(&gamelock);
}

void print_card(struct Card c) {
    printf("%s%s", ranks[c.rank], suits[c.suit]);
    if(c.special > 0) {
        printf("(%s)", 
              c.special == 1 ? "2x" :
              c.special == 2 ? "SW" : "SF");
    }
}

void print_hand(int player) {
    printf("Player %d hand: ", player);
    for(int i = 0; i < players[player].hand_size; i++) {
        print_card(players[player].hand[i]);
        printf(" ");
    }
    printf("| Chips: %d\n", players[player].chips);
}

int get_trump_suit() {
    return simple_rand() % 4;
}

void activate_special(int player, struct Card card) {
    switch(card.special) {
        case 1: // Double points
            printf("Player %d activates DOUBLE POINTS for next round!\n", player);
            break;
        case 2: // Swap hands
            printf("Player %d activates SWAP HANDS!\n", player);
            if(player < NUM_PLAYERS-1) {
                struct Card temp[CARDS_PER_PLAYER];
                int temp_size = players[player].hand_size;
                for(int i = 0; i < players[player].hand_size; i++) {
                    temp[i] = players[player].hand[i];
                }
                players[player].hand_size = players[player+1].hand_size;
                for(int i = 0; i < players[player+1].hand_size; i++) {
                    players[player].hand[i] = players[player+1].hand[i];
                }
                players[player+1].hand_size = temp_size;
                for(int i = 0; i < temp_size; i++) {
                    players[player+1].hand[i] = temp[i];
                }
            }
            break;
        case 3: // See future
            printf("Player %d peeks at next trump suit: %s\n", 
                 player, suits[get_trump_suit()]);
            break;
    }
}

void betting_round(int dealer) {
    printf("\n--- Betting Round ---\n");
    current_bet = 10; // Small blind
    
    for(int i = 0; i < NUM_PLAYERS; i++) {
        int player = (dealer + i) % NUM_PLAYERS;
        if(players[player].folded) continue;
        
        // Simple AI betting logic
        int decision = simple_rand() % 3;
        if(players[player].chips <= current_bet) {
            decision = 0; // Fold if can't meet bet
        }
        
        switch(decision) {
            case 0: // Fold
                printf("Player %d folds!\n", player);
                players[player].folded = 1;
                break;
            case 1: // Call
                printf("Player %d calls %d\n", player, current_bet);
                players[player].chips -= current_bet;
                pot += current_bet;
                break;
            case 2: // Raise
                current_bet += 10;
                printf("Player %d raises to %d\n", player, current_bet);
                players[player].chips -= current_bet;
                pot += current_bet;
                break;
        }
    }
}

int play_round(int dealer, int round_num) {
    struct Card played[NUM_PLAYERS];
    int winning_player = dealer;
    int winning_value = -1;
    int trump_suit = get_trump_suit();
    
    betting_round(dealer);
    
    printf("\n--- Round %d ---\n", round_num + 1);
    printf("Trump suit: %s | Pot: %d\n", suits[trump_suit], pot);
    
    for(int i = 0; i < NUM_PLAYERS; i++) {
        int current = (dealer + i) % NUM_PLAYERS;
        if(players[current].folded) continue;
        
        // Play first card (simple AI)
        if(players[current].hand_size > 0) {
            played[current] = players[current].hand[0];
            
            // Activate special ability if present
            if(played[current].special > 0) {
                activate_special(current, played[current]);
            }
            
            // Remove from hand
            for(int j = 0; j < players[current].hand_size-1; j++) {
                players[current].hand[j] = players[current].hand[j+1];
            }
            players[current].hand_size--;
            
            printf("Player %d plays: ", current);
            print_card(played[current]);
            printf("\n");
            
            // Calculate card value
            int card_value = played[current].rank;
            if(played[current].suit == trump_suit) {
                card_value += 20; // Trump bonus
            } else if(played[current].suit == played[dealer].suit) {
                card_value += 10; // Follow suit bonus
            }
            
            if(played[current].special == 1) {
                card_value *= 2; // Double points
            }
            
            if(card_value > winning_value) {
                winning_value = card_value;
                winning_player = current;
            }
        }
    }
    
    printf("Player %d wins the round and %d chips!\n", winning_player, pot);
    players[winning_player].chips += pot;
    pot = 0;
    current_bet = 0;
    
    // Reset folded status for next round
    for(int i = 0; i < NUM_PLAYERS; i++) {
        players[i].folded = 0;
    }
    
    return winning_player;
}

uint64 sys_philosopher(void) {
    init_game();
    shuffle_deck();
    deal_cards();

    printf("\n=== HIGH STAKES CARD GAME ===\n");
    printf("%d players, %d cards each, %d rounds\n", 
          NUM_PLAYERS, CARDS_PER_PLAYER, NUM_ROUNDS);
    printf("Special cards: 2x=Double, SW=Swap, SF=See Future\n\n");
    
    // Show initial hands
    for(int i = 0; i < NUM_PLAYERS; i++) {
        print_hand(i);
    }
    
    int dealer = simple_rand() % NUM_PLAYERS;
    printf("\nPlayer %d is dealer first\n", dealer);
    
    for(int round = 0; round < NUM_ROUNDS; round++) {
        dealer = play_round(dealer, round);
        // Small delay between rounds
        for(int i = 0; i < 1000000; i++) {}
        
        // Check if any player has no chips left
        int active_players = 0;
        for(int i = 0; i < NUM_PLAYERS; i++) {
            if(players[i].chips > 0) active_players++;
        }
        if(active_players < 2) break;
    }
    
    // Final results
    printf("\n=== GAME OVER ===\n");
    int winner = 0;
    for(int i = 0; i < NUM_PLAYERS; i++) {
        printf("Player %d chips: %d\n", i, players[i].chips);
        if(players[i].chips > players[winner].chips) {
            winner = i;
        }
    }
    printf("\nPlayer %d wins the tournament!\n", winner);
    
    return 0;
}