#define _GNU_SOURCE

typedef struct {
    int SO_NAVI;
    int SO_PORTI;
    int SO_MERCI;
    int SO_SIZE;
    int SO_MIN_VITA;
    int SO_MAX_VITA;
    int SO_LATO;
    int SO_SPEED;
    int SO_CAPACITY;
    int SO_BANCHINE;
    int SO_FILL;
    int SO_LOADSPEED;
    int SO_DAYS;
} params;

/* Un tipo di merce*/
typedef struct {
    int quantita;
    int vita;
    int num_lots;
	int tipo;
} merce;

/* Struct che contiene tutte le merci generate da un porto */
typedef struct {
    int n_of_merch;
    int quantita;
    int vita;
    int num_lots;
    int tipo;
} merch_exchange;

typedef struct {
    int quantita; /* Tonnellate totali */
    int vita;
    int num_lots;
	int tipo; /* il tipo della merce*/
} merce_in_stiva;

typedef struct {
    pid_t ship_pid;
    int sem_id;
    int finished;
    int finished_port_out;
    int finished_port_in;
} processes_id;


/*-------------DUMP-------------*/

/* merce presente in un porto (disponibile per il carico) */
/* merce scaduta in porto */
/* merce consegnata ad un porto che la richiede */
typedef struct {
    int index;
    int merch_type;
    int merch_quantity;
    int total_quantity;
} merch_port;

/* merce presente su una nave */
/* merce scaduta in nave */
typedef struct {
    int index;
    int merch_type;
    int merch_quantity;
    int final_merch_type;
    int final_merch_quantity;
} merch_ship;

typedef struct {
    int ship_status;
    int merch_in_port;
    int merch_delivered;
    int merch_recived;
    int docks_occupied;
    int total_docks;
} dump;
/*
 * ship_status == 0: merce al porto
 * ship_status == 1: merce in nave con carico
 * ship_status == 2: merce in nave senza carico
 */

typedef struct {
    int shm[11];

    int sem[10];

    int msg[10];
} ipcs_id;