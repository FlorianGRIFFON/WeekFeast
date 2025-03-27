#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <ctype.h>

typedef struct {
    char name[50];
    char difficulty[10];
} Dish;

typedef struct {
    Dish* dishes;
    int count;
} DishList;

typedef struct {
    char dishes[14][50];
} WeekPlan;

DishList parse_json(const char* filename);
WeekPlan generate_week(DishList dishes, const char* difficulty, FILE* history);
void save_plan(WeekPlan plan, const char* filename);
int load_previous_weeks(WeekPlan* previous, const char* filename);
int is_dish_used(WeekPlan* plans, int plan_count, const char* dish);

void trim(char* str) {
    int len = strlen(str);
    while (len > 0 && isspace(str[len - 1])) {
        str[--len] = '\0';
    }
    int start = 0;
    while (isspace(str[start])) start++;
    if (start > 0) memmove(str, str + start, len - start + 1);
}

DishList parse_json(const char* filename) {
    DishList list = {NULL, 0};
    FILE* fp = fopen(filename, "r");
    if (!fp) {
        printf("Error opening JSON file\n");
        return list;
    }

    char buffer[1024];
    int count = 0;
    while (fgets(buffer, sizeof(buffer), fp)) {
        if (strstr(buffer, "\"name\"")) count++;
    }
    rewind(fp);

    list.dishes = (Dish*)malloc(count * sizeof(Dish));
    list.count = count;

    int index = 0;
    char* ptr;
    while (fgets(buffer, sizeof(buffer), fp) && index < count) {
        if ((ptr = strstr(buffer, "\"name\""))) {
            sscanf(ptr, "\"name\":\"%49[^\"]\",\"difficulty\":\"%9[^\"]\"", 
                   list.dishes[index].name, list.dishes[index].difficulty);
            trim(list.dishes[index].difficulty);
            index++;
        }
    }
    fclose(fp);
    return list;
}

WeekPlan generate_week(DishList dishes, const char* difficulty, FILE* history) {
    WeekPlan plan = {{{0}}};
    WeekPlan previous[2];
    int prev_count = load_previous_weeks(previous, "meal_history.txt");
    
    srand(time(NULL));
    
    // Define target counts for each difficulty
    int easy_target, medium_target, hard_target;
    if (strcmp(difficulty, "easy") == 0) {
        easy_target = 10;   // Mostly easy
        medium_target = 4;  // Some medium
        hard_target = 0;    // No hard
    } else if (strcmp(difficulty, "medium") == 0) {
        easy_target = 5;    // Some easy
        medium_target = 6;  // Mostly medium
        hard_target = 3;    // Some hard
    } else if (strcmp(difficulty, "hard") == 0) {
        easy_target = 2;    // Little easy
        medium_target = 5;  // Medium amount
        hard_target = 7;    // Mostly hard
    } else { // "mixed"
        easy_target = 5;
        medium_target = 5;
        hard_target = 4;
    }

    // Count eligible dishes for each difficulty
    int easy_count = 0, medium_count = 0, hard_count = 0;
    for (int i = 0; i < dishes.count; i++) {
        if (!is_dish_used(previous, prev_count, dishes.dishes[i].name)) {
            if (strcmp(dishes.dishes[i].difficulty, "easy") == 0) easy_count++;
            else if (strcmp(dishes.dishes[i].difficulty, "medium") == 0) medium_count++;
            else if (strcmp(dishes.dishes[i].difficulty, "hard") == 0) hard_count++;
        }
    }

    // Adjust targets if not enough dishes are available
    int total_available = easy_count + medium_count + hard_count;
    if (total_available < 14) {
        printf("Warning: Only %d total unique dishes available, need 14\n", total_available);
    }
    if (easy_count < easy_target) easy_target = easy_count;
    if (medium_count < medium_target) medium_target = medium_count;
    if (hard_count < hard_target) hard_target = hard_count;

    // Fill remaining slots after initial targets
    int slots_left = 14 - (easy_target + medium_target + hard_target);
    while (slots_left > 0 && (easy_count > easy_target || medium_count > medium_target || hard_count > hard_target)) {
        if (easy_count > easy_target) { easy_target++; slots_left--; }
        else if (medium_count > medium_target) { medium_target++; slots_left--; }
        else if (hard_count > hard_target) { hard_target++; slots_left--; }
    }

    // Build arrays of eligible indices for each difficulty
    int* easy_indices = (int*)malloc(easy_count * sizeof(int));
    int* medium_indices = (int*)malloc(medium_count * sizeof(int));
    int* hard_indices = (int*)malloc(hard_count * sizeof(int));
    int e = 0, m = 0, h = 0;
    for (int i = 0; i < dishes.count; i++) {
        if (!is_dish_used(previous, prev_count, dishes.dishes[i].name)) {
            if (strcmp(dishes.dishes[i].difficulty, "easy") == 0) easy_indices[e++] = i;
            else if (strcmp(dishes.dishes[i].difficulty, "medium") == 0) medium_indices[m++] = i;
            else if (strcmp(dishes.dishes[i].difficulty, "hard") == 0) hard_indices[h++] = i;
        }
    }

    // Fill the plan
    int slot = 0;
    // Easy dishes
    for (int i = 0; i < easy_target; i++) {
        int rand_idx = rand() % (easy_count - i);
        strcpy(plan.dishes[slot++], dishes.dishes[easy_indices[rand_idx]].name);
        easy_indices[rand_idx] = easy_indices[easy_count - i - 1];
    }
    // Medium dishes
    for (int i = 0; i < medium_target; i++) {
        int rand_idx = rand() % (medium_count - i);
        strcpy(plan.dishes[slot++], dishes.dishes[medium_indices[rand_idx]].name);
        medium_indices[rand_idx] = medium_indices[medium_count - i - 1];
    }
    // Hard dishes
    for (int i = 0; i < hard_target; i++) {
        int rand_idx = rand() % (hard_count - i);
        strcpy(plan.dishes[slot++], dishes.dishes[hard_indices[rand_idx]].name);
        hard_indices[rand_idx] = hard_indices[hard_count - i - 1];
    }

    free(easy_indices);
    free(medium_indices);
    free(hard_indices);
    return plan;
}

int load_previous_weeks(WeekPlan* previous, const char* filename) {
    FILE* fp = fopen(filename, "r");
    if (!fp) return 0;
    
    char buffer[1024];
    int week_count = 0;
    int slot = 0;
    
    while (fgets(buffer, sizeof(buffer), fp) && week_count < 2) {
        if (strstr(buffer, "Day")) {
            if (slot % 14 == 0 && slot > 0) week_count++;
            if (week_count >= 2) break;
            sscanf(buffer, "%*[^:]: %49[^\n]", previous[week_count].dishes[slot % 14]);
            slot++;
        }
    }
    fclose(fp);
    return week_count + (slot > 0 ? 1 : 0);
}

int is_dish_used(WeekPlan* plans, int plan_count, const char* dish) {
    for (int i = 0; i < plan_count; i++) {
        for (int j = 0; j < 14; j++) {
            if (strcmp(plans[i].dishes[j], dish) == 0) return 1;
        }
    }
    return 0;
}

void save_plan(WeekPlan plan, const char* filename) {
    FILE* fp = fopen(filename, "a");
    if (!fp) {
        printf("Error opening output file\n");
        return;
    }
    
    fprintf(fp, "\nNew Weekly Plan:\n");
    for (int i = 0; i < 7; i++) {
        fprintf(fp, "Day %d: %s, %s\n", i + 1, 
                plan.dishes[i * 2], plan.dishes[i * 2 + 1]);
    }
    fclose(fp);
}

int main(int argc, char* argv[]) {
    if (argc != 3) {
        printf("Usage: %s <json_file> <difficulty: easy|medium|hard|mixed>\n", argv[0]);
        return 1;
    }

    DishList dishes = parse_json(argv[1]);
    if (dishes.count == 0) {
        printf("No dishes loaded\n");
        return 1;
    }

    WeekPlan plan = generate_week(dishes, argv[2], NULL);
    save_plan(plan, "meal_history.txt");

    free(dishes.dishes);
    return 0;
}