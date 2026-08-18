/**
 * @file dentist.c
 * @brief Dentist Clinic Simulation - Operating System Project
 *
 * @author shanjeda alam 
 * @version 1.0
 * @date 2026
 *
 * @details
 * This program simulates a dentist clinic using different
 * Operating System concepts.
 *
 * Main OS Concepts:
 * 1. POSIX Threads
 * 2. Mutex
 * 3. Semaphores
 * 4. FIFO / FCFS Scheduling
 * 5. Critical Section
 * 6. Synchronization
 * 7. Structure
 * 8. Pointer
 * 9. Dynamic Memory Allocation
 *
 * Main Features:
 * - Patient management
 * - Appointment management
 * - Waiting room management
 * - Doctor treatment management
 * - No-show patient handling
 * - FIFO / FCFS scheduling
 * - Thread synchronization
 * - Mutex protected critical sections
 * - Semaphore based communication
 *
 * User Input:
 * - Number of patients
 * - Number of waiting room chairs
 * - Doctor capacity
 * - Number of appointment slots
 * - Patient names
 * - Patient arrival / no-show status
 *
 * Special Rule:
 * If a confirmed appointment patient becomes a no-show,
 * the appointment slot is released and another waiting
 * patient can receive treatment.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <semaphore.h>
#include <unistd.h>


/**
 * @struct patient
 * @brief Stores all information about a patient.
 *
 * This structure stores appointment information, waiting room
 * information, treatment status and private semaphores.
 */
struct patient
{
    int id;                     /**< Patient ID */
    char name[64];              /**< Patient name */

    int appointment;            /**< 1 = confirmed, 0 = not confirmed */
    int willArrive;             /**< 1 = will arrive, 0 = no-show */

    int arrivalOrder;           /**< FIFO / FCFS arrival order */
    int waiting;                /**< 1 = waiting, 0 = not waiting */

    int treated;                /**< 1 = treated */
    int noShow;                 /**< 1 = no-show */
    int leftNoChair;            /**< 1 = left because room was full */
    int leftDoctorFull;         /**< 1 = left because doctor was full */

    int requestPending;          /**< 1 = appointment request pending */

    sem_t replySem;              /**< Receptionist reply semaphore */
    sem_t dentistCallSem;        /**< Dentist call semaphore */
    sem_t treatmentDoneSem;      /**< Treatment completion semaphore */
};


/* ============================================================
   GLOBAL VARIABLES
   ============================================================ */

/** Total number of patients. */
int totalPatients;

/** Number of chairs in the waiting room. */
int waitingRoomChairs;

/** Maximum number of patients the doctor can treat. */
int doctorCapacity;

/** Number of available appointment slots. */
int appointmentSlots;

/** Current number of patients in waiting room. */
int waitingPatients = 0;

/** Number of patients already treated. */
int treatedPatients = 0;

/** Total number of confirmed appointments. */
int confirmedAppointments = 0;

/** Currently active appointment slots. */
int activeAppointments = 0;

/** Counter used for FIFO / FCFS order. */
int arrivalCounter = 0;

/** Indicates whether the clinic is closed. */
int clinicClosed = 0;


/**
 * @brief Pointer to dynamically allocated patient array.
 */
struct patient *patients;


/**
 * @brief Pointer to dynamically allocated patient thread array.
 */
pthread_t *patientThreads;


/* ============================================================
   MUTEX AND SEMAPHORES
   ============================================================ */

/**
 * @brief Mutex used to protect shared data.
 *
 * This mutex protects critical sections where multiple
 * threads access shared variables.
 */
pthread_mutex_t mutex;


/**
 * @brief Semaphore used to notify the receptionist
 * when a patient requests an appointment.
 */
sem_t requestReady;


/**
 * @brief Semaphore used to notify the dentist
 * when a patient enters the waiting room.
 */
sem_t patientReady;


/* ============================================================
   FUNCTION DECLARATIONS
   ============================================================ */

/**
 * @brief Initializes a patient structure.
 *
 * @param i Index of the patient.
 */
void initializePatient(int i);


/**
 * @brief Destroys all semaphores of a patient.
 *
 * @param i Index of the patient.
 */
void destroyPatient(int i);


/**
 * @brief Finds the next patient using FIFO / FCFS.
 *
 * @return Patient index if available, otherwise -1.
 */
int findNextPatient();


/**
 * @brief Finds the next appointment request.
 *
 * @return Patient index if request exists, otherwise -1.
 */
int findNextRequest();


/**
 * @brief Receptionist thread function.
 *
 * Handles appointment requests from patients.
 *
 * @param arg Thread argument.
 * @return NULL when the thread finishes.
 */
void *receptionist(void *arg);


/**
 * @brief Dentist thread function.
 *
 * Selects patients using FIFO / FCFS and treats them.
 *
 * @param arg Thread argument.
 * @return NULL when the thread finishes.
 */
void *dentist(void *arg);


/**
 * @brief Patient thread function.
 *
 * Handles appointment request, arrival, waiting room,
 * no-show and treatment.
 *
 * @param arg Pointer to a patient structure.
 * @return NULL when the thread finishes.
 */
void *patient(void *arg);


/**
 * @brief Displays the final patient table.
 */
void printPatientTable();


/**
 * @brief Displays final clinic statistics.
 */
void printStatistics();


/**
 * @brief Displays the contents of project_info.txt.
 */
void showProjectInfo();


/* ============================================================
   INITIALIZE PATIENT
   ============================================================ */

/**
 * @brief Initializes one patient.
 *
 * All patient status values are set to their default values.
 * Private semaphores are also initialized here.
 *
 * @param i Index of the patient in the patient array.
 */
void initializePatient(int i)
{
    patients[i].id = i + 1;

    patients[i].appointment = 0;
    patients[i].willArrive = 1;

    patients[i].arrivalOrder = 0;
    patients[i].waiting = 0;

    patients[i].treated = 0;
    patients[i].noShow = 0;
    patients[i].leftNoChair = 0;
    patients[i].leftDoctorFull = 0;

    patients[i].requestPending = 0;

    sem_init(&patients[i].replySem, 0, 0);
    sem_init(&patients[i].dentistCallSem, 0, 0);
    sem_init(&patients[i].treatmentDoneSem, 0, 0);
}


/* ============================================================
   DESTROY PATIENT
   ============================================================ */

/**
 * @brief Destroys patient semaphores.
 *
 * @param i Index of the patient.
 */
void destroyPatient(int i)
{
    sem_destroy(&patients[i].replySem);
    sem_destroy(&patients[i].dentistCallSem);
    sem_destroy(&patients[i].treatmentDoneSem);
}


/* ============================================================
   FIND NEXT PATIENT
   ============================================================ */

/**
 * @brief Finds the next waiting patient using FIFO / FCFS.
 *
 * The patient with the smallest arrival order is selected.
 *
 * @return Patient index or -1 if no patient is waiting.
 */
int findNextPatient()
{
    int selected = -1;
    int smallestOrder = 999999;

    for (int i = 0; i < totalPatients; i++)
    {
        if (patients[i].waiting == 1 &&
            patients[i].arrivalOrder < smallestOrder)
        {
            smallestOrder = patients[i].arrivalOrder;
            selected = i;
        }
    }

    return selected;
}


/* ============================================================
   FIND NEXT REQUEST
   ============================================================ */

/**
 * @brief Finds the next patient requesting an appointment.
 *
 * @return Patient index or -1 if no request exists.
 */
int findNextRequest()
{
    for (int i = 0; i < totalPatients; i++)
    {
        if (patients[i].requestPending == 1)
        {
            return i;
        }
    }

    return -1;
}


/* ============================================================
   SHOW PROJECT INFO
   ============================================================ */

/**
 * @brief Reads and displays project_info.txt.
 *
 * This function connects the external text documentation
 * file with the C program.
 */
void showProjectInfo()
{
    FILE *file;

    file = fopen("project_info.txt", "r");

    if (file == NULL)
    {
        printf("\nproject_info.txt file not found.\n");
        return;
    }

    char line[200];

    printf("\n============================================================\n");
    printf("                    PROJECT INFORMATION\n");
    printf("============================================================\n");

    while (fgets(line, sizeof(line), file))
    {
        printf("%s", line);
    }

    fclose(file);

    printf("\n============================================================\n");
}


/* ============================================================
   RECEPTIONIST THREAD
   ============================================================ */

/**
 * @brief Receptionist thread.
 *
 * The receptionist waits for appointment requests and assigns
 * available appointment slots.
 *
 * @param arg Thread argument.
 * @return NULL when receptionist exits.
 */
void *receptionist(void *arg)
{
    (void)arg;

    printf("\n[Receptionist] Online.\n");

    while (1)
    {
        /* Wait for appointment request */
        sem_wait(&requestReady);

        pthread_mutex_lock(&mutex);

        /* Check clinic closing */
        if (clinicClosed == 1)
        {
            pthread_mutex_unlock(&mutex);
            break;
        }

        /* Find request */
        int id = findNextRequest();

        if (id == -1)
        {
            pthread_mutex_unlock(&mutex);
            continue;
        }

        patients[id].requestPending = 0;

        /* Check appointment availability */
        if (activeAppointments < appointmentSlots)
        {
            patients[id].appointment = 1;

            activeAppointments++;
            confirmedAppointments++;

            printf("[Receptionist] %s (ID %d) - "
                   "Appointment CONFIRMED [%d/%d]\n",
                   patients[id].name,
                   patients[id].id,
                   activeAppointments,
                   appointmentSlots);
        }
        else
        {
            patients[id].appointment = 0;

            printf("[Receptionist] %s (ID %d) - "
                   "No appointment slot. Walk-in allowed.\n",
                   patients[id].name,
                   patients[id].id);
        }

        pthread_mutex_unlock(&mutex);

        /* Reply to exact patient */
        sem_post(&patients[id].replySem);
    }

    printf("[Receptionist] Closing.\n");

    return NULL;
}


/* ============================================================
   DENTIST THREAD
   ============================================================ */

/**
 * @brief Dentist thread.
 *
 * The dentist selects waiting patients according to FIFO/FCFS.
 * Each patient receives treatment for a simulated period.
 *
 * @param arg Thread argument.
 * @return NULL when dentist exits.
 */
void *dentist(void *arg)
{
    (void)arg;

    printf("\n[Dr. Smith] Clinic is open.\n");

    while (1)
    {
        /* Wait for patient */
        sem_wait(&patientReady);

        pthread_mutex_lock(&mutex);

        /* Close clinic when no patient remains */
        if (clinicClosed == 1 &&
            waitingPatients == 0)
        {
            pthread_mutex_unlock(&mutex);
            break;
        }

        /* Doctor has treatment capacity */
        if (waitingPatients > 0 &&
            treatedPatients < doctorCapacity)
        {
            int id = findNextPatient();

            if (id != -1)
            {
                patients[id].waiting = 0;
                waitingPatients--;

                printf("\n[Dr. Smith] Calling %s (ID %d)\n",
                       patients[id].name,
                       patients[id].id);

                printf("[Dr. Smith] FCFS Order: #%d\n",
                       patients[id].arrivalOrder);

                printf("[Dr. Smith] Waiting room: %d/%d\n",
                       waitingPatients,
                       waitingRoomChairs);

                pthread_mutex_unlock(&mutex);

                /* Call exact patient */
                sem_post(&patients[id].dentistCallSem);

                printf("[Dr. Smith] Treating %s...\n",
                       patients[id].name);

                /* Treatment simulation */
                sleep(2);

                printf("[Dr. Smith] Treatment finished for %s.\n",
                       patients[id].name);

                pthread_mutex_lock(&mutex);

                treatedPatients++;
                patients[id].treated = 1;

                /* Release appointment slot */
                if (patients[id].appointment == 1 &&
                    activeAppointments > 0)
                {
                    activeAppointments--;
                }

                pthread_mutex_unlock(&mutex);

                /* Notify patient */
                sem_post(&patients[id].treatmentDoneSem);
            }
            else
            {
                pthread_mutex_unlock(&mutex);
            }
        }

        /* Doctor capacity reached */
        else if (waitingPatients > 0 &&
                 treatedPatients >= doctorCapacity)
        {
            printf("\n[Dr. Smith] Daily treatment limit reached.\n");
            printf("[Dr. Smith] Remaining patients must leave.\n");

            for (int i = 0; i < totalPatients; i++)
            {
                if (patients[i].waiting == 1)
                {
                    patients[i].waiting = 0;
                    patients[i].leftDoctorFull = 1;

                    waitingPatients--;

                    printf("[Dr. Smith] %s (ID %d) - "
                           "Left because doctor is full.\n",
                           patients[i].name,
                           patients[i].id);

                    sem_post(&patients[i].dentistCallSem);
                }
            }

            pthread_mutex_unlock(&mutex);
        }
        else
        {
            pthread_mutex_unlock(&mutex);
        }
    }

    printf("\n[Dr. Smith] Closing.\n");

    return NULL;
}


/* ============================================================
   PATIENT THREAD
   ============================================================ */

/**
 * @brief Patient thread.
 *
 * Handles:
 * - Appointment request
 * - Appointment confirmation
 * - No-show
 * - Waiting room entry
 * - FIFO order
 * - Treatment
 *
 * @param arg Pointer to struct patient.
 * @return NULL when patient thread finishes.
 */
void *patient(void *arg)
{
    /*
     * Pointer to patient structure.
     */
    struct patient *p = (struct patient *)arg;


    /* --------------------------------------------------------
       REQUEST APPOINTMENT
       -------------------------------------------------------- */

    printf("\n[Patient %d | %s] Requesting appointment.\n",
           p->id,
           p->name);

    pthread_mutex_lock(&mutex);

    p->requestPending = 1;

    pthread_mutex_unlock(&mutex);

    /* Notify receptionist */
    sem_post(&requestReady);

    /* Wait for receptionist */
    sem_wait(&p->replySem);

    printf("[Patient %d | %s] Appointment: ",
           p->id,
           p->name);

    if (p->appointment == 1)
    {
        printf("CONFIRMED\n");
    }
    else
    {
        printf("NOT CONFIRMED / WALK-IN\n");
    }


    /* --------------------------------------------------------
       NO-SHOW
       -------------------------------------------------------- */

    if (p->willArrive == 0)
    {
        pthread_mutex_lock(&mutex);

        p->noShow = 1;

        printf("\n[Patient %d | %s] NO-SHOW.\n",
               p->id,
               p->name);

        /*
         * Release confirmed appointment slot.
         */
        if (p->appointment == 1)
        {
            if (activeAppointments > 0)
            {
                activeAppointments--;
            }

            printf("[Receptionist] %s's appointment slot "
                   "has been released.\n",
                   p->name);

            printf("[Receptionist] Active appointments: "
                   "%d/%d\n",
                   activeAppointments,
                   appointmentSlots);
        }

        pthread_mutex_unlock(&mutex);

        return NULL;
    }


    /* --------------------------------------------------------
       ARRIVE AT CLINIC
       -------------------------------------------------------- */

    printf("[Patient %d | %s] Arrived at the clinic.\n",
           p->id,
           p->name);

    pthread_mutex_lock(&mutex);


    /* Check doctor capacity */

    if (treatedPatients >= doctorCapacity)
    {
        p->leftDoctorFull = 1;

        printf("[Patient %d | %s] Doctor is full. "
               "Leaving clinic.\n",
               p->id,
               p->name);

        pthread_mutex_unlock(&mutex);

        return NULL;
    }


    /* --------------------------------------------------------
       ENTER WAITING ROOM
       -------------------------------------------------------- */

    if (waitingPatients < waitingRoomChairs)
    {
        arrivalCounter++;

        p->arrivalOrder = arrivalCounter;
        p->waiting = 1;

        waitingPatients++;

        printf("[Patient %d | %s] Sitting in waiting room.\n",
               p->id,
               p->name);

        printf("[Patient %d | %s] Chair %d/%d | "
               "FCFS Order #%d\n",
               p->id,
               p->name,
               waitingPatients,
               waitingRoomChairs,
               p->arrivalOrder);

        pthread_mutex_unlock(&mutex);

        /* Notify dentist */
        sem_post(&patientReady);

        /* Wait until dentist calls this patient */
        sem_wait(&p->dentistCallSem);

        pthread_mutex_lock(&mutex);

        if (p->leftDoctorFull == 1)
        {
            printf("[Patient %d | %s] Sent home because "
                   "doctor is full.\n",
                   p->id,
                   p->name);

            pthread_mutex_unlock(&mutex);

            return NULL;
        }

        pthread_mutex_unlock(&mutex);

        printf("[Patient %d | %s] Entering treatment room.\n",
               p->id,
               p->name);

        /* Wait for treatment */
        sem_wait(&p->treatmentDoneSem);

        printf("[Patient %d | %s] Treatment completed. "
               "Leaving clinic.\n",
               p->id,
               p->name);
    }


    /* --------------------------------------------------------
       WAITING ROOM FULL
       -------------------------------------------------------- */

    else
    {
        p->leftNoChair = 1;

        printf("[Patient %d | %s] Waiting room is FULL "
               "(%d/%d). Leaving clinic.\n",
               p->id,
               p->name,
               waitingPatients,
               waitingRoomChairs);

        pthread_mutex_unlock(&mutex);
    }

    return NULL;
}


/* ============================================================
   PATIENT TABLE
   ============================================================ */

/**
 * @brief Displays final results of all patients.
 */
void printPatientTable()
{
    printf("\n");

    printf("+-----+------------------+---------------+------------------+\n");
    printf("| ID  | Name             | Appointment   | Result           |\n");
    printf("+-----+------------------+---------------+------------------+\n");

    for (int i = 0; i < totalPatients; i++)
    {
        char appointmentText[20];
        char resultText[30];

        if (patients[i].appointment == 1)
        {
            strcpy(appointmentText, "Confirmed");
        }
        else
        {
            strcpy(appointmentText, "Not confirmed");
        }

        if (patients[i].treated == 1)
        {
            strcpy(resultText, "Treated");
        }
        else if (patients[i].noShow == 1)
        {
            strcpy(resultText, "No-show");
        }
        else if (patients[i].leftNoChair == 1)
        {
            strcpy(resultText, "Room full");
        }
        else if (patients[i].leftDoctorFull == 1)
        {
            strcpy(resultText, "Doctor full");
        }
        else
        {
            strcpy(resultText, "Left");
        }

        printf("| %-3d | %-16s | %-13s | %-16s |\n",
               patients[i].id,
               patients[i].name,
               appointmentText,
               resultText);
    }

    printf("+-----+------------------+---------------+------------------+\n");
}


/* ============================================================
   STATISTICS
   ============================================================ */

/**
 * @brief Displays final clinic statistics.
 */
void printStatistics()
{
    int noShowCount = 0;
    int roomFullCount = 0;
    int doctorFullCount = 0;

    for (int i = 0; i < totalPatients; i++)
    {
        if (patients[i].noShow == 1)
        {
            noShowCount++;
        }

        if (patients[i].leftNoChair == 1)
        {
            roomFullCount++;
        }

        if (patients[i].leftDoctorFull == 1)
        {
            doctorFullCount++;
        }
    }

    printf("\n");

    printf("+--------------------------------------------+\n");
    printf("|                 STATISTICS                 |\n");
    printf("+--------------------------------------------+\n");

    printf("| Total patients        : %-16d |\n",
           totalPatients);

    printf("| Confirmed appointments: %-16d |\n",
           confirmedAppointments);

    printf("| Patients treated      : %-16d |\n",
           treatedPatients);

    printf("| No-show patients      : %-16d |\n",
           noShowCount);

    printf("| Room-full patients    : %-16d |\n",
           roomFullCount);

    printf("| Doctor-full patients  : %-16d |\n",
           doctorFullCount);

    printf("| Waiting room chairs   : %-16d |\n",
           waitingRoomChairs);

    printf("| Doctor capacity       : %-16d |\n",
           doctorCapacity);

    printf("| Appointment slots     : %-16d |\n",
           appointmentSlots);

    printf("| Scheduling            : %-16s |\n",
           "FIFO / FCFS");

    printf("+--------------------------------------------+\n");
}


/* ============================================================
   MAIN FUNCTION
   ============================================================ */

/**
 * @brief Main entry point of the program.
 *
 * Takes clinic settings and patient information from the user,
 * initializes threads, mutexes and semaphores, starts the
 * simulation and finally cleans up all resources.
 *
 * @return 0 if the program finishes successfully.
 */
int main()
{
    int i;

    printf("\n");
    printf("============================================================\n");
    printf("              DENTIST CLINIC SIMULATION\n");
    printf("                    OS PROJECT\n");
    printf("============================================================\n");


    /* --------------------------------------------------------
       SHOW PROJECT INFORMATION
       -------------------------------------------------------- */

    showProjectInfo();


    /* --------------------------------------------------------
       USER INPUT
       -------------------------------------------------------- */

    printf("\nEnter number of patients: ");
    scanf("%d", &totalPatients);

    if (totalPatients <= 0)
    {
        printf("Invalid number of patients.\n");
        return 1;
    }


    printf("Enter number of waiting room chairs: ");
    scanf("%d", &waitingRoomChairs);

    if (waitingRoomChairs <= 0)
    {
        printf("Invalid number of chairs.\n");
        return 1;
    }


    printf("Enter doctor's maximum capacity: ");
    scanf("%d", &doctorCapacity);

    if (doctorCapacity <= 0)
    {
        printf("Invalid doctor capacity.\n");
        return 1;
    }


    printf("Enter number of appointment slots: ");
    scanf("%d", &appointmentSlots);

    if (appointmentSlots <= 0 ||
        appointmentSlots > totalPatients)
    {
        printf("Invalid appointment slot number.\n");
        return 1;
    }


    /* --------------------------------------------------------
       MEMORY ALLOCATION
       -------------------------------------------------------- */

    patients =
        malloc(totalPatients * sizeof(struct patient));

    patientThreads =
        malloc(totalPatients * sizeof(pthread_t));

    if (patients == NULL ||
        patientThreads == NULL)
    {
        printf("Memory allocation failed.\n");

        free(patients);
        free(patientThreads);

        return 1;
    }


    /* --------------------------------------------------------
       PATIENT INPUT
       -------------------------------------------------------- */

    printf("\n------------------------------------------------------------\n");
    printf("Enter patient information\n");
    printf("1 = Patient will come\n");
    printf("0 = Patient will not come (No-show)\n");
    printf("------------------------------------------------------------\n");

    for (i = 0; i < totalPatients; i++)
    {
        initializePatient(i);

        printf("\nPatient %d name: ",
               i + 1);

        scanf("%63s",
              patients[i].name);

        printf("Will %s come? (1=Yes, 0=No): ",
               patients[i].name);

        scanf("%d",
              &patients[i].willArrive);
    }


    /* --------------------------------------------------------
       CLINIC SETTINGS
       -------------------------------------------------------- */

    printf("\n============================================================\n");
    printf("                    CLINIC SETTINGS\n");
    printf("============================================================\n");

    printf("Total patients      : %d\n",
           totalPatients);

    printf("Waiting room chairs : %d\n",
           waitingRoomChairs);

    printf("Doctor capacity     : %d\n",
           doctorCapacity);

    printf("Appointment slots   : %d\n",
           appointmentSlots);

    printf("Scheduling          : FIFO / FCFS\n");

    printf("============================================================\n");


    /* --------------------------------------------------------
       INITIALIZE SYNCHRONIZATION
       -------------------------------------------------------- */

    pthread_mutex_init(&mutex, NULL);

    sem_init(&requestReady, 0, 0);
    sem_init(&patientReady, 0, 0);


    /* --------------------------------------------------------
       CREATE DENTIST THREAD
       -------------------------------------------------------- */

    pthread_t dentistThread;

    pthread_create(&dentistThread,
                   NULL,
                   dentist,
                   NULL);


    /* --------------------------------------------------------
       CREATE RECEPTIONIST THREAD
       -------------------------------------------------------- */

    pthread_t receptionistThread;

    pthread_create(&receptionistThread,
                   NULL,
                   receptionist,
                   NULL);


    printf("\n============================================================\n");
    printf("                  SIMULATION STARTED\n");
    printf("============================================================\n");


    /* --------------------------------------------------------
       CREATE PATIENT THREADS
       -------------------------------------------------------- */

    for (i = 0; i < totalPatients; i++)
    {
        pthread_create(&patientThreads[i],
                       NULL,
                       patient,
                       &patients[i]);

        /*
         * Small delay between patient arrivals.
         */
        usleep(300000);
    }


    /* --------------------------------------------------------
       WAIT FOR PATIENT THREADS
       -------------------------------------------------------- */

    for (i = 0; i < totalPatients; i++)
    {
        pthread_join(patientThreads[i],
                     NULL);
    }


    /* --------------------------------------------------------
       CLOSE CLINIC
       -------------------------------------------------------- */

    pthread_mutex_lock(&mutex);

    clinicClosed = 1;

    pthread_mutex_unlock(&mutex);


    /* Wake dentist and receptionist */
    sem_post(&patientReady);
    sem_post(&requestReady);


    /* Wait for dentist */
    pthread_join(dentistThread,
                 NULL);


    /* Wait for receptionist */
    pthread_join(receptionistThread,
                 NULL);


    /* --------------------------------------------------------
       FINAL RESULT
       -------------------------------------------------------- */

    printf("\n============================================================\n");
    printf("                     CLINIC CLOSED\n");
    printf("============================================================\n");

    printPatientTable();

    printStatistics();


    /* --------------------------------------------------------
       CLEANUP
       -------------------------------------------------------- */

    pthread_mutex_destroy(&mutex);

    sem_destroy(&requestReady);
    sem_destroy(&patientReady);

    for (i = 0; i < totalPatients; i++)
    {
        destroyPatient(i);
    }

    free(patients);
    free(patientThreads);

    printf("\nAll threads terminated successfully.\n");
    printf("Program finished.\n\n");

    return 0;
}

