#include "commons.h"

params *parameters;

merch_port *merch_in_port;
merch_port *merch_expired_port;
merch_port *merch_delivered_port;
merch_ship *merch_in_ship;
merch_ship *merch_expired_ship;

dump *dump_status;
ipcs_id ipc_remover;
processes_id *ships_id;

/*-------------------------------FUNCTIONS DECLARATION-------------------------------------------*/

void connect_parameters();

void take_parameters(const char *parameters_file);

void sig_handler(int signum);

merce *get_lot(merce *merch);

merch_exchange *take_merch_in(merch_exchange *merch);

merch_exchange *take_merch_out(merch_exchange *merch);

processes_id *take_ships();

void connect_dump();

dump *attachSharedMemoryDump(dump *shared_memory);

merch_port *attachSharedMemoryPort(merch_port *shared_memory, int key);

merch_ship *attachSharedMemoryShip(merch_ship *shared_memory, int key);

void inizialization_struct(processes_id *ships_id);

int get_sem();

void enter_sem(int sem_id);

int launchProcess(char *program, char *params[]);

void total_docks_numbers(int port_number);

int take_docks_number(int queue_id, my_msgbuf my_msg);

int total_quantity(int merch_number, merch_exchange *merch_out);

int preliminary_offer(int merch_number, merch_exchange *merch_in);

int prelimnary_request(int merch_number, merch_exchange *merch_out);

void get_out_sem(int sem_id);

void remove_merch_expired(int merch_expired_type, merch_exchange *merch_in, merch_exchange *merch_out);

void print_daily_dump();

void check_end(merch_exchange *merch_in, merch_exchange *merch_out, processes_id *ships_id);

void print_final_dump(int *initial_merch, int *big_merch_request, int *big_merch_offer);

void remove_ipc_obj();

/*----------------------------------FUNCTIONS BODY----------------------------------------------*/

int main(int argc, char const *argv[])
{
  int i, day, sem_id;
  int *initial_merch, *big_merch_request, *big_merch_offer;

  pid_t porti_pid;
  merce *merch;
  merch_exchange *merch_in;
  merch_exchange *merch_out;

  struct sigaction sa;

  char *file_name = "parameters.txt";
  connect_parameters();
  take_parameters(file_name);

  sa.sa_handler = sig_handler;
  sa.sa_flags = 0;
  sigaction(SIGINT, &sa, NULL);

  initial_merch = malloc(parameters->SO_MERCI * sizeof(int));
  big_merch_request = malloc(parameters->SO_MERCI * sizeof(int));
  big_merch_offer = malloc(parameters->SO_MERCI * sizeof(int));

  merch = NULL;
  merch_in = NULL;
  merch_out = NULL;
  ships_id = NULL;

  /* Shared memory connection */
  merch = get_lot(merch);
  merch_in = take_merch_in(merch_in);
  merch_out = take_merch_out(merch_out);

  ships_id = take_ships();
  connect_dump();
  inizialization_struct(ships_id);

  sem_id = get_sem();

  for (day = 0; day < parameters->SO_DAYS; day++)
  {
    if (day == 0)
    {
      enter_sem(sem_id);

      porti_pid = launchProcess("./porti", NULL);

      enter_sem(sem_id);
      for (i = 0; i < parameters->SO_MERCI; i++)
      {
        printf("merce %d:\tquantita': %d ton || tempo di vita: %d giorni\n", merch[i].tipo, merch[i].quantita, merch[i].vita);
      }
      for (i = 0; i < parameters->SO_PORTI; i++)
      {
        total_docks_numbers(i);
      }
      for (i = 0; i < parameters->SO_MERCI; i++)
      {
        initial_merch[i] = total_quantity(i, merch_out);         /* Total quantity of merch generated at the beginning of the simulation */
        big_merch_offer[i] = preliminary_offer(i, merch_in);     /* Port that offered the biggest quantity of merch */
        big_merch_request[i] = prelimnary_request(i, merch_out); /* Port that requested the biggest quantity of merch */
      }

      launchProcess("./navi", NULL);
      get_out_sem(sem_id);
    }

    sleep(1);

    for (i = 0; i < parameters->SO_NAVI; i++)
    {
      int status;
      waitpid(ships_id[i].ship_pid, &status, 0);
    }

    kill(porti_pid, SIGSTOP);
    for (i = 0; i < parameters->SO_NAVI; i++)
    {
      kill(ships_id[i].ship_pid, SIGSTOP);
    }

    for (i = 0; i < parameters->SO_MERCI; i++)
    {
      if (day == merch[i].vita)
      {
        printf("merce %d scaduta dopo %d giorni. La rimuovo\n", merch[i].tipo, day);
        remove_merch_expired(merch[i].tipo, merch_in, merch_out);
      }
    }

    print_daily_dump();

    check_end(merch_in, merch_out, ships_id);

    kill(porti_pid, SIGCONT);
    for (i = 0; i < parameters->SO_NAVI; i++)
    {
      kill(ships_id[i].ship_pid, SIGCONT);
    }
    printf("\nFinished day %d\n\n", day);

    if (ships_id->finished == 1)
    {
      printf("La simulazione termina\n");
      break;
    }
  }

  printf("Finito il loop, ora esco\n");
  for (i = 0; i < parameters->SO_NAVI; i++)
  {
    kill(ships_id[i].ship_pid, SIGTERM);
  }

  print_final_dump(initial_merch, big_merch_offer, big_merch_request);

  remove_ipc_obj();

  free(initial_merch);
  free(big_merch_offer);
  free(big_merch_request);

  return 0;
}

/* Create a shared memory where to put the configurations parameters readed by the file */
void connect_parameters()
{
  int shm_id;

  /* Create the shared memory segment */
  shm_id = shmget(SHM_KEY_PARAMS, sizeof(params), IPC_CREAT | 0600);
  TEST_ERROR
  if (shm_id == -1)
  {
    perror("Error creating shared memory");
    exit(EXIT_FAILURE);
  }

  ipc_remover.shm[10] = shm_id;
  printf("ok\n");

  /* Attach the struct to the shared memory segment */
  parameters = (params *)shmat(shm_id, NULL, 0);
  TEST_ERROR
  if (parameters == (void *)-1)
  {
    perror("Error attaching shared memory");
    exit(EXIT_FAILURE);
  }

  return;
}

/* Read the file with the parameters */
void take_parameters(const char *parameters_file)
{
  FILE *file = fopen(parameters_file, "r");
  TEST_ERROR
  if (file == NULL)
  {
    perror("Error opening file");
    exit(EXIT_FAILURE);
  }

  fscanf(file, "SO_NAVI %d\n", &parameters->SO_NAVI);
  fscanf(file, "SO_PORTI %d\n", &parameters->SO_PORTI);
  fscanf(file, "SO_MERCI %d\n", &parameters->SO_MERCI);
  fscanf(file, "SO_SIZE %d\n", &parameters->SO_SIZE);
  fscanf(file, "SO_MIN_VITA %d\n", &parameters->SO_MIN_VITA);
  fscanf(file, "SO_MAX_VITA %d\n", &parameters->SO_MAX_VITA);
  fscanf(file, "SO_LATO %d\n", &parameters->SO_LATO);
  fscanf(file, "SO_SPEED %d\n", &parameters->SO_SPEED);
  fscanf(file, "SO_CAPACITY %d\n", &parameters->SO_CAPACITY);
  fscanf(file, "SO_BANCHINE %d\n", &parameters->SO_BANCHINE);
  fscanf(file, "SO_FILL %d\n", &parameters->SO_FILL);
  fscanf(file, "SO_LOADSPEED %d\n", &parameters->SO_LOADSPEED);
  fscanf(file, "SO_DAYS %d\n", &parameters->SO_DAYS);

  fclose(file);
}

/* If the execution of the simulation is stopped by a SIGINT signal (Ctrl+C) the ipc objects are removed */
void sig_handler(int signum)
{

  remove_ipc_obj();

  exit(signum);
}

/* Take the dimension of a single lot created by the ports */
merce *get_lot(merce *merch)
{
  int shm_id;

  shm_id = shmget(SHM_KEY_MASTER_LOT, sizeof(merce *), IPC_CREAT | 0600);
  TEST_ERROR
  if (shm_id == -1)
  {
    perror("shmget");
    exit(EXIT_FAILURE);
  }

  ipc_remover.shm[0] = shm_id;

  merch = (merce *)shmat(shm_id, NULL, 0);
  TEST_ERROR
  if (merch == (void *)-1)
  {
    perror("shmat");
    exit(EXIT_FAILURE);
  }

  return merch;
}

/* Take the merch offered by all the ports */
merch_exchange *take_merch_in(merch_exchange *merch)
{
  int shm_id;

  shm_id = shmget(SHM_KEY_IN, sizeof(merch_exchange), IPC_CREAT | 0600);
  if (shm_id == -1)
  {
    perror("shmget");
    exit(EXIT_FAILURE);
  }

  ipc_remover.shm[1] = shm_id;

  merch = (merch_exchange *)shmat(shm_id, NULL, 0);
  if (merch == (void *)-1)
  {
    perror("shmat");
    exit(EXIT_FAILURE);
  }

  return merch;
}

/* Take the merch requested by all the ports */
merch_exchange *take_merch_out(merch_exchange *merch)
{
  int shm_id;

  shm_id = shmget(SHM_KEY_OUT, sizeof(merch_exchange), IPC_CREAT | 0600);
  if (shm_id == -1)
  {
    perror("shmget");
    exit(EXIT_FAILURE);
  }

  ipc_remover.shm[2] = shm_id;

  merch = (merch_exchange *)shmat(shm_id, NULL, 0);
  if (merch == (void *)-1)
  {
    perror("shmat");
    exit(EXIT_FAILURE);
  }

  return merch;
}

/* Receive the pids of the ships. These are important during the signal sending to the ships */
processes_id *take_ships()
{
  int shm_id;

  /* Create the shared memory segment */
  shm_id = shmget(SHM_KEY_MASTER, sizeof(processes_id), IPC_CREAT | 0600);
  TEST_ERROR
  if (shm_id == -1)
  {
    perror("Error creating shared memory");
    exit(EXIT_FAILURE);
  }

  ipc_remover.shm[3] = shm_id;

  /* Attach the struct to the shared memory segment */
  ships_id = (processes_id *)shmat(shm_id, NULL, 0);
  TEST_ERROR
  if (ships_id == (void *)-1)
  {
    perror("Error attaching shared memory");
    exit(EXIT_FAILURE);
  }

  return ships_id;
}

/* manager of the creation of a shared memory where to put all the informations needed for the dump printing */
void connect_dump()
{
  merch_in_port = attachSharedMemoryPort(merch_in_port, 0);
  merch_expired_port = attachSharedMemoryPort(merch_expired_port, 1);
  merch_delivered_port = attachSharedMemoryPort(merch_delivered_port, 2);

  merch_in_ship = attachSharedMemoryShip(merch_in_ship, 0);
  merch_expired_ship = attachSharedMemoryShip(merch_expired_ship, 1);

  dump_status = attachSharedMemoryDump(dump_status);

  return;
}

/* Attach a shared memory segment for the dump prints */
dump *attachSharedMemoryDump(dump *shared_memory)
{
  /* Create a shared memory segment */
  int shmid = shmget(SHM_DUMP_D, sizeof(dump), IPC_CREAT | 0666);

  ipc_remover.shm[9] = shmid;

  /* Attach the shared memory segment */
  shared_memory = (dump *)shmat(shmid, NULL, 0);

  return shared_memory;
}

/* Attach a shared memory segment for the informations about the ports */
merch_port *attachSharedMemoryPort(merch_port *shared_memory, int key)
{
  int shmid;
  int shared_k = SHM_DUMP_P + key;

  /* Create a shared memory segment */
  shmid = shmget(shared_k, sizeof(merch_port), IPC_CREAT | 0666);

  ipc_remover.shm[4 + key] = shmid;

  /* Attach the shared memory segment */
  shared_memory = (merch_port *)shmat(shmid, NULL, 0);

  return shared_memory;
}

/* Attach a shared memory segment for the informations about the ships */
merch_ship *attachSharedMemoryShip(merch_ship *shared_memory, int key)
{

  int shmid, shared_k = SHM_DUMP_S + key;

  /* Create a shared memory segment */
  shmid = shmget(shared_k, sizeof(merch_ship), IPC_CREAT | 0666);

  ipc_remover.shm[7 + key] = shmid;

  /* Attach the shared memory segment */
  shared_memory = (merch_ship *)shmat(shmid, NULL, 0);

  return shared_memory;
}

/* Inizialize to 0 some index used by the dump */
void inizialization_struct(processes_id *ships_id)
{
  int i, j;
  int soporti = parameters->SO_PORTI;

  for (i = 0; i < parameters->SO_PORTI; i++)
  {
    for (j = 0; j < parameters->SO_MERCI; j++)
    {
      merch_in_port[i * soporti + j].merch_type = -1;
      merch_expired_port[i * soporti + j].merch_type = -1;
      merch_delivered_port[i * soporti + j].merch_type = -1;
    }
    merch_expired_port[i].index = 0;
    merch_in_port[i].index = 0;
    merch_delivered_port[i].index = 0;

    dump_status[i].merch_in_port = 0;
    dump_status[i].merch_delivered = 0;
    dump_status[i].merch_recived = 0;
  }

  for (i = 0; i < parameters->SO_NAVI; i++)
  {
    merch_in_ship[i].merch_type = -1;
    merch_expired_ship[i].merch_type = -1;

    merch_in_ship[i].index = 0;
    merch_expired_ship[i].index = 0;
  }
  ships_id->finished_port_out = 0;
  ships_id->finished_port_in = 0;

  return;
}

/* Get the id of the semaphore used for the management of porti.c and navi.c launch*/
int get_sem()
{
  int sem_id;
  sem_id = semget(SEM_MASTER, 1, IPC_CREAT | 0600);
  TEST_ERROR
  if (sem_id == -1)
  {
    perror("semget");
    exit(1);
  }

  ipc_remover.sem[0] = sem_id;

  semctl(sem_id, 0, SETVAL, 1);

  return sem_id;
}

/* Enter the critic section of the semaphore created from the function above */
void enter_sem(int sem_id)
{
  struct sembuf sops;

  sops.sem_num = 0;
  sops.sem_op = -1;
  sops.sem_flg = 0;

  semop(sem_id, &sops, 1);

  return;
}

/* Function used to launch, via execve, the file with the ports and ships code */
int launchProcess(char *program, char *params[])
{
  pid_t pid = fork();

  if (pid == 0)
  {
    execve(program, (char **)params, NULL);
    TEST_ERROR

    exit(EXIT_SUCCESS);
  }
  else if (pid < 0)
  {
    perror("Error forking process");
    exit(EXIT_FAILURE);
  }

  return pid;
}

/* Read the messages from the ports indicate the numbers of docks in a port */
void total_docks_numbers(int port_number)
{
  int queue_id;
  my_msgbuf my_msg;

  queue_id = msgget(MSG_DOCKS, 0600);
  TEST_ERROR

  ipc_remover.msg[0] = queue_id;

  dump_status[port_number].total_docks = take_docks_number(queue_id, my_msg);

  return;
}

/* Function that actually read the messages from the ports */
int take_docks_number(int queue_id, my_msgbuf my_msg)
{
  int msg_type = 4;

  msgrcv(queue_id, &my_msg, sizeof(double), msg_type, 0);
  TEST_ERROR

  return (int)my_msg.mtext;
}

/* Calculate the total quantity of merch generated at the beginning of the simulation */
int total_quantity(int merch_number, merch_exchange *merch_out)
{
  int j, port, quantity = 0;
  int soporti = parameters->SO_PORTI;

  for (port = 0; port < parameters->SO_PORTI; port++)
  {
    for (j = 0; j < parameters->SO_MERCI; j++)
    {
      if (merch_number == merch_out[port * soporti + j].tipo)
      {
        quantity += merch_out[port * soporti + j].quantita * merch_out[port * soporti + j].num_lots;
        break;
      }
    }
  }
  return quantity;
}

/* Find the port that has offered the biggest quantity of merch */
int preliminary_offer(int merch_number, merch_exchange *merch_in)
{
  int port, j;
  int big_offerer = -1, big_merch_off = -1;
  int soporti = parameters->SO_PORTI;

  for (port = 0; port < parameters->SO_PORTI; port++)
  {
    for (j = 0; j < parameters->SO_MERCI; j++)
    {
      if (merch_number == merch_in[port * soporti + j].tipo)
      {
        if ((merch_in[port * soporti + j].num_lots * merch_in[port * soporti + j].quantita) > big_merch_off)
        {
          big_merch_off = merch_in[port * soporti + j].num_lots * merch_in[port * soporti + j].quantita;
          big_offerer = port;
        }
        break;
      }
    }
  }

  return big_offerer;
}

/* Find the port that has requested the biggest quantity of merch */
int prelimnary_request(int merch_number, merch_exchange *merch_out)
{
  int j, port;
  int big_applicant = -1, big_merch_req = -1;
  int soporti = parameters->SO_PORTI;

  for (port = 0; port < parameters->SO_PORTI; port++)
  {
    for (j = 0; j < parameters->SO_MERCI; j++)
    {
      if (merch_number == merch_out[port * soporti + j].tipo)
      {
        if ((merch_out[port * soporti + j].quantita * merch_out[port * soporti + j].num_lots) > big_merch_req)
        {
          big_merch_req = (merch_out[port * soporti + j].quantita * merch_out[port * soporti + j].num_lots);
          big_applicant = port;
        }
        break;
      }
    }
  }

  return big_applicant;
}

/* Exit the critic section of the semaphore used to manage the the execution */
void get_out_sem(int sem_id)
{
  struct sembuf sops;

  sops.sem_num = 0;
  sops.sem_op = 1;
  sops.sem_flg = 0;

  semop(sem_id, &sops, 1);

  return;
}

/* Each day the manager remove the merches that has expired in that day */
void remove_merch_expired(int merch_expired_type, merch_exchange *merch_in, merch_exchange *merch_out)
{
  int i, port, ship, index;
  int soporti = parameters->SO_PORTI, sonavi = parameters->SO_NAVI;

  for (port = 0; port < parameters->SO_PORTI; port++)
  {
    for (i = 0; i < merch_out[port].n_of_merch; i++)
    {
      if (merch_out[port * soporti + i].tipo == merch_expired_type)
      {
        printf("\t\t\trimossa la merce %d dal porto %d in vendita\n", merch_out[port * soporti + i].tipo, port);

        index = merch_expired_port[port].index;
        merch_expired_port[port * soporti + index].merch_type = merch_expired_type;
        merch_expired_port[port * soporti + index].merch_quantity = merch_out[port * soporti + i].quantita * merch_out[port * soporti + index].num_lots;
        merch_expired_port[port].index++;

        dump_status[port].merch_in_port -= merch_expired_port[port * soporti + index].merch_quantity;

        merch_out[port * soporti + i].tipo = -1;
        merch_out[port * soporti + i].quantita = -1;

        merch_in_port[port * soporti + i].merch_type = -1;
        merch_in_port[port * soporti + i].merch_quantity = -1;
        break;
      }
    }
  }

  for (ship = 0; ship < parameters->SO_NAVI; ship++)
  {
    if (merch_in_ship[ship].merch_type == merch_expired_type)
    {
      merch_expired_ship[ship].merch_type = merch_expired_type;
      merch_expired_ship[ship].merch_quantity = merch_in_ship[ship].merch_quantity;

      index = merch_expired_ship[ship].index;
      merch_expired_ship[ship * sonavi + index].final_merch_type = merch_expired_type;
      merch_expired_ship[ship * sonavi + index].final_merch_quantity = merch_in_ship[ship].merch_quantity;
      merch_expired_ship[ship].index++;
    }
  }

  for (port = 0; port < parameters->SO_PORTI; port++)
  {
    for (i = 0; i < merch_in[port].n_of_merch; i++)
    {
      if (merch_in[port * soporti + i].tipo == merch_expired_type)
      {
        printf("\t\t\trimossa la merce %d dal porto %d in richiesta\n", merch_in[port * soporti + i].tipo, port);
        merch_in[port * soporti + i].tipo = -1;
        merch_in[port * soporti + i].quantita = -1;
        break;
      }
    }
  }

  return;
}

/* All the data printed after each day of execution */
void print_daily_dump()
{
  int i, port, ship;
  int ship_at_port = 0, ship_cargo = 0, ship_no_cargo = 0;
  int soporti = parameters->SO_PORTI;

  printf("____________________________________________________________\n\n");
  printf("Merce presente in porto:\n");
  printf("____________________________________________________________\n");
  for (port = 0; port < parameters->SO_PORTI; port++)
  {
    printf("\tPorto %d:\n", port);
    for (i = 0; i < merch_in_port[port].index; i++)
    {
      if (merch_in_port[port * soporti + i].merch_type != -1)
      {
        printf("| Merch %d | Quantita': %d |\n", merch_in_port[port * soporti + i].merch_type, merch_in_port[port * soporti + i].merch_quantity);
      }
    }
    printf("------------------------------------------------------\n");
    printf("\n\n");
  }

  printf("\n");
  printf("____________________________________________________________\n");
  printf("Merce scaduta in porto:\n");
  printf("____________________________________________________________\n");
  for (port = 0; port < parameters->SO_PORTI; port++)
  {
    printf("\tPorto %d:\n", port);
    for (i = 0; i < merch_expired_port[port].index; i++)
    {
      if (merch_expired_port[port * soporti + i].merch_type != -1)
      {
        printf("| Merch %d | Quantita': %d |\n", merch_expired_port[port * soporti + i].merch_type, merch_expired_port[port * soporti + i].merch_quantity);
      }
    }
    printf("------------------------------------------------------\n");
    printf("\n\n");
  }

  printf("\n");

  printf("____________________________________________________________\n\n");
  printf("Merce consegnata in porto:\n");
  printf("____________________________________________________________\n");
  for (port = 0; port < parameters->SO_PORTI; port++)
  {
    printf("\tPorto %d:\n", port);
    for (i = 0; i < merch_delivered_port[port].index; i++)
    {
      if (merch_delivered_port[port * soporti + i].merch_type != -1)
      {
        printf("| Merch %d | Quantita': %d |\n", merch_delivered_port[port * soporti + i].merch_type, merch_delivered_port[port * soporti + i].merch_quantity);
      }
    }
    printf("------------------------------------------------------\n");
    printf("\n\n");
  }

  printf("\n");

  printf("____________________________________________________________\n\n");
  printf("Merce in nave:\n");
  printf("____________________________________________________________\n");
  for (ship = 0; ship < parameters->SO_NAVI; ship++)
  {
    if (merch_in_ship[ship].merch_type != -1)
    {
      printf("\n");
      printf("| Nave %d | Merch %d | Quantita': %d |\n", ship, merch_in_ship[ship].merch_type, merch_in_ship[ship].merch_quantity);
      printf("\n");
    }
  }

  printf("\n");

  printf("____________________________________________________________\n\n");
  printf("Merce scaduta in nave:\n");
  printf("____________________________________________________________\n");
  for (ship = 0; ship < parameters->SO_NAVI; ship++)
  {
    if (merch_expired_ship[ship].merch_type != -1)
    {
      printf("\n");
      printf("| Nave %d | Merch %d | Quantita': %d |\n", ship, merch_expired_ship[ship].merch_type, merch_expired_ship[ship].merch_quantity);
      printf("\n\n");
    }
  }

  printf("\n");

  for (i = 0; i < parameters->SO_NAVI; i++)
  {
    if (dump_status[i].ship_status == 0)
    {
      ship_at_port++;
    }
    else if (dump_status[i].ship_status == 1)
    {
      ship_cargo++;
    }
    else if (dump_status[i].ship_status == 2)
    {
      ship_no_cargo++;
    }
    else
    {
      printf("Errore nella nave %d (MASTER)\n", i);
    }
  }

  printf("____________________________________________________________\n\n");
  printf("Numero di navi:\n");
  printf("____________________________________________________________\n");
  printf("\n");
  printf("| Al porto | In mare con carico | In mare senza carico |\n");
  printf("|   %d     |          %d        |           %d         |\n", ship_at_port, ship_cargo, ship_no_cargo);

  printf("\n");

  printf("____________________________________________________________\n\n");
  printf("Quantita' di merce per porto\n");
  printf("____________________________________________________________\n");
  for (port = 0; port < parameters->SO_PORTI; port++)
  {
    printf("\tPorto %d:\n", port);
    printf("| Tonnellate in porto | Tonnellate spedite | Tonnellate ricevute |\n");
    printf("|         %d          |          %d        |          %d         |\n", dump_status[port].merch_in_port, dump_status[port].merch_delivered, dump_status[port].merch_recived);
    printf("\n");
    printf("Numero totale di banchine: %d | Numero di banchine occupate: %d\n", dump_status[port].total_docks, dump_status[port].docks_occupied);
    printf("------------------------------------------------------\n");
  }

  printf("\n");

  printf("__________________________________________________________________________________________________\n");
}

/* Check if the simulations need to end. If that is the case, it end the simulation */
void check_end(merch_exchange *merch_in, merch_exchange *merch_out, processes_id *ships_id)
{
  int i, port;
  int finished_merch_out, finished_merch_in;
  int soporti = parameters->SO_PORTI;

  for (port = 0; port < parameters->SO_PORTI; port++)
  {
    finished_merch_out = 0;
    finished_merch_in = 0;

    for (i = 0; i < merch_out[port].n_of_merch; i++)
    {
      if (merch_out[port * soporti + i].tipo != -1)
      {
        finished_merch_out++;
        break;
      }
    }

    if (finished_merch_out == 0)
    {
      ships_id->finished_port_out++;
    }

    for (i = 0; i < merch_in[port].n_of_merch; i++)
    {
      if (merch_in_port[port * soporti + i].merch_type != -1)
      {
        finished_merch_in++;
        break;
      }
    }

    if (finished_merch_in == 0)
    {
      ships_id->finished_port_in++;
    }
  }

  if (ships_id->finished_port_out == parameters->SO_PORTI)
  {
    printf("(MASTER) Non c'e' piu' nessun porto che vende le merci, quindi la simulazione termina\n\n");
    ships_id->finished = 1;
  }
  else if (finished_merch_in == parameters->SO_PORTI)
  {
    printf("(MASTER) Non c'e' piu' nessun porto che offre le merci, quindi la simulazione termina\n\n");
    ships_id->finished = 1;
  }
  else
  {
    ships_id->finished_port_out = 0;
    ships_id->finished_port_in = 0;
  }

  return;
}

/* All the data printed after the ending of the execution */
void print_final_dump(int *initial_merch, int *big_merch_request, int *big_merch_offer)
{
  int i, j, port, ship;
  int ship_at_port = 0, ship_cargo = 0, ship_no_cargo = 0, quantity = 0;
  int soporti = parameters->SO_PORTI, sonavi = parameters->SO_NAVI;

  printf("____________________________________________________________\n\n");
  printf("Final dump:\n");
  printf("____________________________________________________________\n\n");

  for (i = 0; i < parameters->SO_NAVI; i++)
  {
    if (dump_status[i].ship_status == 0)
    {
      ship_at_port++;
    }
    else if (dump_status[i].ship_status == 1)
    {
      ship_cargo++;
    }
    else if (dump_status[i].ship_status == 2)
    {
      ship_no_cargo++;
    }
  }
  printf("Navi ancora in mare con carico a bordo: %d\n", ship_cargo);
  printf("Navi ancora in mare senza carico a bordo: %d\n", ship_no_cargo);
  printf("Navi al porto attraccate a una banchina: %d\n", ship_at_port);

  for (i = 0; i < parameters->SO_MERCI; i++)
  {
    printf("Merce %d:\n", i);

    printf("\tTonnellate disponibili per il carico: ");
    for (port = 0; port < parameters->SO_PORTI; port++)
    {
      for (j = 0; j < parameters->SO_MERCI; j++)
      {
        if (i == merch_in_port[port * soporti + j].merch_type)
        {
          quantity += merch_in_port[port * soporti + j].merch_quantity;
        }
      }
    }
    printf("%d\n", quantity);
    printf("------------------------------------------------------\n");
    quantity = 0;

    printf("\tTonnellate scadute in porto: ");
    for (port = 0; port < parameters->SO_PORTI; port++)
    {
      for (j = 0; j < parameters->SO_MERCI; j++)
      {
        if (i == merch_expired_port[port * soporti + j].merch_type)
        {
          quantity += merch_expired_port[port * soporti + j].merch_quantity;
        }
      }
    }
    printf("%d\n", quantity);
    printf("------------------------------------------------------\n");
    quantity = 0;

    printf("\tTonnellate consegnate in un porto: ");
    for (port = 0; port < parameters->SO_PORTI; port++)
    {
      for (j = 0; j < parameters->SO_MERCI; j++)
      {
        if (i == merch_delivered_port[port * soporti + j].merch_type)
        {
          quantity += merch_delivered_port[port * soporti + j].merch_quantity;
        }
      }
    }
    printf("%d\n", quantity);
    printf("------------------------------------------------------\n");
    quantity = 0;

    printf("\tTonnellate in viaggio in nave: ");
    for (ship = 0; ship < parameters->SO_NAVI; ship++)
    {
      if (i == merch_in_ship[ship].merch_type)
      {
        quantity += merch_in_ship[ship].merch_quantity;
        break;
      }
    }
    printf("%d\n", quantity);
    printf("------------------------------------------------------\n");
    quantity = 0;

    printf("\tTonnellate scadute in nave: ");
    for (ship = 0; ship < parameters->SO_NAVI; ship++)
    {
      for (j = 0; j < parameters->SO_MERCI; j++)
      {
        if (i == merch_expired_ship[ship * sonavi + j].final_merch_type)
        {
          quantity += merch_expired_ship[ship * sonavi + j].final_merch_quantity;
        }
      }
    }
    printf("%d\n", quantity);
    printf("------------------------------------------------------\n");
    quantity = 0;

    printf("\n  Quantita' generate dalla merce %d\n", i);
    printf("\tTonnellate generate a inizio simulazione: %d\n", initial_merch[i]);
    printf("------------------------------------------------------\n");

    printf("\tTonnellate rimaste ferme in porto: ");
    for (port = 0; port < parameters->SO_PORTI; port++)
    {
      for (j = 0; j < parameters->SO_MERCI; j++)
      {
        if (i == merch_in_port[port * soporti + j].merch_type)
        {
          quantity += merch_in_port[port * soporti + j].merch_quantity;
        }
        if (i == merch_expired_port[port * soporti + j].merch_type)
        {
          quantity += merch_expired_port[port * soporti + j].merch_quantity;
        }
      }
    }
    printf("%d\n", quantity);
    printf("------------------------------------------------------\n");
    quantity = 0;

    printf("\tPorto che ha offerto la maggior quantita': %d\n", big_merch_offer[i]);
    printf("------------------------------------------------------\n");
    printf("\tPorto che ha richiesto la maggior quantita': %d\n", big_merch_request[i]);

    printf("\n\n");
  }

  printf("____________________________________________________________\n");
  printf("\nPorti:\n");
  printf("____________________________________________________________\n\n");
  for (port = 0; port < parameters->SO_PORTI; port++)
  {
    printf("Porto: %d\n", port);
    printf("\tQuantita' di merce in porto: ");
    for (i = 0; i < parameters->SO_MERCI; i++)
    {
      if (merch_in_port[port * soporti + i].merch_type != -1)
      {
        quantity += merch_in_port[port * soporti + i].merch_quantity;
      }
    }
    printf("%d\n", quantity);
    printf("------------------------------------------------------\n");
    quantity = 0;

    printf("\tQuantita' di merce spedita: ");
    quantity = merch_in_port[port].total_quantity;
    printf("%d\n", quantity);
    printf("------------------------------------------------------\n");

    printf("\tQuantita' di merce ricevuta: ");
    quantity = merch_delivered_port[port].total_quantity;
    printf("%d\n", quantity);
    printf("------------------------------------------------------\n");
    quantity = 0;
  }
}

/* After the end of the simulation, it clear the ipc objects created during the execution */
void remove_ipc_obj()
{
  int i;

  msgctl(ipc_remover.msg[0], IPC_RMID, NULL);

  semctl(ipc_remover.sem[0], 0, IPC_RMID);
  for (i = 0; i < parameters->SO_PORTI; i++)
  {
    semctl(ships_id[i].sem_id, 0, IPC_RMID);
  }

  for (i = 0; i < 11; i++)
  {
    shmctl(ipc_remover.shm[i], IPC_RMID, NULL);
  }

  return;
}
