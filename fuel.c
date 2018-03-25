/*
By George Fidler
3/6/17

This file contains all functions relating to fuel-based decisions.
*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include "trader_bot.h"
#include "trader_header.h"

 
//Determines which petrol station is most cost effective for the bot to go when situated at a given location based on the price of fuel, distance to get there and amount of fuel avaliable comparative to fuel_tank_capacity.
//This function is used to find the best petrol station as a sub-function "best_fuel_distance" and "best_fuel_cost" which are called on numerous occasions within the program. 
struct location *evaluate_best_petrol_station(struct bot *b, struct location *location, int distance_to_location, 
int *best_petrol_distance, int for_distance) {

    int map_size = size_of_map(b);
    int absolute_distance_to_petrol = 0;
    int actual_distance_to_petrol = 0;
    int petrol_station_cost = 0;
    int best_station_cost = 0;
    struct location *petrol_station = location;
    struct location *best_petrol_station = location;
    int reverse_direction = FALSE;
    int quantity_of_transaction = 0;

    while (petrol_station != NULL) {                    //cycles through each petrol station on the map once.
        petrol_station = get_location_of_type(b, location, petrol_station, 
            &absolute_distance_to_petrol, LOCATION_PETROL_STATION);

        if (petrol_station == NULL) {                   //if the initial location has been reached, end the cycle.
            continue;
        }

        if (absolute_distance_to_petrol > map_size / 2) {         //if the distance to a location is greater than half the map size away, it is quicker to go around the world the other way.
            actual_distance_to_petrol = map_size - absolute_distance_to_petrol;
            reverse_direction = TRUE;
        } else {
            actual_distance_to_petrol = absolute_distance_to_petrol;
        }

        if (actual_distance_to_petrol >= petrol_station->quantity) {    //if it takes the same amount or more fuel to get to a petrol station than can be bought there, its pointless.
            absolute_distance_to_petrol += 1;
            petrol_station = petrol_station->next;
            continue;
        }

        quantity_of_transaction = petrol_station->quantity;

    
        if (quantity_of_transaction >= b->fuel_tank_capacity) {    //petrol station cost is determined with regard to whether the station's quantity is able to refill the tank to capacity. If it only able to fill it half then the station's cost is doubled. This serves to highlight stations which can fill the bot to full and when all stations have quantity less than "fuel_tank_capacity" the bot will value fuel more.
            petrol_station_cost = petrol_station->price * (actual_distance_to_petrol + distance_to_location);
        } else if (quantity_of_transaction > 0) {
            petrol_station_cost = petrol_station->price * (actual_distance_to_petrol + distance_to_location) *
                (b->fuel_tank_capacity / (petrol_station->quantity + 1)); 
        } else {
            petrol_station_cost = -1;
        }

        if (actual_distance_to_petrol + distance_to_location > b->fuel && for_distance == TRUE) { //if the petrol station cannot be reached in one fueltank it should not be chosen as the next destination. However it can be chosen as the destination after another action.
            petrol_station_cost = -1;
        }

        if (petrol_station_cost != -1) {                          //The best value (lowest cost) of all stations tested is stored as well as the distance to that station.
            if (petrol_station_cost < best_station_cost || best_station_cost == 0) {
                best_station_cost = petrol_station_cost;
                best_petrol_station = petrol_station;
                if (reverse_direction == TRUE) { 
                    *best_petrol_distance = -actual_distance_to_petrol;
                } else { 
                    *best_petrol_distance = actual_distance_to_petrol;
                }
            }
        }
        petrol_station = petrol_station->next;
        absolute_distance_to_petrol += 1;
        reverse_direction = FALSE;
    }

    return best_petrol_station;
}

//Uses the data found in "evaluate_best_petrol_station" to output the distance to the best petrol station for a given location.
int best_petrol_distance(struct bot *b, struct location *location, int distance_to_location) {
    int distance = 0;
    struct location *petrol_station = evaluate_best_petrol_station(b, location, distance_to_location, &distance, TRUE);

    return distance;
}


//Uses the data found in "evaluate_best_petrol_station" to output the cost of the best value petrol station at a given location.
//I initially intended to do this in the same manner as "best_petrol_distance" (i.e. a pointer) but a few extra conditions in bug testing led me to this approach.
int best_petrol_cost(struct bot *b, struct location *location, int distance_to_location) {
    int distance_to_petrol = 0;
    int petrol_station_cost;
    struct location *petrol_station = evaluate_best_petrol_station(b, location, distance_to_location, &distance_to_petrol, FALSE);

    if(distance_to_petrol < 0) {                    //takes the absolute value
        distance_to_petrol = -distance_to_petrol;
    }
    if(distance_to_petrol + distance_to_location == 0) {  //If the bot is currently at the petrol station, its cost is just its price.
        petrol_station_cost = petrol_station->price;
    } else if(petrol_station->quantity >= b->fuel_tank_capacity) {  //else cost is cost is determined in the same manner as "evaluate_best_petrol_station"
            petrol_station_cost = petrol_station->price * (distance_to_petrol + distance_to_location);
        } else {
            petrol_station_cost = petrol_station->price * (distance_to_petrol + distance_to_location) * (b->fuel_tank_capacity / (petrol_station->quantity + 1));         //note: may be a better approach involving marginal cost of fuel i.e. (price * distance) / quantity available (less than fuel_tank_capacity)
        }

    return petrol_station_cost;
}
