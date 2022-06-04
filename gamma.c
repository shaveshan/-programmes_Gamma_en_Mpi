/*
	Implémentation distribuée d'un programme GAMMA à l'aide de MPI
	Exemple de la somme des éléments d'un multiset
	On a en donnée:
	multiset = {10, 4, 71, 15, 21, 14, 51, 19, 1, 9, 32, 17, 12, 4, 61, 8}
	N = 16
	éléments sont simples
	Réaction R(x, y) = TRUE
	Action   A(x, y) = x+y
*/

#include <stdio.h>
#include <unistd.h>
#include "mpi.h"
#include <stdlib.h>

int N_threads = 8;
int N = 16;
int old_ms[16] = {1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1};
int new_ms[16] = {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};
int N_taches, N_taches_per_thread;

void actifs(int deb, int fin)
{
	int i, slave, tag=50;
	MPI_Status status;
	for (slave=deb; slave<fin; slave++)
	{
		MPI_Send(&old_ms[2*slave], 1, MPI_INT, slave,tag, MPI_COMM_WORLD);
		MPI_Send(&old_ms[2*slave+1], 1, MPI_INT, slave,tag, MPI_COMM_WORLD);
		//printf("Maitre sent to actif %d out of %d       // data %d %d\n", slave, fin, old_ms[2*slave], old_ms[2*slave+1]);
			
		MPI_Recv(&new_ms[slave], 1, MPI_INT, slave, tag, MPI_COMM_WORLD, &status);
		//printf("Maitre received from actif %d out of %d // data %d\n", slave, fin, new_ms[slave]);
	}
}

void passifs(int deb, int fin)
{
	int i, slave, tag=50;
	MPI_Status status;
	int dummy = 0;
	for (slave=deb; slave<fin; slave++)
	{
		MPI_Send(&dummy, 1, MPI_INT, slave,tag, MPI_COMM_WORLD);
		MPI_Send(&dummy, 1, MPI_INT, slave,tag, MPI_COMM_WORLD);
		//printf("Maitre sent to passif %d out of %d       // data %d %d\n", slave, fin, dummy, dummy);
			
		MPI_Recv(&dummy, 1, MPI_INT, slave, tag, MPI_COMM_WORLD, &status);
		//printf("Maitre received from passif %d out of %d // data %d\n", slave, fin, dummy);
	}
}

void master_as_worker(int ind0, int ind1)
{
	new_ms[ind0] = old_ms[ind0] + old_ms[ind1];
	//printf("When number of tasks = %d the old_ms[%d] = %d\n", N_taches, ind0, old_ms[ind0]);
}

void action (void)
{
	int master = 0;
	int tag = 50;
	MPI_Status status; 	/* Return status for receive */
	int x, y, res;
	MPI_Recv(&x, 1, MPI_INT, master, tag, MPI_COMM_WORLD, &status);
	MPI_Recv(&y, 1, MPI_INT, master, tag, MPI_COMM_WORLD, &status);

	res = x + y;
		
	MPI_Send(&res, 1, MPI_INT, master,tag, MPI_COMM_WORLD);
}

//====================================================fonction de l'exclave==========================================
void worker(void)
{
	int iter1, iter2, cycle = 3;
	// cas ou le nombre de taches est supérieur au nombre de threads
	if (N_taches > N_threads)
	{
		for (iter1 = 1; iter1 < N_taches_per_thread; iter1++)
		{ 
			action();
		}
	}else
		// cas ou le nombre de taches est égal ou inférieur au  nombre de threads
		for (iter2 = 0; iter2 < cycle; iter2++)
		{
			action();
		}
}

//====================================================fonction du maitre==========================================
void master_as_dispacher(void)
{
	int i, iter, cycle = 3, slave;
	int tag = 50;
	MPI_Status status;

	// Cas ou le nombre de taches est supérieur au nombre de threads
	if (N_taches > N_threads)
	{
		while (N_taches > N_threads)
		{
			// Distribution des taches aux threads
			N_taches_per_thread = N_taches / N_threads;
			for (iter=0; iter<N_taches_per_thread; iter++)
			{
				master_as_worker(iter*N_taches_per_thread, iter*N_taches_per_thread + 1);
				actifs(iter*N_taches_per_thread + 1, N_threads);
			}
			N_taches = N_taches/2;
			N_taches_per_thread = N_taches_per_thread/2;
		}
	}
	// Cas ou le nombre de taches est égal ou inférieur au nombre de threads
	if (N_taches <= N_threads)
	{	
		while (N_taches > 1)		
		{	
			if (N_taches ==	N_threads)
			{
				master_as_worker(0, 1);
				actifs(1, N_taches);
			}else
			{
				master_as_worker(0, 1);
				actifs(1, N_taches);
				passifs(N_taches, N_threads);
			}
			for(iter=0; iter<N_taches; iter++) old_ms[iter] = new_ms[iter];
			printf("\n When N_taches = %d new_ms = ", N_taches);
			for(iter=0; iter<N_taches; iter++) printf(" %d ", old_ms[iter]);
			N_taches = N_taches/2;				
		}
		// Dernier tache est prise par le maitre
		master_as_worker(0, 1);	
		for(iter=0; iter<N_taches; iter++) old_ms[iter] = new_ms[iter];
		printf("\n When N_taches = %d new_ms = ", N_taches);
		for(iter=0; iter<N_taches; iter++) printf(" %d ", old_ms[iter]);
		printf("\n");
	}		
	// Afficher le résultatS	
	//printf("Résult = %d\n", old_ms[0]);
}

void Afficher_Info(void)
{
	int i;
	printf("========Info about the GAMMA program==============================\n");
	printf("		============================\n");
	printf("		Taille du Multiset = %d\n", N);
	printf("		Données = ");
	for(i=0; i<N; i++) printf(" %d ", old_ms[i]);
	printf("\n");
	printf("		Réaction = (x, y)\n");
	printf("		Action = (x, y) <-- x + y\n");
	printf("		Nombre de threads  = %d\n", N_threads);
	printf("		Nombre de taches   = %d\n", N_taches);
	printf("===================================================================\n");
}

//=======================================Programme principal=================================================
int main(int argc, char** argv) 
{
	int rank; 			/* Rank of process */
	int tag = 50;		/* Tag for messages */
	// int multiset[16] = {10, 4, 71, 15, 21, 14, 51, 19, 1, 9, 32, 17, 12, 4, 61, 8};

	MPI_Init(&argc, &argv);
	MPI_Comm_rank(MPI_COMM_WORLD, &rank);
	
	N_taches = N/2;
	N_taches_per_thread = N_taches/N_threads;
		
	if(rank == 0)  	
	{
		Afficher_Info();
		master_as_dispacher();
	}else				
	{
		worker();
	}
	
	MPI_Finalize();	
	return 0;
}
