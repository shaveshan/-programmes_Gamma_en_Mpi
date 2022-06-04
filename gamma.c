/*
	Implémentation distribuée d'un programme GAMMA à l'aide de MPI
	Exemple de la somme des éléments d'un multiset
	On a en donnée:
	multiset = {10, 4, 71, 15, 21, 14, 51, 19, 1, 9, 32, 17, 12, 4, 61, 8}
	N = 16
	éléments sont simples
	Réaction R(x, y) = TRUE
	Action   A(x, y) = x+y


	!mpicc sum.c -o sum
	!mpirun --allow-run-as-root -np 8 ./sum
*/


#include <stdio.h>
#include <unistd.h>
#include "mpi.h"
#include <stdlib.h>

#define N_threads 8
#define N 70
int old_ms[N] , new_ms[N] , perd_ms[N];
int N_taches, N_taches_per_thread;
int perd_rank = 0;


void init_data(){
	for(int iter=0; iter<N; iter++)  old_ms[iter]  = 1;
	for(int iter=0; iter<N; iter++)  new_ms[iter]  = 0;
	for(int iter=0; iter<N; iter++)  perd_ms[iter] = 0;
}



void actifs(int deb, int fin)
{
	int i, slave, tag=50;
	MPI_Status status;
	for (slave=deb; slave<fin; slave++)
	{
		MPI_Send(&old_ms[2*slave], 1, MPI_INT, slave%N_threads,tag, MPI_COMM_WORLD);
		MPI_Send(&old_ms[2*slave+1], 1, MPI_INT, slave%N_threads,tag, MPI_COMM_WORLD);
		//printf("Maitre sent to actif %d out of %d       // data old[%d] + old[%d] \n", slave%N, fin, 2*slave , 2*slave+1);
			
		MPI_Recv(&new_ms[slave], 1, MPI_INT, slave%N_threads, tag, MPI_COMM_WORLD, &status);
		//printf("Maitre received from actif %d out of %d // data new[%d] \n", slave%N, fin, slave);
	}
}

void passifs(int deb, int fin)
{
	int i, slave, tag=50;
	MPI_Status status;
	int dummy = 0;
	for (slave=deb; slave<fin; slave++)
	{
		MPI_Send(&dummy, 1, MPI_INT, slave%N_threads,tag, MPI_COMM_WORLD);
		MPI_Send(&dummy, 1, MPI_INT, slave%N_threads,tag, MPI_COMM_WORLD);
		//printf("Maitre sent to passif %d out of %d       // data %d %d\n", slave%N, fin, dummy, dummy);



		MPI_Recv(&dummy, 1, MPI_INT, slave%N_threads, tag, MPI_COMM_WORLD, &status);
		//printf("Maitre received from passif %d out of %d // data %d\n", slave%N, fin, dummy);
	}
}

void master_as_worker(int ind0, int ind1)
{
	new_ms[ind0] = old_ms[ind0] + old_ms[ind1];
	//printf("master_as_worker : When number of tasks = %d the new_ms[%d] = %d\n", N_taches, ind0, new_ms[ind0]);
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




int get_cycle( int lendata){
    int tache = lendata/2 , cyc = 0 ;
    while(tache > 1){
        cyc += (tache % 8 == 0) ? tache / 8 : tache / 8 + 1;
        tache /= 2;
    }
    return cyc ;
}


void worker(void)  {
	// cas ou le nombre de taches est supérieur au nombre de threads
	for (int iter1 = 0; iter1 < get_cycle(N) ; iter1++)  action();
}

//====================================================fonction du maitre==========================================
void master_as_dispacher(void)
{
	int i, iter, slave;
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
				master_as_worker(iter*N_threads, iter*N_threads + 1);
				actifs(iter*N_threads + 1, N_threads*(iter+1));
			}


			if ( N_taches !=	(N_threads* N_taches_per_thread) ){
				master_as_worker(N_taches_per_thread*N_threads, (N_taches_per_thread*N_threads) + 1);

				if((N_taches_per_thread*N_threads) + 1 != N_taches ) actifs((N_taches_per_thread*N_threads) + 1, N_taches);
				passifs(N_taches, N_threads*(N_taches_per_thread+1) );
			} 

			if(N_taches % 2 == 1 ) {
				perd_ms[perd_rank] =  new_ms[N_taches - 1];
				perd_rank++;
			}

			for(iter=0; iter<N_taches; iter++) old_ms[iter] = new_ms[iter];


			N_taches = N_taches/2;
			N_taches_per_thread = N_taches_per_thread/2;
		}
	}
	// Cas ou le nombre de taches est égal ou inférieur au nombre de threads
	if (N_taches <= N_threads)
	{	
		//printf("\n ( When N_taches = %d ) <= ( N_threads = %d ) \n", N_taches , N_threads);
		int p = 0;
		while (N_taches > 1)		
		{	
			

			master_as_worker(0, 1);
			actifs(1, N_taches);
			if (N_taches !=	N_threads) passifs(N_taches, N_threads);



			for(iter=0; iter<N_taches; iter++) old_ms[iter] = new_ms[iter];
			//printf("\n When N_taches = %d new_ms = ", N_taches);
			//for(iter=0; iter<N_taches; iter++) printf(" %d ", old_ms[iter]);
			


			if(N_taches % 2 == 1 ){
				perd_ms[perd_rank] =  new_ms[N_taches - 1];
				perd_rank++;
			}


			N_taches = N_taches/2;
						
		}
		// Dernier tache est prise par le maitre
		master_as_worker(0, 1);


	
		for(iter=0; iter<N_taches; iter++) old_ms[iter] = new_ms[iter];
		//printf("\n When N_taches = %d new_ms  = ", N_taches);
		//for(iter=0; iter<N_taches; iter++) printf(" %d ", old_ms[iter]);
		//printf("\n");

		

		


		

		
	}

	if( perd_rank > 0 ){

		perd_ms[perd_rank] = old_ms[0]; 
		N_taches = (perd_rank + 1)/2;
		for(iter=0; iter<perd_rank+1; iter++) old_ms[iter] = perd_ms[iter];
		//printf("\n --------> perd_rank = %d  ----- perd_ms = ", perd_rank);
		//for(iter=0; iter<perd_rank+1; iter++)  printf(" %d ", perd_ms[iter]);
		//printf("\n");
		if( perd_rank % 2 == 1){
			perd_rank = 0;
		}else{
			perd_ms[0] = old_ms[perd_rank];
			perd_rank = 1;
		}
		master_as_dispacher();
	}else{
		// Afficher le résultatS	
		printf("Résult = %d\n", old_ms[0]);
	}		

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
	init_data();
	int rank; 			/* Rank of process */
	int tag = 50;		/* Tag for messages */
	// int multiset[16] = {10, 4, 71, 15, 21, 14, 51, 19, 1, 9, 32, 17, 12, 4, 61, 8};

	MPI_Init(&argc, &argv);
	MPI_Comm_rank(MPI_COMM_WORLD, &rank);

	if(N % 2 == 1){
		perd_ms[perd_rank] =  old_ms[N - 1];
		perd_rank++;
	}

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
