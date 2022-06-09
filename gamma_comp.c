/*
	Implémentation distribuée d'un programme GAMMA à l'aide de MPI
	Exemple de la somme des éléments d'un multiset

	https://colab.research.google.com/
	!mpicc sum.c -o sum
	!mpirun --allow-run-as-root -np 8 ./sum
*/


#include <stdio.h>
#include <unistd.h>
#include "mpi.h"
#include <stdlib.h>

#define N_threads 8
#define N 16
#define COMP 2



int old_ms[N][COMP] , new_ms[N][COMP] , perd_ms[N][COMP];
int N_taches, N_taches_per_thread;
int perd_rank = 0;
int cycle = 0;


void init_data(){
	for(int i=0; i<N; i++)  
        for(int j=0; j<COMP; j++) {
            old_ms[i][j]   = 1+j;
            new_ms[i][j]   = 0;
            perd_ms[i][j]  = 0;
        }	  
}



void actifs(int deb, int fin){
	int i, slave, tag=50;
	MPI_Status status;
	for (slave=deb; slave<fin; slave++){
		MPI_Send(old_ms[2*slave],COMP, MPI_INT, slave%N_threads,tag, MPI_COMM_WORLD);
		MPI_Send(old_ms[2*slave+1],COMP, MPI_INT, slave%N_threads,tag, MPI_COMM_WORLD);
		MPI_Recv(new_ms[slave],COMP, MPI_INT, slave%N_threads, tag, MPI_COMM_WORLD, &status);
	}
}

void passifs(int deb, int fin){
	int i, slave, tag=50;
	MPI_Status status;
	int* dummy;
    dummy = malloc (COMP * sizeof(int));
    for(int i=0;i<COMP;i++) dummy[i] = 0;
    
	for (slave=deb; slave<fin; slave++){
		MPI_Send(dummy, COMP , MPI_INT, slave%N_threads,tag, MPI_COMM_WORLD);
		MPI_Send(dummy, COMP , MPI_INT, slave%N_threads,tag, MPI_COMM_WORLD);
        //printf("Maitre sent to passif %d out of %d\n", slave%N, fin);
		MPI_Recv(dummy, COMP , MPI_INT, slave%N_threads, tag, MPI_COMM_WORLD, &status);
	}
    free(dummy);
}

void master_as_worker(int ind0, int ind1){
    //printf("master_as_worker : When number of tasks = %d\n", N_taches);
    for(int j=0; j<COMP; j++)  new_ms[ind0][j] = old_ms[ind0][j] + old_ms[ind1][j];
	
}

void action (void){
	int master = 0;
	int tag = 50;
	MPI_Status status; 	/* Return status for receive */
	int *x ;
	int *y ;
	int *res;
	x   = malloc (COMP * sizeof(int));
	y   = malloc (COMP * sizeof(int));  
	res = malloc (COMP * sizeof(int));

	MPI_Recv(x, COMP, MPI_INT, master, tag, MPI_COMM_WORLD, &status);
	MPI_Recv(y, COMP, MPI_INT, master, tag, MPI_COMM_WORLD, &status);

    for(int i = 0 ; i<COMP ;i++) res[i] = x[i] + y[i];

	MPI_Send(res, COMP, MPI_INT, master,tag, MPI_COMM_WORLD);
    //printf("Action : x= {%d,%d} & y = {%d,%d}\n", x[0], x[1] ,y[0] , y[1]);
    free(x);
    free(y);
    free(res);
}

//====================================================fonction de l'exclave==========================================



int get_cycle( int lendata){
    
    int tache = lendata/2 , cyc = 0 ;
    while(tache > 1){
        cyc += (tache % 8 == 0) ? tache / 8 : tache / 8 + 1;
        tache /= 2;
    }

    int data = lendata, cyc_perd=0;
    while(data> 1){
        if(data%2) cyc_perd++;
        data /=2; 
    }
    cyc_perd += cyc_perd > 0 ? 1:0;
    return cyc + (cyc_perd > 0 ? get_cycle(cyc_perd) : 0);
}


void worker(void)  {
	// cas ou le nombre de taches est supérieur au nombre de threads
	for (int i = 0; i < cycle ; i++)  action();
}

//====================================================fonction du maitre==========================================
void master_as_dispacher(void){
	int slave;
	int tag = 50;
	MPI_Status status;

	// Cas ou le nombre de taches est supérieur au nombre de threads
	if (N_taches > N_threads)
	{
		while (N_taches > N_threads){
			// Distribution des taches aux threads
			N_taches_per_thread = N_taches / N_threads;
			for (int i=0; i<N_taches_per_thread; i++){
				master_as_worker(i*N_threads, i*N_threads + 1);
				actifs(i*N_threads + 1, N_threads*(i+1));
			}


			if ( N_taches !=	(N_threads* N_taches_per_thread) ){
				master_as_worker(N_taches_per_thread*N_threads, (N_taches_per_thread*N_threads) + 1);

				if((N_taches_per_thread*N_threads) + 1 != N_taches ) actifs((N_taches_per_thread*N_threads) + 1, N_taches);
				
                passifs(N_taches, N_threads*(N_taches_per_thread+1) );
			} 
            
            

			if(N_taches % 2 == 1 ) {
                for(int j=0; j<COMP; j++) perd_ms[perd_rank][j] =  new_ms[N_taches - 1][j];
				perd_rank++;
			}

			for(int i=0; i<N_taches; i++)
                for(int j=0; j<COMP; j++) 
                    old_ms[i][j] = new_ms[i][j];

			N_taches = N_taches/2;
			N_taches_per_thread = N_taches_per_thread/2;
		}
	}
	// Cas ou le nombre de taches est égal ou inférieur au nombre de threads
	if (N_taches <= N_threads)
	{	
		
		while (N_taches > 1)		
		{	
			
			master_as_worker(0, 1);
			actifs(1, N_taches);
			if (N_taches !=	N_threads) passifs(N_taches, N_threads);
			for(int i=0; i<N_taches; i++)
                for(int j=0 ; j<COMP ;j++)
                    old_ms[i][j] = new_ms[i][j];
			
            

			if(N_taches % 2 == 1 ){
                for(int j=0 ; j<COMP ;j++) perd_ms[perd_rank][j] =  new_ms[N_taches - 1][j];
				perd_rank++;
			}

			N_taches = N_taches/2;
						
		}
		// Dernier tache est prise par le maitre
		master_as_worker(0, 1);

		for(int i=0; i<N_taches; i++)
                for(int j=0 ; j<COMP ;j++)
                    old_ms[i][j] = new_ms[i][j];
        
		
	}

	if( perd_rank > 0 ){
        for(int j=0 ; j<COMP ;j++)
		    perd_ms[perd_rank][j] = old_ms[0][j]; 
		N_taches = (perd_rank + 1)/2;
		for(int i=0; i<perd_rank+1; i++) 
            for(int j=0 ; j<COMP ;j++)
                old_ms[i][j] = perd_ms[i][j];

		if( perd_rank % 2 == 1){
			perd_rank = 0;
		}else{
            for(int j=0 ; j<COMP ;j++)	perd_ms[0][j] = old_ms[perd_rank][j];
		 	perd_rank = 1;
		}
		master_as_dispacher();
	}else{
		// Afficher le résultatS	
	    printf("Résult = (");
        for(int j=0;j<COMP-1;j++)printf(" %d ,", old_ms[0][j]);
        printf(" %d )\n" , old_ms[0][COMP-1]);
    
	}		

}

void Afficher_Info(void){

	printf("========Info about the GAMMA program==============================\n");
	printf("		============================\n");
	printf("		Taille du Multiset = %d\n", N);

	printf("		Données = \n");
	for(int i=0;i<N;i++){
        for(int j=0;j<COMP;j++)printf("old [%d][%d] = %d ", i,j,old_ms[i][j]);
        
        printf("\n");
    }
	printf("\n");
	printf("		Réaction = (x, y)\n");
	printf("		Action = ([x1,x2] , [y1,y2]) <-- (x1+y1 , x2+y2) \n");
	printf("		Nombre de threads  = %d\n", N_threads);
	printf("		Nombre de taches   = %d\n", N_taches);
    printf("		Cycle              = %d\n" , cycle);
	printf("===================================================================\n");
}



//=======================================Programme principal=================================================
int main(int argc, char** argv) {
	init_data();
    cycle = get_cycle(N);

    if(N < 2 || COMP < 0){
        printf("ERR 404 \n");
        exit(1);
    }
    
	int rank; 			/* Rank of process */
	int tag = 50;		/* Tag for messages */

	MPI_Init(&argc, &argv);
	MPI_Comm_rank(MPI_COMM_WORLD, &rank);

	if(N % 2 == 1){
        for(int j= 0 ; j<COMP ;j++) perd_ms[perd_rank][j] =  old_ms[N - 1][j];
		perd_rank++;
	}
	N_taches = N/2;
		
	if(rank == 0)  	
	{
		Afficher_Info();
		master_as_dispacher();
	}
    else worker();
	
	MPI_Finalize();	
	return 0;
}
