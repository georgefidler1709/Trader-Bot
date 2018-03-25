/*
By George Fidler
3/6/17

This file contains miscellaneous sub-functions and functions smaller in scope used in the bot. 
*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include "trader_bot.h"
#include "trader_header.h"

//Checks the bot's cargo for a location's commodity. If it is present, its position in cargo is returned. Otherwise return -1.
//This function is used to determine whether a buyer is worth evaluating as the bot carries a commodity they would buy.
int cargo_search(struct cargo *cargo, struct commodity *location_commodity) {
    int same_commodity = FALSE;
    int counter;

    if (cargo == NULL) {              //If no cargo, they cannot share a commodity
        return -1;
    }

    for (counter = 0; same_commodity != TRUE; counter++) {
        if (strcmp(cargo->commodity->name, location_commodity->name) == 0) {  //If the bot and the location share a commodity type, return that commodity's spot in cargo
            same_commodity = TRUE;
            break;
        }
        if (cargo->next == NULL) {
            return -1;
        }
        cargo = cargo->next;
    }
    return counter;
}


//Counts the number of locations total in a given world.
//The result of this function "map_size" is used to determine to determine if it is faster to move backwards or forwardsto get to a location.
int size_of_map(struct bot *b) {
    struct location *initial = b->location;
    struct location *current = b->location;
    int map_size = 0;

    do {                //scans through each location, incrementing "map_size" by one each time
        current = current->next;
        map_size++;
    } while (current != initial);

    return map_size;
}


//Determines the best buyer for the current cargo that can be reached with current fuel in the tank, not accounting for fuel cost.
//This function is intended use during the final turns of game wherein the bot hopes to sell its cargo as quickly as posslbe for maximum profit and no longer has to care about refuelling.
int distance_to_final_sales(struct bot *b, struct location *location) {
    int map_size = size_of_map(b);
    int absolute_distance_to_buyer = 0;
    int actual_distance_to_buyer = 0;
    struct location *buyer = location;
    struct cargo *current = b->cargo;
    int reverse_direction = FALSE;
    int shared_commodity;
    int number_sold = 0;
    int buyer_value = 0;
    int best_buyer_value = 0;
    int best_buyer_distance = 0;

    while (buyer != NULL) {           //cycles through all buyers on the map
        buyer = get_location_of_type(b, location, buyer, &absolute_distance_to_buyer, LOCATION_BUYER);
        if (buyer == NULL) {
            continue;
        }
        shared_commodity = cargo_search(current, buyer->commodity); //if a buyer does not want any of the commodities currently in cargo, move on.
        if (shared_commodity == -1) {
            buyer = buyer->next;
            absolute_distance_to_buyer += 1;
            continue;
        } else {
            for (int counter = 0; counter != shared_commodity; counter++) {
                current = current->next;
            }
        }

        if (absolute_distance_to_buyer > map_size / 2) {      //if the distance to the byer is greater than half the map, it is quicker to get there from the other direction.
            actual_distance_to_buyer = map_size - absolute_distance_to_buyer;
            reverse_direction = TRUE;
        } else {
            actual_distance_to_buyer = absolute_distance_to_buyer;
        }

        if (actual_distance_to_buyer > b->fuel) {         //Since this function is designed for the final turns of a game, fuel will not be bought and thus if a buyer is further away than there is fuel in the tank, it is worthless
            buyer = buyer->next;
            absolute_distance_to_buyer += 1;
            continue;
        }

        if (current->quantity < buyer->quantity) {          //determines transaction quantity as the greatest number both bot and buyer are able to trade.
            number_sold = current->quantity;
        } else {
            number_sold = buyer->quantity;
        }

        buyer_value = buyer->price * number_sold;         //Value no longer account for fuel price as fuel will never be bouth again.

        if (buyer_value > best_buyer_value) {           //stores the most profitable buyer tested so far as well as the distance to the buyer so that it can be returned after all buyers have been tested.
            best_buyer_value = buyer_value;
            if (reverse_direction == FALSE) {
                best_buyer_distance = actual_distance_to_buyer;
            } else {
                best_buyer_distance = -actual_distance_to_buyer;
            }
        }

        buyer = buyer->next;
        absolute_distance_to_buyer += 1;
        reverse_direction = FALSE;
    }

    return best_buyer_distance;
}


//Cycles through the bot's cargo and deduts the weight and volume of quantity it is carrying from "weight_remaining" and "volume_remaining" whose initial value is set to
//maximum_cargo_weight and maximum_cargo_volume respectively.
//This function is called when evaluating a seller in determining how much of the seller's quantity the bot could carry in total.
void cargo_capacity_check(struct bot *b, struct cargo *cargo, int *weight_remaining, int *volume_remaining) {
    struct cargo *current = cargo;

    while (current != NULL) {                                     //if there is currently cargo
        if (current->quantity > 0) { 
          *weight_remaining = *weight_remaining - (current->quantity * current->commodity->weight);   //deduct the weight and volume of each individual unit from the maximum values
          *volume_remaining = *volume_remaining - (current->quantity * current->commodity->volume);
        }
        current = current->next;                                    //do this for all commodities carried.
    }
}


//Finds the total of buyer's quantities for all commodities currently in cargo.
//This function is used in "evaluate_dump" to determine if the current cargo could feasibly find buyers.
int buyer_total_for_cargo(struct bot *b) {
    struct location *initial = b->location;
    struct location *search = b->location->next;
    struct cargo *cargo;
    int shared_commodity;
    int buyer_quantity_total = 0;

    while (search != initial) {                           //Using a similar approach to "get_location_of_type" this scans through the world searching for buyers until a full loop has been completed
        cargo = b->cargo;
        if (search->type == LOCATION_BUYER) {
            shared_commodity = cargo_search(b->cargo, search->commodity);     //uses "cargo_search" to determine if the buyer's commodity of choice i one we have in cargo
            if (shared_commodity != -1) {
                for (int counter = 0; shared_commodity != counter; counter++) {   //if so, add the buyer's quantity to "buyer_quantity_total"
                    cargo = cargo->next;
                }
                buyer_quantity_total += search->quantity;
            }
        }
        search = search->next;
    }
    return buyer_quantity_total;
}


//Returns the number of bots at a given location on a given turn.
//This function was used to ensure my bot never got stuck in multi-bot by having more buyers attempting to buy at a location (buyer or petrol station) than there was quantity available,
//resulting in all bots receiving 0 quantity and inevitably trying the same thing next turn. 
int bots_on_location(struct location *location) {
    struct bot_list *current_bot = location->bots;
    int bot_counter;

    for (bot_counter = 0; current_bot != NULL; bot_counter++) {   //adds one to bot_counter for each bot at the given location
        current_bot = current_bot->next;
    }

    return bot_counter;
}


//Cycles through all locations on the map, starting from a given location, until a location of the given type is found. A pointer to this location is then returned.
//This function was made to be called repeatedly within another function, returning all locations of a given type once so they could be evaluated inside that other function. 
struct location *get_location_of_type(struct bot *b, struct location *initial, struct location *curr, 
    int *distance_to_curr, int location_type) {

    struct location *start = initial;
    struct location *search = curr;

    while (search->type != location_type) {
        if (search == start && *distance_to_curr != 0) {                         //Ensures that the initial location is only returned for evaluation if it is of the correct type
            break;
        } 
        if (location_type == (LOCATION_BUYER && start->type == LOCATION_BUYER) || 
            (location_type == LOCATION_PETROL_STATION && start->type == LOCATION_PETROL_STATION)) {
            break;
        }
        *distance_to_curr += 1;
        search = search->next;
    }

    if (search == initial) {
        return NULL;             //A NULL return value tells the evaluation function   that all locations on the map of the correct type have been evaluated.                                            
    } else {
        return search;              //Once a location of the given type is found, a pointer to its location is returned
    }
}