/*
By George Fidler
3/6/17

This file contains all evaluating functions called in "scan world"
*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include "trader_bot.h"
#include "trader_header.h"

//Determines the value of a buyer location based on the profit made in travelling to the location and trading a commodity in cargo.
int evaluate_buyer(struct bot *b, struct location *buyer, int distance_from_current, int cannot_afford_petrol) {
    struct cargo *current = b->cargo;

    if(bots_on_location(buyer) >= buyer->quantity && distance_from_current == 0) {  //Ensures that the bot does not fail to sell to the buyer due to too many players trying to do the same all at once. 
        return 0;
    }

    if (cannot_afford_petrol == TRUE) {                       //Only applies when the bot wants to refuel to reach the actual best value location but cannot. A buyer is thus disqualified if the distance to reach it + the distance to petrol is greater than fuel in the tank.
        int check = best_petrol_distance(b, buyer, distance_from_current);
        if (distance_from_current + abs(check) > b->fuel || check == 0) {
            return 0;
        }
    }

    int shared_commodity = cargo_search(current, buyer->commodity);       //buyer is given a value of 0 if the bot has nothing to sell it.
    if (shared_commodity < 0) {
        return 0;
    } else {
        for (int counter = 0; counter < shared_commodity; counter++) {
            current = current->next;
        }
    }

    int number_sold;
    if (current->quantity < buyer->quantity) {                //actual quantity of the transaction is the smallest of the quantities each party wants to trade.
        number_sold = current->quantity;
    } else {
        number_sold = buyer->quantity;
    }

    int travel_cost = best_petrol_cost(b, buyer, distance_from_current);  //Adjusted price of fuel is found for travelling to the buyer
    int buyer_value = (number_sold * buyer->price) - travel_cost;         //values then used in a simple equation to determine total value (money made - money lost due to petrol cost).

    return buyer_value;

}


//Determines the value of a seller as the margin made if the bot was to sell their commodity to the nearest buyer.
int evaluate_seller(struct bot *b, struct location *seller, int distance_from_current, int *transaction_quantity) {
    int max_transportable_weight = b->maximum_cargo_weight;
    int max_transportable_volume = b->maximum_cargo_volume;

    cargo_capacity_check(b, b->cargo, &max_transportable_weight, &max_transportable_volume);  //Total amount of the sller's commodity the bot has space to carry is calculated
    max_transportable_weight = (max_transportable_weight / seller->commodity->weight);
    max_transportable_volume = (max_transportable_volume / seller->commodity->volume);

    if (max_transportable_weight < max_transportable_volume) {
        *transaction_quantity = max_transportable_weight;
    } else {
        *transaction_quantity = max_transportable_volume;
    }

    if (*transaction_quantity * seller->price > b->cash) {                 //If the bot cannot afford this quantity, set the quantity to the maximum it can actually afford.
        *transaction_quantity = b->cash / seller->price;
    }

    if (*transaction_quantity > seller->quantity) {                       //If the seller does not actually carry this quantity, set the quantity to the seller's max
        *transaction_quantity = seller->quantity;
    }

    int seller_value = get_best_value_for_seller(b, seller, distance_from_current, &transaction_quantity);  //the rest of the evaluation is done in finding the most valuable buyer for this seller.

    return seller_value;

}


//Determines the most valuable buyer for a given seller based on the profit margin between the two minus the cost of travel. 
//This profit margin is then returned to be used as the seller's value.
int get_best_value_for_seller(struct bot *b, struct location *seller, int distance_from_current, int **transaction_quantity) {
    int absolute_distance_to_buyer = 0;
    int actual_distance_to_buyer;
    struct location * buyer = seller;
    int map_size = size_of_map(b);
    int quantity_of_transaction = 0;
    int marginal_profit = 0;
    int travel_cost = 0;
    int buyer_value = 0;
    int best_value_for_seller = 0;
    int max_transportable_quantity = **transaction_quantity;
    int right_comm = TRUE;

    while (right_comm == TRUE) { 
        buyer = get_location_of_type(b, seller, buyer, & absolute_distance_to_buyer, LOCATION_BUYER);    //evaluates each buyer on the map once.
        if (buyer == NULL) {
            right_comm = FALSE;   //If the buyer is not of the right commodity, keep searching.
            break;
        } else if (buyer->commodity != seller->commodity) {
            buyer = buyer->next;
            absolute_distance_to_buyer += 1;
            continue;
        }

        if (absolute_distance_to_buyer > map_size / 2) {    //sets "actual_distance_to_buyer" to be the length of the shortest route from seller to buyer.
            actual_distance_to_buyer = map_size - absolute_distance_to_buyer;
        } else {
            actual_distance_to_buyer = absolute_distance_to_buyer;
        }

        marginal_profit = buyer->price - seller->price;   //profit margin found

        if (buyer->quantity < seller->quantity) {
            quantity_of_transaction = buyer->quantity;    //quantity of transaction set at the the highest quantity each are willing to trade/the bot is able to carry.
        } else {
            quantity_of_transaction = seller->quantity;
        }
        if (max_transportable_quantity < quantity_of_transaction) {
            quantity_of_transaction = max_transportable_quantity;
        }

        travel_cost = best_petrol_cost(b, seller, distance_from_current + actual_distance_to_buyer);    //Adjusted price of fuel is found for travelling from the current location to the seller, and then from the seller to the buyer.

        if (distance_from_current + actual_distance_to_buyer + best_petrol_distance(b, buyer, actual_distance_to_buyer) > b->fuel && 
              travel_cost > b->cash - (seller->price * quantity_of_transaction)) {
            buyer = buyer->next;
            absolute_distance_to_buyer += 1;
            continue;
        }

        buyer_value = (quantity_of_transaction * marginal_profit) - travel_cost;    //simple algorithm to determine the profit made from completing the transaction (money made -money lost due to travel) 

        if (buyer_value > best_value_for_seller || best_value_for_seller == 0) {    //if this value is the best value found for this seller so far, save it as the "best_value_for_seller"       
            best_value_for_seller = buyer_value; 
            **transaction_quantity = quantity_of_transaction;   //This value "transaction_quantity" is then used as the value of *n if this location is chosen as the best value, ensuring no more is bought than can be sold to the buyer.
        }

        buyer = buyer->next;
        absolute_distance_to_buyer += 1;

    }

    return best_value_for_seller;   //Once all buyers have been tested, the best value for the given seller is returned.
}


//Determines a value for the dump as the price of the commodities in cargo that can no longer have a buyer willing to take them.
//This function was added purely for multi-bot purposes as the bot should never buy more than necessary but a buyer could be sold to before my bot can reach them.
int evaluate_dump(struct bot *b, struct location *dump, int distance_from_current, int cannot_afford_petrol) {
    if (cannot_afford_petrol == TRUE) {                       //If the bot is in a state of trying to get enough money together to buy fuel to survive, only dump if there is an adequate enough amount of fuel to then go buy, sell and reach the fuel station again.                    
        int check = best_petrol_distance(b, dump, distance_from_current);
        if (distance_from_current + abs(check) > b->fuel + (b->fuel_tank_capacity / 2) || check == 0) {
            return 0;
        }
    }

    int buyers_quantity = buyer_total_for_cargo(b); //finds total of buyer quantities for commodities in cargo
    int map_size = size_of_map(b);
    int bot_quantity_total = 0;
    struct cargo *cargo = b->cargo;

    while (cargo != NULL) {             //determines the total quantities of commodities in cargo
        bot_quantity_total += cargo->quantity;
        cargo = cargo->next;
    }

    if (buyers_quantity > bot_quantity_total) {   //if there is a way to sell the cargo, there is no need to dump
        return 0;
    }

    cargo = b->cargo;
    int most_quantity_commodity = 0;        //Finds the price of the nearest buyer for the commodity with the highest quantity in cargo and uses that buyers price as the price per unit to be used in giving the dump a value in "scan_world".
    struct cargo *most_commodity = cargo;

    while (cargo != NULL) {
        if (cargo->quantity > most_quantity_commodity) {
            most_commodity = cargo;
        }
        cargo = cargo->next;
    }

    struct location *closest_buyer = dump->next;
    while (closest_buyer != dump) {
        if (closest_buyer->type == LOCATION_BUYER) {
            if (cargo_search(b->cargo, closest_buyer->commodity) != -1) {
                break;
            }
        }
        closest_buyer = closest_buyer->next;
    }

    int price = closest_buyer->price;
    int travel_cost = best_petrol_cost(b, dump, distance_from_current);        //Finds adjusted petrol cost of travelling to the dump.

    int quantity_dumped = bot_quantity_total - buyers_quantity;               //Quantity that needs to be dumped is the quantity which cannot be sold to a buyer

    int dump_value = ((quantity_dumped * price) - travel_cost) / 2;             //This is then used in a modified version of the equation used in "evaluate_buyer" and "evaluate_seller" however the value of this one is halved so that dump is only chosen when truly all other options are exhausted

    return dump_value;
}

