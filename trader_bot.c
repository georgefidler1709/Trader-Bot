/*
By George Fidler
3/6/17
A bot that attempts to maximise profit in a 'Trader-bot World' , both single-play and multi-player. 

The bot appraoches the task by giving each loation with a value prospect (buyer, seller and dump) an evaluation score. The highest of these scores after a complete lap of the world is chosen as the 'best value' location on the map.
However if the bot cannot reach this 'best value' location and then reach the most cost-effective petrol station, the bot will go refuel s station at this instead.
The location evaluations are based on revenue for buyers, profit for sellers and revenue lost for dumps. Each has its value deducted by the price of petrol necessary to get there from the current location.
*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include "trader_bot.h"
#include "trader_header.h"

//names the bot
char *get_bot_name(void) {      
    return "The Travelling Salesman";
}


//This function serves as the pseduo-main function of this process i.e. it is within this function all information from other functions
//is processed and the final decision of what *n and *action should equal on this turn is made.

void get_action(struct bot *b, int *action, int *n) {
    struct location *start = b->location;
    int best_value = 0, best_value_quantity = 0, distance_to_best_value = 0;
    int cannot_afford_petrol = FALSE;

    scan_world(b, start, &best_value, &distance_to_best_value, &best_value_quantity, cannot_afford_petrol); //The most valuable location in terms of profit (or future profit for a sellers commodity) is determined in "scan_world"

    if (start->type == LOCATION_PETROL_STATION && b->fuel != b->fuel_tank_capacity && start->quantity >= bots_on_location(start) 
        && best_petrol_cost(b, start, 0) != 0 && b->cash > start->price) {            //If the starting location is a petrol station and the bot both needs and can afford fuel, fuel up this turn 
        *action = ACTION_BUY; 
        *n = b->fuel_tank_capacity;

    } else if (fuelcheck(b, distance_to_best_value) == 1 && b->turns_left >= MIN_TURNS_TO_ACTION_THEN_MAKE_PROFIT) {             //If "fuel_check" returns 1 the bot cannot reach the best value location and then reach a petrol station thereafter, thus (as long as there are enough turns left in the game for refuelling to be valuable), find the best fuel station and move there to refuel.
        *n = best_petrol_distance(b, b->location, 0); 
        *action = ACTION_MOVE;

        if (*n == 0 || best_petrol_cost(b, b->location, 0) > b->cash) {             //If no petrol station of value is found, we rescan the world disregarding fuel cost to find the closest possible buyer of a commodity in cargo such that the bot can generate enough money to afford fuel and continue its game.
            best_value = 0;
            best_value_quantity = 0;
            distance_to_best_value = 0;
            cannot_afford_petrol = TRUE;
            scan_world(b, start, &best_value, &distance_to_best_value, &best_value_quantity, cannot_afford_petrol); //"cannot_afford_petrol" becoming true is the trigger used within "scan_world" to fulfil the process outlined in the above comment.
            *n = distance_to_best_value; 
            *action = ACTION_MOVE;
        }

    } else {                  //If there is enough fuel to move to the location of best value, do so.
        *action = ACTION_MOVE; 
        *n = distance_to_best_value;
    }

    if (distance_to_best_value == 0) {     //If the best value is at the current location it is time to do something other than move
        if (start->type == LOCATION_BUYER) { //For a buyer, buy as much as possible.
            *action = ACTION_SELL; 
            *n = start->quantity;
        }

        if (start->type == LOCATION_SELLER && cannot_afford_petrol == FALSE) { //For a seller, buy only as much as the best buyer found in evaluating the seller is willing to buy. This in hopes of minimizing the chance of being left without a buyer for the commodity.
            *action = ACTION_BUY; 
            *n = best_value_quantity;
        }

        if (start->type == LOCATION_DUMP) { //Dump is given a value within "evaluate_dump" only in the case that we are left with a commodity without a buyer and must clear cargo space.
            *action = ACTION_DUMP;
        }

    }

    if (best_value <= 0) {  
/*For all locations to have a value <= 0: 
a)There is no fuel left on the map 
b) There are no sellers within range such that the profit made from them is greater than the fuel cost involved in the transaction 
c) Fuel has become relatively expensive in my algorthms for evaluating locations because fuel stops within range carry only a fraction of "fuel_capacity: 
*/
        int petrol_distance = best_petrol_distance(b, start, 0);    //finds the best petrol station available
        struct location *search = b->location;
        int counter;

        if (petrol_distance > 0) {
            for (counter = 0; counter != petrol_distance; counter++) {
                search = search->next;
            }
        } else {
            for (counter = 0; counter != petrol_distance; counter--) {
                search = search->previous;
            }
        }

        if (search->type == LOCATION_PETROL_STATION && search->quantity > b->fuel_tank_capacity / 4 + petrol_distance &&  //does it still count as a magic number if i literally mean a quarter? Seems kind of crazy to put anything else
            (b->turns_left >= MIN_TURNS_TO_ACTION_THEN_MAKE_PROFIT || (b->fuel < MIN_TURNS_TO_BUY_AND_SELL *b->maximum_move && b->turns_left >= MIN_TURNS_TO_ACTION_THEN_MAKE_PROFIT))) { 
        //if the petrol station would be able to fill up the bot's tank by over a quarter (including the distance it took to travel to the petrol station) and there is enough time to make use of this fuel, go fuel up. This accounts for the condition outlined in c) as fuel become precious.
            *n = petrol_distance;
            *action = ACTION_MOVE;
        } else {                                          //Otherwise disregard fuel cost and just sell whatever is in cargo to the highest margin buyer that can be reached with the fuel left in the tank.
            int distance_to_buyer_disregarding_fuel = distance_to_final_sales(b, start);
            if (distance_to_buyer_disregarding_fuel != 0) { 
                *action = ACTION_MOVE; 
                *n = distance_to_buyer_disregarding_fuel;
            } else {
                *action = ACTION_SELL; 
                *n = start->quantity;
            }
        }
    }
    if (*action != ACTION_DUMP && *n == 0) {   //failsafe: the bot should never succeed in this condition but if so the hope is that by moving away from the current location the bug will not arise again at the new location.
        *action = ACTION_MOVE; 
        *n = b->maximum_move;
    }
}


//Cycles through every location on the map and gives values to all buyers, sellers and dumps. The best value location and appropriate extra information (e.g. distance to the location from the bots current position) is then passed. 
//This function is the crux of my trader_bot system. Each algorithm called within is tuned to each location type in hopes of giving a fair evaluation as an integer value which is comparable to the values returned for the 2 other location types assessed.
//It is noted that "evaluate_buyer" is given an edge over the others as it does not account for the margin, simply the immediate revenue. This faster cycle of buying and selling seemed to result in the most profit in practice.
void scan_world(struct bot *b, struct location *start, int *best_value, int *distance_to_best_value, 
    int *best_value_quantity, int cannot_afford_petrol) {

    struct location *forwards = start;
    struct location *backwards = start;
    int distance = 0;
    int value = 0;
    int transaction_quantity = 0;

    while (distance < 2 || (backwards != forwards->previous && backwards != forwards->previous->previous)) {    //cycles through the entire map until the initial location is reached
        if (forwards->type == LOCATION_BUYER) {                                   //Only buyers are evaluated in the last 3 turns of the game as it requres a minimum of 4 turns to complete a seller-buyer transaction. 
            value = evaluate_buyer(b, forwards, distance, cannot_afford_petrol);
            if (value > *best_value) {                                        //if the value of the current location is greater than the current "best_value" store the information of this location as the current best location.
                *best_value = value; 
                *distance_to_best_value = distance;
            }

        } else if (forwards->type == LOCATION_SELLER && b->turns_left >= MIN_TURNS_TO_BUY_AND_SELL) {                   
            value = evaluate_seller(b, forwards, distance, &transaction_quantity);
            if (value > *best_value) { 
                *best_value = value; 
                *best_value_quantity = transaction_quantity; 
                *distance_to_best_value = distance;
            }

        } else if (forwards->type == LOCATION_DUMP && b->turns_left >= MIN_TURNS_TO_ACTION_THEN_MAKE_PROFIT) {
            value = evaluate_dump(b, forwards, distance, cannot_afford_petrol);
            if (value > *best_value) { 
                *best_value = value; 
                *best_value_quantity = transaction_quantity;
                *distance_to_best_value = distance;
            }
        }

        if (backwards->type == LOCATION_BUYER) {
            value = evaluate_buyer(b, backwards, distance, cannot_afford_petrol);
            if (value > *best_value) { 
                *best_value = value; 
                *distance_to_best_value = -distance;
            }

        } else if (backwards->type == LOCATION_SELLER && b->turns_left >= MIN_TURNS_TO_BUY_AND_SELL) {
            value = evaluate_seller(b, backwards, distance, &transaction_quantity);
            if (value > *best_value) { 
                *best_value = value; 
                *best_value_quantity = transaction_quantity; 
                *distance_to_best_value = -distance;
            }
        } else if (backwards->type == LOCATION_DUMP && b->turns_left >= MIN_TURNS_TO_ACTION_THEN_MAKE_PROFIT) {
            value = evaluate_dump(b, backwards, distance, cannot_afford_petrol);
            if (value > *best_value) { 
                *best_value = value; 
                *best_value_quantity = transaction_quantity; 
                *distance_to_best_value = distance;
            }
        }

        forwards = forwards->next;
        backwards = backwards->previous;
        distance++;
    }
}


//Determines wether the distance to the ost valuable location (chosen in "scan_world") + the distance to the best petrol station (chosen in "evaluate_best_petrol_station") is greater than the petrol in the tank. If so, return 1.
//This function is used in "get_action" after the best value location is found to check that after completing this action the bot would be able to rach a petrol station to fuel up.
int fuelcheck(struct bot *b, int distance_to_best_value) {
    int distance_counter;
    struct location *current = b->location;

    if (distance_to_best_value > 0) {                                 //"distance_to_best_value" is used to find the location deemed to be most valuable
        for (distance_counter = 0; distance_counter < distance_to_best_value; distance_counter++) {
            current = current->next;
        }
    } else {
        for (distance_counter = 0; distance_counter < -distance_to_best_value; distance_counter++) {  
            current = current->previous;
        }
    }

    int distance_to_petrol = best_petrol_distance(b, current, distance_to_best_value);    //The distance to the best petrol station from that location is then found. 

    if (distance_to_petrol < 0) {                 //This distance is onverted to its absolute value for comparison to fuel in tank.
        distance_to_petrol = -distance_to_petrol;
    } else if (distance_to_petrol == 0) {           
        return 1;
    }

    int total_distance = distance_counter + distance_to_petrol;
    if (total_distance > b->fuel) {                 //If the distance of the trip in total (distance to best value + distance to the petrol station) > fuel in the tank, return 1.
        return 1;
    } else {
        return 0;
    }
}

