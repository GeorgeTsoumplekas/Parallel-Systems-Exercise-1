/**
 * Parallel implementation of V3 using openMP
**/

#include <stdio.h>
#include "test.c"
#include <omp.h>
#include <time.h>

/**
 * Function that checks whether a specific column of the matrix (colNum) has an element in a specific row (wantedRow)
 * This is useful in deciding whether A[wantedRow][colNum] != 0 and calculating the number of triangles adjacen to each node
 * The implementation is based on binary search. Using binary search on rowVector we try to find the element with value wantetRow
 * If we find it we return its position on rowVector, otherwise we return -1.
 * Inputs:
 *      int* rowVector: the row indices array of the csc format
 *      int* colVector: the column changes array of the csc format
 *      int colNum: the index of the column we want to examine
 *      int wantedRow: the index of the row that we want our element to belong to
 * Outputs:
 *      int result: 1 if there is an element in (wanted row, colNum), 0 otherwise
 **/

int elementInColumnCheck(int* rowVector,int* colVector, int colNum, int wantedRow){
    int result=-1;

    int left = colVector[colNum];       //first element of the sub-array in which we do our binary search
    int right = colVector[colNum+1]-1;  //last element of the sub-array in which we do our binary search
    int middle = (left+right)/2;

    //binary search
    while(left<=right){
        if(rowVector[middle]<wantedRow){
            left = middle+1;
        }
        else if(rowVector[middle]==wantedRow){
            result = middle;
            break;
        }
        else{
            right = middle-1;
        }
        middle = (left+right)/2;    
    }
    return result;
}

/**
 * The actual function the different threads execute in parallel. 
 * This function calculates the number of triangles adjacent to a particular node i.
 * Algorithm works as follows: 
 * First of we take each pair of nonzero elements that belong in the same column i, let's say elements (j,i), (k,i). There is no need to check 
 * the pair (k,i), (j,i) since we would calculate the same triangle twice this way. So we examine only the cases where k>j.
 * If the element (j,k) is a non zero element then we have a triangle. We only increase the value of triangleCount[i] because if we increased
 * triangleCount[element1] and triangleCount[element2] we would add the same triangle three times instead of one.
 * Input:
 *      int* rowVector: the row indices array of the csc format
 *      int* colVector: the column changes array of the csc format
 *      int* triangleCount: the array containing the number of triangles adjacent to each node
 *      int i: the column index (also the node index) of which we want to compute the number of triangles
 * Output:
 *      None
**/

void compute(int* rowVector, int* colVector, int* triangleCount, int i){
    
    int elemsInCol = colVector[i+1]- colVector[i];  //number of nonzero elements in column i

    int element1;   //first common idice we investigate
    int element2;   //second common indice we investigate

    //Check for every pair of the column with this double for loop
    for(int j=0; j<elemsInCol-1; j++){
        element1 = rowVector[colVector[i]+j];
        for (int k=j+1; k<elemsInCol; k++ ){
            element2 = rowVector[colVector[i]+k];            
            //Check if the third common indice exists
            if (elementInColumnCheck(rowVector, colVector, element1, element2)>=0){
                triangleCount[i]++;   
            }
        }      
    }
}


int main(int argc, char* argv[]){
    
    FILE *stream;       //file pointer to read the given file
    MM_typecode t;      //the typecode struct
    
    if(argc<2){
        printf("Please pass as argument the .mtx file\n");
        exit(-1);
    }  

    char* s=argv[1];

    //Checking if the argument is .mtx file
    int nameLength = strlen(s);    //length of the name of the file    
    if(!((s[nameLength-1]=='x') && (s[nameLength-2]=='t') && (s[nameLength-3]=='m') && (s[nameLength-4]=='.'))){
        printf("Your argument is not an .mtx file\n");
        exit(-1);
    }

    //Opening The file as shown in the command line
    stream=fopen(s, "r");        
    if(stream==NULL){
        printf("Could not open file, pass another file\n");
        exit(-1);
    }

    mm_read_banner(stream,&t);

    //Checking if the matrix type is ok
    if (mm_is_sparse(t)==0){
        printf("The array is not sparce. Please give me another matrix market file\n");
        exit(-1);
    }
    if (mm_is_coordinate(t)==0){
        printf("The array is not in coordinate format. Please give me another matrix market file\n");
        exit(-1);
    }
    if (mm_is_symmetric(t)==0){
        printf("The array is not symmetric. Please give me another matrix market file\n");
        exit(-1);
    }

    if(argc<3){
        printf("Please give me the wanted number of threads as an argument too\n");
        exit(-1);
    }

    CSCArray* cscArray = COOtoCSC(stream);  //The sparse array in csc format
    int M = cscArray->M;
    int* rowVector = cscArray->rowVector;
    int* colVector = cscArray->colVector;

    int* triangleCount = calloc(cscArray->M, sizeof(int));    //Each entry contains the number of triangles in which at least an element of this column belongs to
    if(triangleCount==NULL){
        printf("Error in main: Couldn't allocate memory for triangleCount");
        exit(-1);
    }

    int threadNum = atoi(argv[2]);  //number of threads
    printf("\nYou have chosen %d threads \n",threadNum);

    //Start timer
    struct timespec init;
    clock_gettime(CLOCK_MONOTONIC, &init);

    //Set number of threads
    omp_set_num_threads(threadNum);

    /**
     * Execute compute function in parallel using a parallel for. Scheduling is set to dynamic for faster results since there is no need
     * for the treads to be executed in a certain order.
     * Moreover, parallelizing more than that is not efficient since we cannot have as many or more threads simultaneously than the number
     * of columns M of the matrix. Trying to parallelize more only made the program run more slowly.
    **/ 

    #pragma omp parallel for schedule(dynamic)
    for(int i=0; i<M; i++){
        compute(rowVector, colVector, triangleCount, i);
    }

    //End timer
    struct timespec last;   
    clock_gettime(CLOCK_MONOTONIC, &last);

    long ns;
    int seconds;
    if(last.tv_nsec <init.tv_nsec){
        ns=init.tv_nsec - last.tv_nsec;
        seconds= last.tv_sec - init.tv_sec -1;
    }

    if(last.tv_nsec >init.tv_nsec){
        ns= last.tv_nsec -init.tv_nsec ;
        seconds= last.tv_sec - init.tv_sec ;
    }
    printf("The seconds elapsed are %d and the nanoseconds are %ld\n",seconds, ns); 

    CSCArrayfree(cscArray);
    free(cscArray);

    int totalTriangles=0; //Total number of triangles

    //Compute the total number of triangles. This is done by applying a reduction, so that it is computed faster    
    #pragma omp parallel for reduction (+:totalTriangles)
    for (int i=0; i<M; i++){
        totalTriangles += triangleCount[i];
    }

    //We divide the total number by 3 because now each triangle is added 3 times, 1 for each node in which it is adjacent
    printf("Total triangles = %d\n",totalTriangles/3);
   
    free(triangleCount);
    
    return 0;
}