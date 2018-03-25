#define TRUE 1
#define FALSE 0
#define MIN_TURNS_TO_BUY_AND_SELL 3
#define MIN_TURNS_TO_ACTION_THEN_MAKE_PROFIT 6

void get_action(struct bot *b, int *action, int *n);
void scan_world(struct bot *b, struct location *start, int *best_value, int *distance_to_best_value, int *best_value_quantity, int cannot_afford_petrol);
int evaluate_buyer(struct bot *b, struct location *buyer, int distance_from_current, int cannot_afford_petrol);
int evaluate_seller(struct bot *b, struct location *seller, int distance_from_current, int *transaction_quantity);
int get_best_value_for_seller(struct bot *b, struct location *seller, int distance_from_current, int **transaction_quantity);
int evaluate_dump(struct bot *b, struct location *dump, int distance_from_current, int cannot_afford_petrol);
int fuelcheck(struct bot *b, int distance_to_best_value);
int best_petrol_distance(struct bot *b, struct location *location, int distance_to_location);
int best_petrol_cost(struct bot *b, struct location *location, int distance_to_location);
struct location *evaluate_best_petrol_station(struct bot *b, struct location *location, int distance_to_location, int *best_petrol_distance, int for_distance);
struct location *get_location_of_type(struct bot *b, struct location *initial, struct location *curr, 
    int *distance_to_curr, int location_type);
int distance_to_final_sales(struct bot *b, struct location *location);
int size_of_map(struct bot *b);
int cargo_search(struct cargo *cargo, struct commodity *location_commodity);
void cargo_capacity_check(struct bot *b, struct cargo *cargo, int *weight_remaining, int *volume_remaining);
int buyer_total_for_cargo(struct bot *b);
int bots_on_location(struct location *location);

