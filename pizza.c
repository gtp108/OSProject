#define CUSTOMER_COUNT 4
#define PIZZA_PRICE 5

#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <semaphore.h>
#include <inttypes.h>
#include <stdbool.h>
#include <unistd.h>
#include <time.h>

bool interrupt = false;

typedef struct {
  uint8_t id;
  sem_t * order;
  sem_t * pizza;
}
cashier_t;

typedef struct {
  uint8_t id;
  sem_t * init_done;
}simple_arg_t;

void * cook_run();
void * cashier_run();
void * customer_run();

void assure_state();

sem_t rack;
sem_t cook;
sem_t cashier;
sem_t cashier_awake;
sem_t customer;
sem_t customer_private_mutex;

cashier_t cashier_exchange;

uint8_t pizza_count = 0;
  uint8_t pizza_counts[CUSTOMER_COUNT];
uint8_t COOK_COUNT, CASHIER_COUNT, RACK_HOLDER_SIZE, WAITING_TIME;
int main(int argc, char ** argv) {

 
  srand(time(NULL));


    printf("Enter the number of cooks: ");
    scanf("%hhu", &COOK_COUNT);

    printf("Enter the number of cashiers: ");
    scanf("%hhu", &CASHIER_COUNT);

    printf("Enter the size of the rack holder: ");
    scanf("%hhu", &RACK_HOLDER_SIZE);

    printf("Enter the maximum waiting time: ");
    scanf("%hhu", &WAITING_TIME);
    for (uint8_t i = 0; i <CUSTOMER_COUNT; i++) {
    printf("Enter the number of pizzas for Customer %d: ", i);
    scanf("%hhu", &pizza_counts[i]);
  }
  /* Init all semaphores */
  sem_init( & rack, 0, 1);
  sem_init( & cashier, 0, 1);
  sem_init( & cashier_awake, 0, 0);
  sem_init( & cook, 0, RACK_HOLDER_SIZE);
  sem_init( & customer, 0, 0);
  sem_init( & customer_private_mutex, 0, 1);

  simple_arg_t args;
  sem_t init_done;
  sem_init( & init_done, 0, 0);
  args.init_done = & init_done;

  pthread_t cooks[COOK_COUNT];
  for (uint8_t i = 0; i < COOK_COUNT; i++) {

    args.id = i;

    if (pthread_create(cooks + i, NULL, cook_run, (void * ) & args)) {
      printf("[MAIN]\t\t ERROR: Unable to create cook thread.\n");
      exit(1);
    }

    sem_wait( & init_done);
  }

  pthread_t cashiers[CASHIER_COUNT];
  for (uint8_t i = 0; i < CASHIER_COUNT; i++) {

    args.id = i;

    if (pthread_create(cashiers + i, NULL, cashier_run, (void * ) & args)) {
      printf("[MAIN]\t\t ERROR: Unable to create cashier thread.\n");
      exit(2);
    }

    sem_wait( & init_done);
  }

   pthread_t customers[CUSTOMER_COUNT];
  for (uint8_t i = 0; i < CUSTOMER_COUNT; i++) {

    args.id = i;

    if (pthread_create(customers + i, NULL, customer_run, (void *)&args)) {
      printf("[MAIN]\t\t ERROR: Unable to create customer thread.\n");
      exit(3);
    }

    sem_wait(&init_done);
  }

  sem_destroy( & init_done);

  for (uint8_t i = 0; i < CUSTOMER_COUNT; i++) {

    if (pthread_join(customers[i], NULL)) {
      printf("[MAIN]\t\t ERROR: Unable to join cutomers[%d]\n", i);
      exit(4);
    }
  }

  printf("[MAIN]\t\t SUCCESS: All customers terminated\n");
  printf("\n----------------\n\n[MAIN]\t\t SUCCESS: Starting Cleanup\n");

  interrupt = true;

  for (uint8_t i = 0; i < COOK_COUNT; i++) {
    sem_post( & cook);
  }

  for (uint8_t i = 0; i < CASHIER_COUNT; i++) {
    sem_post( & customer);
  }

  printf("[MAIN]\t\t SUCCESS: Told all threads to terminate themselves\n");

  for (uint8_t i = 0; i < COOK_COUNT; i++) {

    if (pthread_join(cooks[i], NULL)) {
      printf("[MAIN]\t\t ERROR: Unable to join cooks[%d]\n", i);
      exit(5);
    }
  }

  for (uint8_t i = 0; i < CASHIER_COUNT; i++) {

    if (pthread_join(cashiers[i], NULL)) {
      printf("[MAIN]\t\t ERROR: Unable to join cashiers[%d]\n", i);
      exit(6);
    }
  }

  assure_state();
  printf("[MAIN]\t\t SUCCESS: All threads terminated, state consistent.\n");
}
void * cook_run(void * args) {

  simple_arg_t * args_ptr = (simple_arg_t * ) args;

  uint8_t cook_id = args_ptr -> id;

  printf("[COOK %d]\t CREATED.\n", cook_id);
  sem_post(args_ptr -> init_done);

  while (1) {

    sem_wait( & cook);
 
    if (interrupt) {
      break;
    }

    sleep(rand() % WAITING_TIME);

    sem_wait( & rack);
    assure_state();
    pizza_count++;
    assure_state();
    sem_post( & rack);
    printf("[COOK %d]\t Placed new pizza in rack.\n", cook_id);

    sem_post( & cashier);
  }
  printf("[COOK %d]\t DONE.\n", cook_id);
  return NULL;
}
void * cashier_run(void * args) {

  simple_arg_t * args_ptr = (simple_arg_t * ) args;

  uint8_t cashier_id = args_ptr -> id;

  sem_t order;
  sem_t pizza;
  sem_init( & order, 0, 0);
  sem_init( & pizza, 0, 0);

  printf("[CASHIER %d]\t CREATED.\n", cashier_id);
  sem_post(args_ptr -> init_done);

  while (1) {

    sem_wait( & customer);

    if (interrupt) {
      break;
    }

    printf("[CASHIER %d]\t Serving customer.\n", cashier_id);

    cashier_exchange.order = & order;
    cashier_exchange.pizza = & pizza;
    cashier_exchange.id = cashier_id;

    sem_post( & cashier_awake);

    sem_wait( & order);
    printf("[CASHIER %d]\t Got order.\n", cashier_id);

    printf("[CASHIER %d]\t Going to rack to get pizza...\n", cashier_id);

    sleep(rand() % WAITING_TIME);

    sem_wait( & cashier);

    sem_wait( & rack);
    assure_state();
    pizza_count--;
    assure_state();
    sem_post( & rack);
    sem_post( & cook);

    printf("[CASHIER %d]\t Got pizza from rack, going back\n", cashier_id);

    sleep(rand() % WAITING_TIME);

    sem_post( & pizza);
    printf("[CASHIER %d]\t Gave pizza to customer.\n", cashier_id);
  }

  sem_destroy( & order);
  sem_destroy( & pizza);
  printf("[CASHIER %d]\t DONE.\n", cashier_id);
  return NULL;
}
void * customer_run(void * args) {

  simple_arg_t * args_ptr = (simple_arg_t * ) args;

  uint8_t customer_id = args_ptr -> id;

  uint8_t pizza_count = pizza_counts[customer_id];
  printf("[CUSTOMER %d]\t CREATED.\n", customer_id);
  sem_post(args_ptr -> init_done);

  sleep(rand() % WAITING_TIME + 1);
 
  sem_wait( & customer_private_mutex);

  sem_post( & customer);
  sem_wait( & cashier_awake);
 
  sem_t * order = cashier_exchange.order;
  sem_t * pizza = cashier_exchange.pizza;
  uint8_t cashier_id = cashier_exchange.id;

  sem_post( & customer_private_mutex);

  printf("[CUSTOMER %d]\t Approached cashier no. %d.\n",
    customer_id, cashier_id);
 
  printf("[CUSTOMER %d]\t Placing order to cashier no. %d.\n",
    customer_id, cashier_id);

    int total_cost = pizza_counts[customer_id]* PIZZA_PRICE;

  sleep(rand() % WAITING_TIME);

  sem_post(order);

  sem_wait(pizza);
 
  printf("[CUSTOMER %d]\t Got pizza from cashier no. %d. Thank you!\n",
    customer_id, cashier_id);
  printf("[CUSTOMER %d]\t Ordered %d pizzas at $%d each. Total cost: $%d\n", customer_id,pizza_counts[customer_id], PIZZA_PRICE, total_cost);

  printf("[CUSTOMER %d]\t DONE.\n", customer_id);
  return NULL;
}
void assure_state() {
 
  if (pizza_count < 0) {
    printf("[ASSURE_STATE]\t ERROR: Negative pizza count!\n");
    exit(40);
  }
 
  if (pizza_count > RACK_HOLDER_SIZE) {
    printf("[ASSURE_STATE]\t ERROR: Rack overfull!\n");
    exit(41);
  }
  printf("[ASSURE_STATE]\t State consistent.\n");
}+
