#include "commons.h"

params *parameters;

merch_exchange *merch_out;
merch_exchange *merch_in;

merch_port *merch_in_port;
merch_port *merch_expired_port;
merch_port *merch_delivered_port;
merch_ship *merch_in_ship;
merch_ship *merch_expired_ship;

dump *dump_status;

double n_x_cord, n_y_cord;

/*-------------------------------FUNCTIONS DECLARATION-------------------------------------------*/

int connect_parameters();

void connect_dump();

dump *attachSharedMemoryDump(dump *shared_memory);

merch_port *attachSharedMemoryPort(merch_port *shared_memory, int key);

merch_ship *attachSharedMemoryShip(merch_ship *shared_memory, int key);

int port_coordinates(double *port_x_cord, double *port_y_cord);

double take_x_cordinates(int queue_id, my_msgbuf *my_msg);

double take_y_cordinates(int queue_id, my_msgbuf *my_msg);

processes_id *give_pid(int ship_number, pid_t pid_ship, processes_id *ships_id);

void get_merch_requested();

int get_merch_sold();

void set_docks();

double nearest_port(int *port_number, double *port_x_cord, double *port_y_cord);

void move_to_port(double *port_x_cord, double *port_y_cord, double nearest_port, int *port_number);

void travel_manager(double *port_x_cord, double *port_y_cord, int *port_number, int ship_number, merce_in_stiva *ship_hold, int *finished_ports, pid_t *pids, processes_id *ships_id);

void dock_acces(int *port_number, int ship_number, merce_in_stiva *ship_hold, int *finished_ports, double *port_x_cord, double *port_y_cord, pid_t *pids, processes_id *ships_id);

int choose_merch_to_take(int *port_number, int ship_number, int *finished_ports, merce_in_stiva *ship_hold, pid_t *pids);

void remove_merch_out(int *port, merce_in_stiva *ship_hold);

void hold_dock(int *port_number, merce_in_stiva *ship_hold, int merch_to_take);

int choose_port(int *port_number, double *port_x_cord, double *port_y_cord, int ship_number, merce_in_stiva *ship_hold);

void remove_unused_merch(int merch_to_remove);

double calc_distance(double *port_x_cord, double *port_y_cord, int *destination_port);

void remove_merch_in(int port_to_act, merce_in_stiva *ship_hold, int ship_number);

void reset_merch(merce_in_stiva *ship_hold);

/*----------------------------------FUNCTIONS BODY----------------------------------------------*/

int main(int argc, char const *argv[])
{
  int ship_number, port_number = -1;
  int *finished_ports, status;
  double cord_nearest_port, msg_id;
  double *port_x_cord, *port_y_cord;

  pid_t *pids, child_pid;
  merce_in_stiva ship_hold;
  processes_id *ships_id;

  connect_parameters();

  port_x_cord = malloc(parameters->SO_PORTI * sizeof(double));
  port_y_cord = malloc(parameters->SO_PORTI * sizeof(double));
  finished_ports = malloc(parameters->SO_PORTI * sizeof(int));
  pids = malloc(parameters->SO_NAVI * sizeof(pid_t));

  /* There must be at least 1 ship */
  if (parameters->SO_NAVI < 1)
  {
    fprintf(stderr, "Il numero di navi (%d) deve essere >= 1\n", parameters->SO_PORTI);
    return 0;
  }

  connect_dump();

  msg_id = port_coordinates(port_x_cord, port_y_cord);

  /* Ships generation */
  for (ship_number = 0; ship_number < parameters->SO_NAVI; ship_number++)
  {
    switch (child_pid = fork())
    {
    case -1:
      perror("fork");

      exit(EXIT_FAILURE);
      break;

    case 0:
      pids[ship_number] = getpid();
      srand(pids[ship_number]);

      ships_id = give_pid(ship_number, pids[ship_number], ships_id);

      ship_hold.tipo = 0;
      ship_hold.quantita = 0;
      ship_hold.num_lots = 0;
      ship_hold.vita = 0;

      dump_status[ship_number].ship_status = 2;

      /* generate the conrdinates randomly */
      n_x_cord = (((double)rand()) / RAND_MAX) * parameters->SO_LATO;
      n_y_cord = (((double)rand()) / RAND_MAX) * parameters->SO_LATO;

      get_merch_requested();
      get_merch_sold(port_number);

      set_docks(ships_id);

      cord_nearest_port = nearest_port(&port_number, port_x_cord, port_y_cord);

      dump_status[ship_number].ship_status = 1;
      move_to_port(port_x_cord, port_y_cord, cord_nearest_port, &port_number);
      dump_status[ship_number].ship_status = 0;

      travel_manager(port_x_cord, port_y_cord, &port_number, ship_number, &ship_hold, finished_ports, pids, ships_id);

      exit(EXIT_SUCCESS);
      break;

    default:
      break;
    }
  }

  while ((child_pid = wait(&status)) != -1)
    ;

  msgctl(msg_id, IPC_RMID, NULL);

  free(port_x_cord);
  free(port_y_cord);
  free(finished_ports);
  free(pids);

  return 0;
}

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

dump *attachSharedMemoryDump(dump *shared_memory)
{

  /* Create a shared memory segment */
  int shmid = shmget(SHM_DUMP_D, sizeof(dump), 0600);

  /* Attach the shared memory segment */
  shared_memory = (dump *)shmat(shmid, NULL, 0);

  return shared_memory;
}

merch_port *attachSharedMemoryPort(merch_port *shared_memory, int key)
{

  int shared_k = SHM_DUMP_P + key;
  /* Create a shared memory segment */
  int shmid = shmget(shared_k, sizeof(merch_port), 0600);

  /* Attach the shared memory segment */
  shared_memory = (merch_port *)shmat(shmid, NULL, 0);

  return shared_memory;
}

merch_ship *attachSharedMemoryShip(merch_ship *shared_memory, int key)
{

  int shared_k = SHM_DUMP_S + key;
  /* Create a shared memory segment */
  int shmid = shmget(shared_k, sizeof(merch_ship), 0600);

  /* Attach the shared memory segment */
  shared_memory = (merch_ship *)shmat(shmid, NULL, 0);

  return shared_memory;
}

/* Take, from message queue, the cordinates of the ports */
int port_coordinates(double *port_x_cord, double *port_y_cord)
{
  int i, queue_id;
  my_msgbuf my_msg;

  /* The cordinates of the first four ports are already known, so there's no need to take them from the ports */
  port_x_cord[0] = port_x_cord[2] = 0;
  port_x_cord[1] = port_x_cord[3] = parameters->SO_LATO;

  port_y_cord[0] = port_y_cord[1] = 0;
  port_y_cord[2] = port_y_cord[3] = parameters->SO_LATO;

  if (parameters->SO_PORTI > 4)
  {
    queue_id = msgget(MSG_KEY, IPC_CREAT | 0600);
    TEST_ERROR

    for (i = 4; i < parameters->SO_PORTI; i++)
    {
      port_x_cord[i] = take_x_cordinates(queue_id, &my_msg);
    }

    for (i = 4; i < parameters->SO_PORTI; i++)
    {
      port_y_cord[i] = take_y_cordinates(queue_id, &my_msg);
    }
  }

  return queue_id;
}

/* Read the message */
double take_x_cordinates(int queue_id, my_msgbuf *my_msg)
{
  int msg_type = 2;
  double port_cord;

  msgrcv(queue_id, my_msg, sizeof(double), msg_type, 0);
  TEST_ERROR
  port_cord = my_msg->mtext;

  return port_cord;
}

/* Read the message */
double take_y_cordinates(int queue_id, my_msgbuf *my_msg)
{
  int msg_type = 3;
  double port_cord;

  msgrcv(queue_id, my_msg, sizeof(double), msg_type, 0);
  TEST_ERROR
  port_cord = my_msg->mtext;

  return port_cord;
}

/* Share via shared memory the pids of the ships processes to the master,
 * so that it can use these to handle the stop of the processes */
processes_id *give_pid(int ship_number, pid_t pid_ship, processes_id *ships_id)
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

  /* Attach the struct to the shared memory segment */
  ships_id = (processes_id *)shmat(shm_id, NULL, 0);
  TEST_ERROR
  if (ships_id == (void *)-1)
  {
    perror("Error attaching shared memory");
    exit(EXIT_FAILURE);
  }

  ships_id[ship_number].ship_pid = pid_ship;
  ships_id->finished = 0;

  return ships_id;
}

/* Get the merches that the ports request so that the ships can decide where to go */
void get_merch_requested()
{
  int shm_id;

  shm_id = shmget(SHM_KEY_IN, sizeof(merch_exchange), 0600);
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

  return;
}

/* Get the merches that the ports offer so that the ships can decide where to go */
int get_merch_sold()
{
  int shm_id, port, i;
  int soporti = parameters->SO_PORTI;

  shm_id = shmget(SHM_KEY_OUT, sizeof(merch_exchange), IPC_CREAT | 0600);

  TEST_ERROR
  if (shm_id == -1)
  {
    perror("shmget");
    exit(EXIT_FAILURE);
  }

  /* Attach to the shared memory segment */
  merch_out = (merch_exchange *)shmat(shm_id, NULL, 0);
  TEST_ERROR
  if (merch_out == (void *)-1)
  {
    perror("shmat");
    exit(EXIT_FAILURE);
  }

  for (port = 0; port < parameters->SO_PORTI; port++)
  {
    merch_in_port[port].index = merch_out[port].n_of_merch;

    dump_status[port].merch_in_port = 0;

    for (i = 0; i < merch_out[port].n_of_merch; i++)
    {
      merch_in_port[port * soporti + i].merch_type = merch_out[port * soporti + i].tipo;
      merch_in_port[port * soporti + i].merch_quantity = merch_out[port * soporti + i].quantita * merch_out[port * soporti + i].num_lots;

      dump_status[port].merch_in_port += merch_in_port[port * soporti + i].merch_quantity;
    }
  }

  return shm_id;
}

/* Get the semaphore (dock) id's */
void set_docks(processes_id *ships_id)
{
  int sem_id, i, key;

  for (i = 0; i < parameters->SO_PORTI; i++)
  {
    key = SEM_KEY + i;

    sem_id = semget(key, 1, 0600);
    TEST_ERROR

    ships_id[i].sem_id = sem_id;
  }

  return;
}

/* Find the nearest port to the actual position of the ship. Return the total distance from the ship to the port */
double nearest_port(int *port_number, double *port_x_cord, double *port_y_cord)
{
  int i, port = *port_number;
  double distance_x, distance_y, dis, *final_dis;
  double nearest_port_distance;

  final_dis = (double *)malloc(parameters->SO_PORTI * sizeof(*final_dis));

  /* La distanza di una nave da un porto non potra' mai essere piu' grande della diagonale della mappa stessa*/
  nearest_port_distance = parameters->SO_LATO * 50;

  /* Calcolo delle distanze finali (in linea retta) */
  for (i = 0; i < parameters->SO_PORTI; i++)
  {
    if (i != port)
    {
      distance_x = fabs(port_x_cord[i] - n_x_cord);
      distance_y = fabs(port_y_cord[i] - n_y_cord);
      dis = ((distance_x * distance_x) + (distance_y * distance_y));

      final_dis[i] = sqrt(dis);

      if (final_dis[i] < nearest_port_distance)
      {
        nearest_port_distance = final_dis[i];
        *port_number = i;
      }
    }
  }

  free(final_dis);
  return nearest_port_distance;
}

/* Move the ship from its actual position to the desired position using the nanosleep */
void move_to_port(double *port_x_cord, double *port_y_cord, double distance, int *port_number)
{
  int int_part, nanosec;
  double cord_port, fract_part;
  struct timespec travel_time;

  cord_port = distance / parameters->SO_SPEED;

  int_part = (int)cord_port;
  fract_part = cord_port - int_part;

  nanosec = fract_part * 1e9;

  travel_time.tv_sec = int_part;
  travel_time.tv_nsec = nanosec;
  nanosleep(&travel_time, NULL);

  /* Actual moving to the cordinates of the ports */
  n_x_cord = port_x_cord[*port_number];
  n_y_cord = port_y_cord[*port_number];

  return;
}

/* Wrapper of the function dock_access */
void travel_manager(double *port_x_cord, double *port_y_cord, int *port_number, int ship_number, merce_in_stiva *ship_hold, int *finished_ports, pid_t *pids, processes_id *ships_id)
{
  dock_acces(port_number, ship_number, ship_hold, finished_ports, port_x_cord, port_y_cord, pids, ships_id);

  return;
}

/* Main function that manage the operation carried out by the ships*/
void dock_acces(int *port_number, int ship_number, merce_in_stiva *ship_hold, int *finished_ports, double *port_x_cord, double *port_y_cord, pid_t *pids, processes_id *ships_id)
{
  int i, sem_id, merch_to_take, sem_result;
  int actual_port = *port_number;
  int chosen_port, cord_destination_port;
  struct sembuf sops;

  sem_id = ships_id[*port_number].sem_id;

  sops.sem_num = 0;
  sops.sem_op = -1;
  sops.sem_flg = 0;
  do
  {
    sem_result = semop(sem_id, &sops, 1);
  } while (sem_result == -1 && errno == EINTR);

  dump_status[ship_number].ship_status = 0;

  dump_status[actual_port].docks_occupied += 1;

  merch_to_take = choose_merch_to_take(port_number, ship_number, finished_ports, ship_hold, pids);

  if (merch_to_take != -1)
  {
    remove_merch_out(port_number, ship_hold);

    hold_dock(port_number, ship_hold, merch_to_take);

    chosen_port = choose_port(port_number, port_x_cord, port_y_cord, ship_number, ship_hold);

    sops.sem_op = 1;
    do
    {
      sem_result = semop(sem_id, &sops, 1);
    } while (sem_result == -1 && errno == EINTR);
    dump_status[actual_port].docks_occupied -= 1;
  }
  else
  { /* If the chosen port doesn't have anymore merch */
    sops.sem_op = 1;
    do
    {
      sem_result = semop(sem_id, &sops, 1);
    } while (sem_result == -1 && errno == EINTR);
    dump_status[actual_port].docks_occupied -= 1;

    cord_destination_port = nearest_port(port_number, port_x_cord, port_y_cord);

    if (finished_ports[*port_number] != -1)
    {

      dump_status[ship_number].ship_status = 2;
      move_to_port(port_x_cord, port_y_cord, cord_destination_port, port_number);
      dump_status[ship_number].ship_status = 0;

      chosen_port = *port_number;
    }
    else
    {
      for (i = 0; i < parameters->SO_PORTI; i++)
      {
        if (i != actual_port && finished_ports[i] != -1)
        {
          cord_destination_port = calc_distance(port_x_cord, port_y_cord, &i);

          dump_status[ship_number].ship_status = 2;
          move_to_port(port_x_cord, port_y_cord, cord_destination_port, &i);
          dump_status[ship_number].ship_status = 0;

          chosen_port = i;
          break;
        }
      }
    }
  }

  travel_manager(port_x_cord, port_y_cord, &chosen_port, ship_number, ship_hold, finished_ports, pids, ships_id);

  return;
}

/* Decide the merch to take from the port */
int choose_merch_to_take(int *port_number, int ship_number, int *finished_ports, merce_in_stiva *ship_hold, pid_t *pids)
{
  int i, big_merch_index = 0, candidate, finished_merch = 0, finished_merch_in = 0, index;
  int soporti = parameters->SO_PORTI, sonavi = parameters->SO_NAVI;
  int big_merch_quantity = merch_out[*port_number * soporti].quantita * merch_out[*port_number * soporti].num_lots;
  int somerci = parameters->SO_MERCI;

  for (i = 0; i < merch_out[*port_number].n_of_merch; i++)
  {
    if (merch_out[*port_number * soporti + i].tipo != -1)
    {
      big_merch_index = i;
    }
    else
      finished_merch++;
  }
  for (i = 0; i < merch_in[*port_number].n_of_merch; i++)
  {
    if (merch_in[*port_number * soporti + i].tipo == -1)
    {
      finished_merch_in++;
    }
  }

  if (finished_merch == merch_out[*port_number].n_of_merch || finished_merch_in == merch_in[*port_number].n_of_merch)
  {
    finished_ports[*port_number] = -1;
    big_merch_index = -1;
  }
  else
  {
    /* Find the merch with the more quantity */
    for (i = 1; i < parameters->SO_MERCI; i++)
    {
      candidate = merch_out[*port_number * somerci + i].quantita * merch_out[*port_number * somerci + i].num_lots;
      if (merch_out[*port_number * somerci + i].tipo != -1 && candidate > big_merch_quantity)
      {
        big_merch_quantity = candidate;
        big_merch_index = i;
      }
    }

    ship_hold->tipo = merch_out[*port_number * soporti + big_merch_index].tipo;
    ship_hold->quantita = merch_out[*port_number * soporti + big_merch_index].quantita;
    ship_hold->num_lots = merch_out[*port_number * soporti + big_merch_index].num_lots;
    ship_hold->vita = merch_out[*port_number * soporti + big_merch_index].vita;

    merch_in_ship[ship_number].merch_type = ship_hold->tipo;
    merch_in_ship[ship_number].merch_quantity = ship_hold->quantita * ship_hold->num_lots;

    index = merch_in_ship[ship_number].index;
    merch_in_ship[ship_number * sonavi + index].final_merch_type = ship_hold->tipo;
    merch_in_ship[ship_number * sonavi + index].final_merch_quantity = ship_hold->quantita;
    merch_in_ship[ship_number].index++;

    merch_in_port[*port_number].total_quantity += ship_hold->quantita * ship_hold->num_lots;
  }

  return big_merch_index;
}

/* After the ship has taken the merch, the merch is removed from the port */
void remove_merch_out(int *port, merce_in_stiva *ship_hold)
{
  int i, merch_to_remove = ship_hold->tipo;
  int soporti = parameters->SO_PORTI;

  for (i = 0; i < merch_out[*port].n_of_merch; i++)
  {
    if (merch_out[*port * soporti + i].tipo == merch_to_remove)
    {

      merch_out[*port * soporti + i].tipo = -1;
      merch_out[*port * soporti + i].quantita = -1;

      dump_status[*port].merch_in_port -= merch_out[*port * soporti + i].quantita * merch_out[*port * soporti + i].num_lots;
      dump_status[*port].merch_delivered += merch_in_port[*port * soporti + i].merch_quantity;

      merch_in_port[*port * soporti + i].merch_type = -1;
      merch_in_port[*port * soporti + i].merch_quantity = -1;

      break;
    }
  }

  return;
}

/* Occupy the dock for the time needed to do the operation of loading and unloading */
void hold_dock(int *port_number, merce_in_stiva *ship_hold, int merch_to_take)
{
  int int_part, operation_control, soporti = parameters->SO_PORTI;
  double total_time_out, total_time_in, total_time, fract_part;
  struct timespec time_occupied;

  total_time_in = ship_hold->quantita * ship_hold->num_lots;

  /* Once at the port , it choose the merch to take.
   * (done here because it is needed for the time occupation of the dock)
   */
  total_time_out = merch_out[*port_number * soporti + merch_to_take].quantita * merch_out[*port_number * soporti + merch_to_take].num_lots;

  total_time = (double)((total_time_out + total_time_in) / parameters->SO_LOADSPEED);
  int_part = (int)total_time;

  fract_part = total_time - int_part;
  fract_part *= 1e9;

  time_occupied.tv_sec = int_part;
  time_occupied.tv_nsec = fract_part;

  do
  {
    operation_control = nanosleep(&time_occupied, NULL);
  } while (operation_control == -1 && errno == EINTR);

  return;
}

/* Choose the port to go to */
int choose_port(int *port_number, double *port_x_cord, double *port_y_cord, int ship_number, merce_in_stiva *ship_hold)
{
  int i, j = 0, found = 0, actual_port = *port_number, merch_to_take = ship_hold->tipo;
  int *candidate_port_index, *candidate_port_quantity, requested_quantity;
  int port, chosen_port_index, min_difference, difference, chosen_port;
  double cord_destination_port;
  int soporti = parameters->SO_PORTI;

  candidate_port_index = malloc(parameters->SO_PORTI * sizeof(parameters->SO_PORTI));
  candidate_port_quantity = malloc(parameters->SO_PORTI * sizeof(parameters->SO_PORTI));

  requested_quantity = ship_hold->quantita * ship_hold->num_lots;

  /* Check if in the other ports there is the merch that I need to deliver. If there is, it mark the port,
   * and check the other ports. At the end it check the quantity of the ports marked, and coose the better port
   */
  for (port = 0; port < parameters->SO_PORTI; port++)
  {
    if (port != actual_port)
    {
      for (i = 0; i < parameters->SO_MERCI; i++)
      {
        if (merch_in[port * soporti + i].tipo != -1)
        {
          if (merch_in[port * soporti + i].tipo == merch_to_take)
          {
            candidate_port_index[j] = port;
            candidate_port_quantity[j] = merch_in[port * soporti + i].num_lots * merch_in[port * soporti + i].quantita;

            while (candidate_port_quantity[j] >= parameters->SO_CAPACITY)
            {
              candidate_port_quantity[j] -= merch_in[port * soporti + i].num_lots;
            }

            j++;
            found++;
            break;
          }
        }
      }
      if (found == -1)
        break;
    }
  }

  switch (found)
  {
  case 0:

    /* If there is a merch that no port require, it is discarted from every port, so that it is no longer
     * checked, and other ports don't waste time cheching for it */
    remove_unused_merch(merch_to_take);

    chosen_port = actual_port;
    break;

  case 1:
    /* Found a port that require the exact quantity. It goes to it */

    cord_destination_port = calc_distance(port_x_cord, port_y_cord, &candidate_port_index[0]);

    dump_status[ship_number].ship_status = 1;
    move_to_port(port_x_cord, port_y_cord, cord_destination_port, &candidate_port_index[0]);
    dump_status[ship_number].ship_status = 0;

    remove_merch_in(candidate_port_index[0], ship_hold, ship_number);

    chosen_port = candidate_port_index[0];
    break;

  default:
    /* If there are more ports that require the same merch, the ship goes to the one
     * that require the quantity most similar to the one taken by the ship */
    chosen_port_index = candidate_port_index[0];
    min_difference = abs(requested_quantity - candidate_port_quantity[0]);

    for (i = 1; i < j; i++)
    { /* Found the port that require the most similar quantity to the offer */
      difference = abs(requested_quantity - candidate_port_quantity[i]);
      if (difference < min_difference)
      {
        min_difference = difference;
        chosen_port_index = candidate_port_index[i];
      }
    }

    cord_destination_port = calc_distance(port_x_cord, port_y_cord, &chosen_port_index);

    dump_status[ship_number].ship_status = 1;
    move_to_port(port_x_cord, port_y_cord, cord_destination_port, &chosen_port_index);
    dump_status[ship_number].ship_status = 0;

    remove_merch_in(chosen_port_index, ship_hold, ship_number);

    chosen_port = chosen_port_index;
    break;
  }

  free(candidate_port_index);
  free(candidate_port_quantity);

  return chosen_port;
}

/* If a merch is not wanted by any port, it is removed from all ports */
void remove_unused_merch(int merch_to_remove)
{
  int i, port, index;
  int soporti = parameters->SO_PORTI;

  for (port = 0; port < parameters->SO_PORTI; port++)
  {
    for (i = 0; i < merch_out[port].n_of_merch; i++)
    {
      if (merch_in_port[port * soporti + i].merch_type == merch_to_remove)
      {
        merch_out[port * soporti + i].tipo = -1;

        dump_status[port].merch_in_port -= merch_out[port * soporti + i].quantita * merch_out[port * soporti + i].num_lots;

        merch_in_port[port * soporti + i].merch_type = -1;
        merch_in_port[port * soporti + i].merch_quantity = -1;

        index = merch_expired_port[port].index;
        merch_expired_port[port * soporti + index].merch_type = merch_to_remove;
        merch_expired_port[port * soporti + index].merch_quantity = merch_out[port * soporti + i].quantita * merch_out[port * soporti + i].num_lots;
        merch_expired_port[port].index++;

        break;
      }
    }
  }

  return;
}

/* Calculate the distance between the ship and its destination  */
double calc_distance(double *port_x_cord, double *port_y_cord, int *destination_port)
{
  double distance_x, distance_y, dis, final_dis;

  distance_x = fabs(port_x_cord[*destination_port] - n_x_cord);
  distance_y = fabs(port_y_cord[*destination_port] - n_y_cord);
  dis = ((distance_x * distance_x) + (distance_y * distance_y));

  final_dis = sqrt(dis);

  return final_dis;
}

/* After a ship is arrived at the port and has unloaded the cargo, the request of a merch is removed */
void remove_merch_in(int port_to_act, merce_in_stiva *ship_hold, int ship_number)
{
  int i, merch_to_remove = ship_hold->tipo, s = 100, index;
  int quantity_removed, soporti = parameters->SO_PORTI;

  for (i = 0; i < merch_in[port_to_act].n_of_merch; i++)
  {
    if (merch_to_remove == merch_in[soporti * port_to_act + i].tipo)
    {
      quantity_removed = (ship_hold->quantita * ship_hold->num_lots) - merch_in[soporti * port_to_act + i].quantita;

      if (quantity_removed <= 0)
      { /* The quantity removed fill the request, so the merch in the port is entirely removed */

        index = merch_delivered_port[port_to_act].index;
        merch_delivered_port[port_to_act * soporti + index].merch_type = merch_to_remove;
        merch_delivered_port[port_to_act * soporti + index].merch_quantity = merch_in[port_to_act * soporti + i].quantita;
        merch_delivered_port[port_to_act].total_quantity += merch_in[port_to_act * soporti + i].quantita;
        merch_delivered_port[port_to_act].index++;

        merch_in_port[port_to_act * soporti + i].merch_quantity += merch_in[port_to_act * soporti + i].quantita;
        dump_status[port_to_act].merch_recived += merch_in[port_to_act * soporti + i].quantita * merch_in[port_to_act * soporti + i].num_lots;
        dump_status[port_to_act].merch_in_port += merch_in[port_to_act * soporti + i].quantita * merch_in[port_to_act * soporti + i].num_lots;

        merch_in[port_to_act * soporti + i].tipo = -1;
        merch_in[port_to_act * soporti + i].quantita = -1;

        reset_merch(ship_hold);
      }
      else
      {
        index = merch_delivered_port[port_to_act].index;
        merch_delivered_port[soporti * port_to_act + index].merch_type = merch_to_remove;
        merch_delivered_port[soporti * port_to_act + index].merch_quantity = ship_hold->quantita * ship_hold->num_lots;
        merch_delivered_port[port_to_act].total_quantity += ship_hold->quantita * ship_hold->num_lots;
        merch_delivered_port[port_to_act].index++;

        merch_in_port[port_to_act * soporti + i].merch_quantity += ship_hold->quantita * ship_hold->num_lots;
        dump_status[port_to_act].merch_recived += ship_hold->quantita * ship_hold->num_lots;
        dump_status[port_to_act].merch_in_port += ship_hold->quantita * ship_hold->num_lots;

        quantity_removed = abs(quantity_removed);
      }

      s = merch_in[soporti * port_to_act + i].tipo;
      merch_in_ship[ship_number].merch_type = -1;
      merch_in_ship[ship_number].merch_quantity = -1;
      break;
    }
  }

  if (s == 100)
  {
    dump_status[ship_number].ship_status = 2;
  }

  return;
}

/* Reset the cargo of the ship */
void reset_merch(merce_in_stiva *ship_hold)
{
  ship_hold->tipo = 0;
  ship_hold->quantita = 0;
  ship_hold->num_lots = 0;
  ship_hold->vita = 0;

  return;
}
