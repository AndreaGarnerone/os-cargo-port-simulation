# Cargo Port Simulation

This repository contains an **Operating Systems project** that simulates the traffic of cargo ships transporting different types of goods through a network of ports.

The simulation is implemented using **multiple concurrent processes**, with explicit management of synchronization, inter-process communication, and simulated time.

---

## Project Description

The system simulates cargo transportation through ports using the following processes:

- **Master process**  
  Responsible for initializing the simulation, spawning other processes, and managing global coordination when required.

- **Ship processes (`SO_NAVI ≥ 1`)**  
  Each ship represents a cargo vessel that transports goods between ports.

- **Port processes (`SO_PORTI ≥ 4`)**  
  Each port manages docking, loading, and unloading operations for ships.

All processes run concurrently and interact according to the rules defined by the simulation.

---

## Simulated Time vs Real Time

The project distinguishes between:

- **Simulated time**  
  Represents the logical time of the simulation (e.g. one simulated day required to transport goods).

- **Real time**  
  Represents the actual execution time of the simulation on the system.

The mapping between the two is defined as follows:

> **1 simulated day = 1 second of real execution time**

For example, a simulation that represents 30 days will complete after approximately 30 seconds of real time.

---

## Operating Systems Concepts Involved

This project focuses on key OS concepts, including:

- Process creation and management
- Inter-process communication (IPC)
- Synchronization mechanisms
- Shared resources and coordination
- Time-based simulation
- Unix system calls

The design choices aim to reflect realistic constraints of concurrent systems.

---

## Technologies Used

- **C**
- **POSIX APIs**
- **Unix/Linux environment**
- Process-based concurrency

---

## Informations
For more informations read the appropriate document [Consegna progetto](Consegna_progetto.pdf).
