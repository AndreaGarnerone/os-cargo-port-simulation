#include "commons.h"

params *parameters;

/*-------------------------------FUNCTIONS DECLARATION-------------------------------------------*/

int connect_parameters();

merce *public_lot(merce *merch);

void gen_cord_port(double *p_x_cord, double *p_y_cord, int port_number);

void send_cord(double p_x_cord, double p_y_cord);

void merch_manager(merce *merch, int port_number, merch_exchange *merch_in, merch_exchange *merch_out, pid_t *pids);

void send_docks(int docks_number);

int gen_quantity(int tot_merch);

int gen_merch_in(int number_merch_in, int *type_in, merce *merch, pid_t *pids);

int gen_merch_out(int n_of_merch_type_out, int *type_out, int n_of_merch_type_in, int *type_in, merce *merch, pid_t *pids);

int fill_merch_in(int number_merch_in, int total_quantity_in, int quantity_merch_in, int *type_in, merce *merch);

int fill_merch_out(int n_of_merch_type_out, int actual_quantity_out, int ton_merch_out, int *type_out, merce *merch);

int create_docks(int port_number, int docks_number);

int public_merch_in(merch_exchange *merch_in, int port_number, merce *merch, int *type_in, int n_of_merch_type_in);

int public_merch_out(merch_exchange *merch_out, int port_number, merce *merch, int *type_out, int n_of_merch_type_out);

int exit_sem();

/*----------------------------------FUNCTIONS BODY----------------------------------------------*/

int main(int argc, char const *argv[])
{
  int i, status;
  double p_x_cord, p_y_cord;
  pid_t *pids;
  pid_t child_pid;
  merce *merch;             /* La struct indica un tipo di merce */
  merch_exchange merch_out; /* Array delle merci che ogni porto vende */
  merch_exchange merch_in;  /* Array delle merci che ogni porto richiede */

  connect_parameters();

  /* There must be at least 4 ports, one for each corner of the map */
  if (parameters->SO_PORTI < 4)
  {
    fprintf(stderr, "Il numero di porti (%d) non puo' essere inferiore a 4\n", parameters->SO_PORTI);
    exit(EXIT_FAILURE);
  }

  merch = NULL;
  merch = public_lot(merch);

  pids = malloc(parameters->SO_PORTI * sizeof(pid_t));

  /* Ports generation */
  for (i = 0; i < parameters->SO_PORTI; i++)
  {
    switch (child_pid = fork())
    {
    case -1:
      perror("fork error\n");
      exit(EXIT_FAILURE);
      break;

    case 0:
      pids[i] = getpid();
      srand(pids[i]);

      gen_cord_port(&p_x_cord, &p_y_cord, i);

      /* Send the coordinates of the ports to the ships*/
      if (i >= 4)
      {
        send_cord(p_x_cord, p_y_cord);
      }

      merch_manager(merch, i, &merch_in, &merch_out, pids);

      exit(EXIT_SUCCESS);
      break;

    default:
      break;
    }
  }

  while ((child_pid = wait(&status)) != -1)
    ;

  exit_sem();

  free(pids);

  return 0;
}

/* Connect the shared memory segment created by the master to take the configuration parameters */
int connect_parameters()
{
  int shm_id;

  /* Create the shared memory segment */
  shm_id = shmget(SHM_KEY_PARAMS, sizeof(params), 0600);
  TEST_ERROR
  if (shm_id == -1)
  {
    perror("Error creating shared memory");
    exit(EXIT_FAILURE);
  }

  /* Attach the struct to the shared memory segment */
  parameters = (params *)shmat(shm_id, NULL, 0);
  TEST_ERROR
  if (parameters == (void *)-1)
  {
    perror("Error attaching shared memory");
    exit(EXIT_FAILURE);
  }

  return shm_id;
}

/* Connect the shared memory segment for sharing the dimension of a single lot of merch */
merce *public_lot(merce *merch)
{
  int shm_id, i;

  shm_id = shmget(SHM_KEY_MASTER_LOT, sizeof(merce *), IPC_CREAT | 0600);
  TEST_ERROR
  if (shm_id == -1)
  {
    perror("shmget");
    exit(EXIT_FAILURE);
  }

  merch = (merce *)shmat(shm_id, NULL, 0);
  TEST_ERROR
  if (merch == (void *)-1)
  {
    perror("shmat");
    exit(EXIT_FAILURE);
  }

  srand(time(NULL));
  for (i = 0; i < parameters->SO_MERCI; i++)
  {
    merch[i].tipo = i;
    merch[i].quantita = rand() % (parameters->SO_SIZE) + 1;
    merch[i].vita = rand() % (parameters->SO_MAX_VITA - parameters->SO_MIN_VITA + 1) + parameters->SO_MIN_VITA;
  }

  return merch;
}

/* Create the cordinates for the ports. The first four in the corners, the other ones randomly generated */
void gen_cord_port(double *p_x_cord, double *p_y_cord, int port_number)
{
  if (port_number < 4)
  {
    switch (port_number)
    {
    case 0:
      *p_x_cord = 0;
      *p_y_cord = 0;
      break;

    case 1:
      *p_x_cord = parameters->SO_LATO;
      *p_y_cord = 0;
      break;

    case 2:
      *p_x_cord = 0;
      *p_y_cord = parameters->SO_LATO;
      break;

    case 3:
      *p_x_cord = parameters->SO_LATO;
      *p_y_cord = parameters->SO_LATO;
      break;
    }
  }
  else
  {
    *p_x_cord = ((double)rand()) / RAND_MAX * parameters->SO_LATO;
    *p_y_cord = ((double)rand()) / RAND_MAX * parameters->SO_LATO;
  }

  return;
}

/* A message queue share the cordinates of each port to the ships */
void send_cord(double p_x_cord, double p_y_cord)
{
  int q_id;
  my_msgbuf my_msg;

  /* Create Message Queue */
  q_id = msgget(MSG_KEY, IPC_CREAT | 0600);
  TEST_ERROR;

  my_msg.mtype = 2;
  my_msg.mtext = p_x_cord;
  if (msgsnd(q_id, &my_msg, sizeof(double), 0) == -1)
  {
    perror("msgsnd x cord failed in send_cord\n");
    exit(1);
  }
  TEST_ERROR

  my_msg.mtype = 3;
  my_msg.mtext = p_y_cord;
  if (msgsnd(q_id, &my_msg, sizeof(double), 0) == -1)
  {
    perror("msgsnd y cord failed in send_cord\n");
    exit(1);
  }
  TEST_ERROR

  return;
}

/* Manager function that manage the entire process of merch creation and sharing */
void merch_manager(merce *merch, int port_number, merch_exchange *merch_in, merch_exchange *merch_out, pid_t *pids)
{
  int i;
  int tot_merch, ton_merch_in, ton_merch_out;  /* Quantita' di merce per ogni porto */
  int n_of_merch_type_in, n_of_merch_type_out; /* Numero di tipi di merce che il porto richiede e vende */
  int *type_in, *type_out;                     /*futuri array che contengono i tipi di merce in entrata e in uscita*/
  int actual_quantity_in, actual_quantity_out; /* Quantita' di merce dopo la generazione, possibilmente diversa da quella desiderata*/
  double divider, docks_number;

  srand(pids[port_number]);
  /* Divide the quantity of merch by SO_FILL between all the ports equally */
  divider = 1.0 / parameters->SO_PORTI;
  tot_merch = divider * parameters->SO_FILL;

  /* Generate the numbers of docks that a port have */
  docks_number = (rand() % parameters->SO_BANCHINE) + 1;

  send_docks(docks_number);

  /* Generate the types of merch that the port require and offer */
  if (parameters->SO_MERCI != 1)
  {
    n_of_merch_type_out = (rand() % (parameters->SO_MERCI - 1)) + 1;
    n_of_merch_type_in = (rand() % (parameters->SO_MERCI - n_of_merch_type_out)) + 1;
  }
  else
  {
    n_of_merch_type_out = 1;
    n_of_merch_type_in = 1;
  }

  /* Generate the quantity of merch that the port require and offer */
  ton_merch_out = gen_quantity(tot_merch);
  ton_merch_in = tot_merch - ton_merch_out;

  /* Array with the types of merch to offer and require.
   * It help to assure that the same merch is not in the
   * require and the offer at the same port */
  type_in = malloc(n_of_merch_type_in * sizeof(*type_in));
  type_out = malloc(n_of_merch_type_out * sizeof(*type_out));

  /* Create the types of merch */
  actual_quantity_in = gen_merch_in(n_of_merch_type_in, type_in, merch, pids);

  actual_quantity_out = gen_merch_out(n_of_merch_type_out, type_out, n_of_merch_type_in, type_in, merch, pids);

  for (i = 0; i < n_of_merch_type_in; i++)
  {
    merch[type_in[i]].num_lots = 1; /* The lots are initialized to 1, then some will be changed */
  }

  for (i = 0; i < n_of_merch_type_out; i++)
  {
    merch[type_out[i]].num_lots = 1; /* The lots are initialized to 1, then some will be changed */
  }

  if (actual_quantity_in < ton_merch_in)
  {
    actual_quantity_in = fill_merch_in(n_of_merch_type_in, actual_quantity_in, ton_merch_in, type_in, merch);
  }
  if (actual_quantity_out < ton_merch_out)
  {
    actual_quantity_out = fill_merch_out(n_of_merch_type_out, actual_quantity_out, ton_merch_out, type_out, merch);
  }

  create_docks(port_number, docks_number);

  /* All the merch in the port must be putted into the shared memory so that the ships can
   * decide where to ship the merch */
  public_merch_in(merch_in, port_number, merch, type_in, n_of_merch_type_in);

  /* After their creation, the biggest merchandise in the port is put in the shared memory
   * ready to be taken by a ship and removed from the resources in the port */
  public_merch_out(merch_out, port_number, merch, type_out, n_of_merch_type_out);

  free(type_in);
  free(type_out);

  return;
}

/* Another message queue that send the number of the docks of each port. A number generated between 1 and SO_BANCHIINE */
void send_docks(int docks_number)
{
  int q_id;
  my_msgbuf my_msg;

  /* Create Message Queue */
  q_id = msgget(MSG_DOCKS, IPC_CREAT | 0600);
  TEST_ERROR;

  my_msg.mtype = 4;
  my_msg.mtext = docks_number;
  if (msgsnd(q_id, &my_msg, sizeof(double), 0) == -1)
  {
    perror("msgsnd x cord failed in send_cord\n");
    exit(1);
  }
  TEST_ERROR

  return;
}

/* Generate the quantity of merch that each port offer and request. */
int gen_quantity(int tot_merch)
{
  int quantity_out;

  srand(getpid());
  quantity_out = rand() % (tot_merch - 1 - 1 + 1) + 1;

  return quantity_out;
}

/* Generate the types of merch requested by each port */
int gen_merch_in(int n_of_merch_type_in, int *type_in, merce *merch, pid_t *pids)
{
  int i, j, m;
  int total_ton = 0;

  for (m = 0; m < parameters->SO_PORTI; m++)
  {
    if (getpid() == pids[m])
    {
      srand(pids[m]);

      for (i = 0; i < n_of_merch_type_in; i++)
      {
        type_in[i] = rand() % parameters->SO_MERCI;
        for (j = 0; j < i; j++)
        {
          if (type_in[i] == type_in[j])
          {
            i--;
            break;
          }
        }
      }

      /* Here it calculate the total quantity of tons of all the merches. It is important because sometimes
       * it don't correspond to the quantity of merch that need to be generated by the port. */
      for (i = 0; i < n_of_merch_type_in; i++)
      {
        for (j = 0; j < parameters->SO_MERCI; j++)
        {
          if (type_in[i] == j)
          {
            total_ton += merch[j].quantita;
          }
        }
      }

      break;
    }
  }
  return total_ton;
}

/* Generate the types of merch offered by each port */
int gen_merch_out(int n_of_merch_type_out, int *type_out, int n_of_merch_type_in, int *type_in, merce *merch, pid_t *pids)
{
  int i, j, m;
  int total_ton = 0;

  for (m = 0; m < parameters->SO_PORTI; m++)
  {

    if (getpid() == pids[m])
    {
      for (i = 0; i < n_of_merch_type_out; i++)
      {
        type_out[i] = rand() % (parameters->SO_MERCI - 1 - 0 + 1) + 0;

        /* Evita che ci siano le stesse merci sia in offerta che in richiesta */
        for (j = 0; j < n_of_merch_type_in; j++)
        {
          while (type_out[i] == type_in[j])
          {
            type_out[i] = rand() % (parameters->SO_MERCI - 1 - 0 + 1) + 0;
            j = 0;
          }
        }

        /* Avoid the type of merch is generated two times */
        for (j = 0; j < i; j++)
        {
          if (type_out[i] == type_out[j])
          {
            i--;
            break;
          }
        }
      }

      for (i = 0; i < n_of_merch_type_out; i++)
      {
        for (j = 0; j < parameters->SO_MERCI; j++)
        {
          if (type_out[i] == j)
          {
            total_ton += merch[j].quantita;
          }
        }
      }
      break;
    }
  }

  return total_ton;
}

/* Because the quantity of merch and the number of types are generated separatly,
 * this function mach helps to match the number of lots of a merch type to reach the
 * quantity requested by the port */
int fill_merch_in(int n_of_merch_type_in, int actual_quantity_in, int ton_merch_in, int *type_in, merce *merch)
{
  int i, *tot_lots, tot_quantity = 0;
  ;

  tot_lots = malloc(n_of_merch_type_in * sizeof(int));

  /* If the demand of merch is less than the requested one,
   * here it is filled by adding lots */
  for (i = 0; actual_quantity_in < ton_merch_in;)
  {
    merch[type_in[i]].num_lots += 1;
    actual_quantity_in += merch[type_in[i]].quantita;

    i++;
    if (i == n_of_merch_type_in)
    {
      i = 0;
    }
  }

  if (actual_quantity_in - ton_merch_in > ton_merch_in - tot_quantity)
  {
    actual_quantity_in = tot_quantity;
    for (i = 0; i < n_of_merch_type_in; i++)
    {
      merch[type_in[i]].num_lots = tot_lots[i];
    }
  }

  free(tot_lots);

  return actual_quantity_in;
}

/* Because the quantity of merch and the number of types are generated separatly,
 * this function mach helps to match the number of lots of a merch type to reach the
 * quantity offered by the port */
int fill_merch_out(int n_of_merch_type_out, int actual_quantity_out, int ton_merch_out, int *type_out, merce *merch)
{
  int i, *tot_lots, tot_quantity = 0;

  tot_lots = malloc(n_of_merch_type_out * sizeof(int));

  /* If the demand of merch is less than the requested one,
   * here it is filled by adding lots */
  for (i = 0; actual_quantity_out < ton_merch_out;)
  {
    tot_lots[i] = merch[type_out[i]].num_lots;
    tot_quantity += merch[type_out[i]].quantita;

    merch[type_out[i]].num_lots += 1;
    actual_quantity_out += merch[type_out[i]].quantita;

    i++;
    if (i == n_of_merch_type_out)
    {
      i = 0;
    }
  }

  if (actual_quantity_out - ton_merch_out > ton_merch_out - tot_quantity)
  {
    actual_quantity_out = tot_quantity;
    for (i = 0; i < n_of_merch_type_out; i++)
    {
      merch[type_out[i]].num_lots = tot_lots[i];
    }
  }

  free(tot_lots);

  return actual_quantity_out;
}

/* Create the semaphore that is needed to manage the docks of a port */
int create_docks(int port_number, int docks_number)
{
  int sem_id, key = SEM_KEY + port_number;
  /*
   * The key of the semaphore array of every port is a general key + the index of the port, so that each port
   * has is own semaphore arrary with an unique key. When a ship arrive at the port it know his index, so
   * that it can rebuild the key
   */
  sem_id = semget(key, 1, IPC_CREAT | 0600);
  TEST_ERROR
  if (sem_id == -1)
  {
    perror("semget");
    exit(1);
  }

  /* Set all the semaphores to 1, so that are all accesible to the firsts ships that come */
  semctl(sem_id, 0, SETVAL, docks_number);

  return sem_id;
}

/* Share the merch created for the offer */
int public_merch_in(merch_exchange *merch_in, int port_number, merce *merch, int *type_in, int n_of_merch_type_in)
{
  int i, shm_id;
  int soporti = parameters->SO_PORTI;

  shm_id = shmget(SHM_KEY_IN, sizeof(merch_exchange), IPC_CREAT | 0600);
  TEST_ERROR
  if (shm_id == -1)
  {
    perror("shmget");
    exit(EXIT_FAILURE);
  }

  merch_in = (merch_exchange *)shmat(shm_id, NULL, 0);
  TEST_ERROR
  if (merch_in == (void *)-1)
  {
    perror("shmat");
    exit(EXIT_FAILURE);
  }

  merch_in[port_number].n_of_merch = n_of_merch_type_in;
  for (i = 0; i < n_of_merch_type_in; i++)
  {
    merch_in[port_number * soporti + i].tipo = merch[type_in[i]].tipo;
    merch_in[port_number * soporti + i].num_lots = merch[type_in[i]].num_lots;
    merch_in[port_number * soporti + i].quantita = merch[type_in[i]].quantita;
    merch_in[port_number * soporti + i].vita = merch[type_in[i]].vita;
  }
  for (i = n_of_merch_type_in; i < parameters->SO_MERCI; i++)
  {
    merch_in[port_number * soporti + i].tipo = -1;
  }

  return shm_id;
}

/* Share the merch created for the request */
int public_merch_out(merch_exchange *merch_out, int port_number, merce *merch, int *type_out, int n_of_merch_type_out)
{
  int i, shm_id;
  int soporti = parameters->SO_PORTI;

  shm_id = shmget(SHM_KEY_OUT, sizeof(merch_exchange), IPC_CREAT | 0600);
  TEST_ERROR
  if (shm_id == -1)
  {
    perror("shmget");
    exit(EXIT_FAILURE);
  }

  merch_out = (merch_exchange *)shmat(shm_id, NULL, 0);
  TEST_ERROR
  if (merch_out == (void *)-1)
  {
    perror("shmat");
    exit(EXIT_FAILURE);
  }

  merch_out[port_number].n_of_merch = n_of_merch_type_out;

  for (i = 0; i < n_of_merch_type_out; i++)
  {
    merch_out[port_number * soporti + i].tipo = type_out[i];
    merch_out[port_number * soporti + i].num_lots = merch[type_out[i]].num_lots;
    merch_out[port_number * soporti + i].quantita = merch[type_out[i]].quantita;
    merch_out[port_number * soporti + i].vita = merch[type_out[i]].vita;
  }

  for (i = n_of_merch_type_out; i < parameters->SO_MERCI; i++)
  {
    merch_out[port_number * parameters->SO_MERCI + i].tipo = -1;
  }

  return shm_id;
}

/* After porti.c has finished, it exit the semaphore, so that navi.c can be launched */
int exit_sem()
{
  int sem_id;
  struct sembuf sops;

  sem_id = semget(SEM_MASTER, 1, 0600);
  TEST_ERROR
  if (sem_id == -1)
  {
    perror("semget");
    exit(1);
  }

  sops.sem_num = 0;
  sops.sem_op = 1;
  sops.sem_flg = 0;

  semop(sem_id, &sops, 1);

  return sem_id;
}
