#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <assert.h>
#include <unistd.h>
#include <sys/time.h>

#define BLOCK_SIZE 32

#define STR_SIZE 256

/* maximum power density possible (say 300W for a 10mm x 10mm chip)	*/
#define MAX_PD	(3.0e6)
/* required precision in degrees	*/
#define PRECISION	0.001
#define SPEC_HEAT_SI 1.75e6
#define K_SI 100
/* capacitance fitting factor	*/
#define FACTOR_CHIP	0.5

/* chip parameters	*/
float t_chip = 0.0005;
float chip_height = 0.016;
float chip_width = 0.016;
/* ambient temperature, assuming no package at all	*/
float amb_temp = 80.0;

void run(int argc, char** argv);

/* define timer macros */
#define pin_stats_reset()   startCycle()
#define pin_stats_pause(cycles)   stopCycle(cycles)
#define pin_stats_dump(cycles)    printf("timer: %Lu\n", cycles)


#define CUDA_CALL_SAFE(f) \
    do \
    {                                                        \
        cudaError_t _cuda_error = f;                         \
        if (_cuda_error != cudaSuccess)                      \
        {                                                    \
            fprintf(stderr,  \
                "%s, %d, CUDA ERROR: %s %s\n",  \
                __FILE__,   \
                __LINE__,   \
                cudaGetErrorName(_cuda_error),  \
                cudaGetErrorString(_cuda_error) \
            ); \
            abort(); \
            exit(EXIT_FAILURE); \
        } \
    } while (0)        

static inline double time_diff(struct timeval tv_start, struct timeval tv_end)
{
    return (double)(tv_end.tv_sec - tv_start.tv_sec) * 1000.0 + (double)(tv_end.tv_usec - tv_start.tv_usec) / 1000.0;
}


/*void writeoutput(float *vect, int grid_rows, int grid_cols, char *file)
{

	int i,j, index=0;
	FILE *fp;
	char str[STR_SIZE];

	if( (fp = fopen(file, "w" )) == 0 )
          printf( "The file was not opened\n" );


	for (i=0; i < grid_rows; i++) 
	 for (j=0; j < grid_cols; j++)
	 {

		 sprintf(str, "%d\t%g\n", index, vect[i*grid_cols+j]);
		 fputs(str,fp);
		 index++;
	 }
		
      fclose(fp);	
}*/

void writeoutput(float *vect, int grid_rows, int grid_cols, char *file)
{
	FILE *fp;

	if ((fp = fopen(file, "wb")) == 0)
    {
        fprintf(stderr, "The file was not opened\n");
        abort();
        exit(EXIT_FAILURE);
    }

    if (fwrite((char *)vect, sizeof(float) * grid_rows * grid_cols, 1, fp) != 1)
    {
        fprintf(stderr, "The file was not written\n");
        abort();
        exit(EXIT_FAILURE);
    }

    fsync(fileno(fp));

	fclose(fp);	
}


/*void readinput(float *vect, int grid_rows, int grid_cols, char *file){

  	int i,j;
	FILE *fp;
	char str[STR_SIZE];
	float val;

	if( (fp  = fopen(file, "r" )) ==0 )
            printf( "The file was not opened\n" );


	for (i=0; i <= grid_rows-1; i++) 
	 for (j=0; j <= grid_cols-1; j++)
	 {
		fgets(str, STR_SIZE, fp);
		if (feof(fp))
			fatal("not enough lines in file");
		//if ((sscanf(str, "%d%f", &index, &val) != 2) || (index != ((i-1)*(grid_cols-2)+j-1)))
		if ((sscanf(str, "%f", &val) != 1))
			fatal("invalid file format");
		vect[i*grid_cols+j] = val;
	}

	fclose(fp);	

}*/

void readinput(float *vect, int grid_rows, int grid_cols, char *file)
{
	FILE *fp;

	if((fp = fopen(file, "rb")) == 0)
    {
        fprintf(stderr, "The file was not opened\n");
        abort();
        exit(EXIT_FAILURE);
    }

    if (fread((char *)vect, sizeof(float) * grid_rows * grid_cols, 1, fp) != 1)
    {
        fprintf(stderr, "The file was not read\n");
        abort();
        exit(EXIT_FAILURE);
    }

	fclose(fp);	
}

#define IN_RANGE(x, min, max)   ((x)>=(min) && (x)<=(max))
#define CLAMP_RANGE(x, min, max) x = (x<(min)) ? min : ((x>(max)) ? max : x )
#define MIN(a, b) ((a)<=(b) ? (a) : (b))

__global__ void calculate_temp(long iteration,  //number of iteration
                               float *power,   //power input
                               float *temp_src,    //temperature input/output
                               float *temp_dst,    //temperature input/output
                               long grid_cols,  //Col of grid
                               long grid_rows,  //Row of grid
							   long border_cols,  // border offset 
							   long border_rows,  // border offset
                               float Cap,      //Capacitance
                               float Rx, 
                               float Ry, 
                               float Rz, 
                               float step, 
                               float time_elapsed)
{
	
    __shared__ float temp_on_cuda[BLOCK_SIZE][BLOCK_SIZE];
    __shared__ float power_on_cuda[BLOCK_SIZE][BLOCK_SIZE];
    __shared__ float temp_t[BLOCK_SIZE][BLOCK_SIZE]; // saving temparary temperature result

	float amb_temp = 80.0;
    float step_div_Cap;
    float Rx_1,Ry_1,Rz_1;
        
	long bx = blockIdx.x;
    long by = blockIdx.y;

	long tx = threadIdx.x;
	long ty = threadIdx.y;
	
	step_div_Cap = step / Cap;
	
	Rx_1 = 1 / Rx;
	Ry_1 = 1 / Ry;
	Rz_1 = 1 / Rz;
	
    // each block finally computes result for a small block
    // after N iterations. 
    // it is the non-overlapping small blocks that cover 
    // all the input data

    // calculate the small block size
	long small_block_rows = BLOCK_SIZE - iteration * 2;//EXPAND_RATE
	long small_block_cols = BLOCK_SIZE - iteration * 2;//EXPAND_RATE

    // calculate the boundary for the block according to 
    // the boundary of its small block
    long blkY = small_block_rows * by - border_rows;
    long blkX = small_block_cols * bx - border_cols;
    long blkYmax = blkY + BLOCK_SIZE - 1;
    long blkXmax = blkX + BLOCK_SIZE - 1;

    // calculate the global thread coordination
	long yidx = blkY + ty;
	long xidx = blkX + tx;

    // load data if it is within the valid input range
	long loadYidx = yidx, loadXidx = xidx;
    long index = grid_cols * loadYidx + loadXidx;
       
	if (IN_RANGE(loadYidx, 0, grid_rows - 1) && IN_RANGE(loadXidx, 0, grid_cols - 1))
    {
        temp_on_cuda[ty][tx] = temp_src[index];  // Load the temperature data from global memory to shared memory
        power_on_cuda[ty][tx] = power[index];    // Load the power data from global memory to shared memory
	}
	__syncthreads();

    // effective range within this block that falls within 
    // the valid range of the input data
    // used to rule out computation outside the boundary.
    long validYmin = (blkY < 0) ? -blkY : 0;
    long validYmax = (blkYmax > grid_rows-1) ? BLOCK_SIZE-1-(blkYmax-grid_rows+1) : BLOCK_SIZE-1;
    long validXmin = (blkX < 0) ? -blkX : 0;
    long validXmax = (blkXmax > grid_cols-1) ? BLOCK_SIZE-1-(blkXmax-grid_cols+1) : BLOCK_SIZE-1;

    long N = ty-1;
    long S = ty+1;
    long W = tx-1;
    long E = tx+1;
    
    N = (N < validYmin) ? validYmin : N;
    S = (S > validYmax) ? validYmax : S;
    W = (W < validXmin) ? validXmin : W;
    E = (E > validXmax) ? validXmax : E;

    bool computed;
    for (long i=0; i<iteration ; i++){ 
        computed = false;
        if( IN_RANGE(tx, i+1, BLOCK_SIZE-i-2) &&  \
              IN_RANGE(ty, i+1, BLOCK_SIZE-i-2) &&  \
              IN_RANGE(tx, validXmin, validXmax) && \
              IN_RANGE(ty, validYmin, validYmax) ) {
              computed = true;
              temp_t[ty][tx] =   temp_on_cuda[ty][tx] + step_div_Cap * (power_on_cuda[ty][tx] + 
                 (temp_on_cuda[S][tx] + temp_on_cuda[N][tx] - 2.0*temp_on_cuda[ty][tx]) * Ry_1 + 
                 (temp_on_cuda[ty][E] + temp_on_cuda[ty][W] - 2.0*temp_on_cuda[ty][tx]) * Rx_1 + 
                 (amb_temp - temp_on_cuda[ty][tx]) * Rz_1);

        }
        __syncthreads();
        if(i==iteration-1)
            break;
        if(computed)	 //Assign the computation range
            temp_on_cuda[ty][tx]= temp_t[ty][tx];
        __syncthreads();
      }

  // update the global memory
  // after the last iteration, only threads coordinated within the 
  // small block perform the calculation and switch on ``computed''
  if (computed){
      temp_dst[index]= temp_t[ty][tx];		
  }
}

/*
   compute N time steps
*/

int compute_tran_temp(float *MatrixPower, float *MatrixTemp[2], long col, long row, \
		long total_iterations, long num_iterations, long blockCols, long blockRows, long borderCols, long borderRows) 
{
        dim3 dimBlock(BLOCK_SIZE, BLOCK_SIZE);
        dim3 dimGrid(blockCols, blockRows);  
	
	float grid_height = chip_height / row;
	float grid_width = chip_width / col;

	float Cap = FACTOR_CHIP * SPEC_HEAT_SI * t_chip * grid_width * grid_height;
	float Rx = grid_width / (2.0 * K_SI * t_chip * grid_height);
	float Ry = grid_height / (2.0 * K_SI * t_chip * grid_width);
	float Rz = t_chip / (K_SI * grid_height * grid_width);

	float max_slope = MAX_PD / (FACTOR_CHIP * t_chip * SPEC_HEAT_SI);
	float step = PRECISION / max_slope;
	float t;

    float time_elapsed;
	time_elapsed = 0.001;

    int src = 1, dst = 0;
	
	for (t = 0; t < total_iterations; t += num_iterations) 
    {
        int temp = src;
        src = dst;
        dst = temp;
        calculate_temp<<< dimGrid, dimBlock >>>(MIN(num_iterations, total_iterations-t), MatrixPower, MatrixTemp[src], MatrixTemp[dst], \
            col, row, borderCols, borderRows, Cap, Rx, Ry, Rz, step, time_elapsed);
	}
    CUDA_CALL_SAFE(cudaDeviceSynchronize());
    return dst;
}

void usage(int argc, char **argv)
{
	fprintf(stderr, "Usage: %s <grid_rows/grid_cols> <pyramid_height> <sim_time> <temp_file> <power_file> <output_file>\n", argv[0]);
	fprintf(stderr, "\t<grid_rows/grid_cols>  - number of rows/cols in the grid (positive integer)\n");
	fprintf(stderr, "\t<pyramid_height> - pyramid heigh(positive integer)\n");
	fprintf(stderr, "\t<sim_time>   - number of iterations\n");
	fprintf(stderr, "\t<temp_file>  - name of the file containing the initial temperature values of each cell\n");
	fprintf(stderr, "\t<power_file> - name of the file containing the dissipated power values of each cell\n");
	fprintf(stderr, "\t<output_file> - name of the output file\n");
	exit(1);
}

int main(int argc, char** argv)
{
    printf("WG size of kernel = %d X %d\n", BLOCK_SIZE, BLOCK_SIZE);

    run(argc,argv);

    return EXIT_SUCCESS;
}

void run(int argc, char **argv)
{
    size_t size;
    long grid_rows, grid_cols;
    float *FilesavingTemp, *FilesavingPower, *MatrixOut; 
    char *tfile, *pfile, *ofile;
    
    long total_iterations = 60;
    long pyramid_height = 1; // number of iterations

    struct timeval tv_start, tv_end;
    double kernel_time = 0;       // in ms
    double writefile_time = 0;       // in ms
    double readfile_time = 0;       // in ms
    double d2h_memcpy_time = 0;       // in ms
    double h2d_memcpy_time = 0;       // in ms
	
	if (argc != 7)
		usage(argc, argv);
	if((grid_rows = atol(argv[1]))<=0||
	   (grid_cols = atol(argv[1]))<=0||
       (pyramid_height = atoi(argv[2]))<=0||
       (total_iterations = atoi(argv[3]))<=0)
		usage(argc, argv);
		
	tfile = argv[4];
    pfile = argv[5];
    ofile = argv[6];
	
    size = grid_rows * grid_cols;

    /* --------------- pyramid parameters --------------- */
    # define EXPAND_RATE 2// add one iteration will extend the pyramid base by 2 per each borderline
    long borderCols = (pyramid_height)*EXPAND_RATE/2;
    long borderRows = (pyramid_height)*EXPAND_RATE/2;
    long smallBlockCol = BLOCK_SIZE-(pyramid_height)*EXPAND_RATE;
    long smallBlockRow = BLOCK_SIZE-(pyramid_height)*EXPAND_RATE;
    long blockCols = grid_cols/smallBlockCol+((grid_cols%smallBlockCol==0)?0:1);
    long blockRows = grid_rows/smallBlockRow+((grid_rows%smallBlockRow==0)?0:1);

    FilesavingTemp = (float *)malloc(size * sizeof(float));
    FilesavingPower = (float *)malloc(size * sizeof(float));
    MatrixOut = (float *)calloc(size, sizeof(float));

    if(!FilesavingPower || !FilesavingTemp || !MatrixOut)
    {
        fprintf(stderr, "unable to allocate memory\n");
        abort();
        exit(EXIT_FAILURE);
    }

    printf("pyramidHeight: %d\ngridSize: [%d, %d]\nborder:[%d, %d]\nblockGrid:[%d, %d]\ntargetBlock:[%d, %d]\n",\
        pyramid_height, grid_cols, grid_rows, borderCols, borderRows, blockCols, blockRows, smallBlockCol, smallBlockRow);
	
    gettimeofday(&tv_start, NULL);
    readinput(FilesavingTemp, grid_rows, grid_cols, tfile);
    readinput(FilesavingPower, grid_rows, grid_cols, pfile);
    gettimeofday(&tv_end, NULL);
    readfile_time += time_diff(tv_start, tv_end);

    float *MatrixTemp[2], *MatrixPower;
    CUDA_CALL_SAFE(cudaMalloc((void **)&MatrixTemp[0], sizeof(float) * size));
    CUDA_CALL_SAFE(cudaMalloc((void **)&MatrixTemp[1], sizeof(float) * size));

    gettimeofday(&tv_start, NULL);
    CUDA_CALL_SAFE(cudaMemcpy(MatrixTemp[0], FilesavingTemp, sizeof(float) * size, cudaMemcpyHostToDevice));
    gettimeofday(&tv_end, NULL);
    h2d_memcpy_time += time_diff(tv_start, tv_end);

    CUDA_CALL_SAFE(cudaMalloc((void **)&MatrixPower, sizeof(float) * size));
    gettimeofday(&tv_start, NULL);
    CUDA_CALL_SAFE(cudaMemcpy(MatrixPower, FilesavingPower, sizeof(float) * size, cudaMemcpyHostToDevice));
    gettimeofday(&tv_end, NULL);
    h2d_memcpy_time += time_diff(tv_start, tv_end);

    printf("Start computing the transient temperature\n");

    gettimeofday(&tv_start, NULL);
    int ret = compute_tran_temp(MatrixPower,MatrixTemp,grid_cols,grid_rows, \
	    total_iterations,pyramid_height, blockCols, blockRows, borderCols, borderRows);
    gettimeofday(&tv_end, NULL);
    kernel_time += time_diff(tv_start, tv_end);

	printf("Ending simulation\n");
    gettimeofday(&tv_start, NULL);
    CUDA_CALL_SAFE(cudaMemcpy(MatrixOut, MatrixTemp[ret], sizeof(float) * size, cudaMemcpyDeviceToHost));
    gettimeofday(&tv_end, NULL);
    d2h_memcpy_time += time_diff(tv_start, tv_end);

    gettimeofday(&tv_start, NULL);
    writeoutput(MatrixOut, grid_rows, grid_cols, ofile);
    gettimeofday(&tv_end, NULL);
    writefile_time += time_diff(tv_start, tv_end);

    CUDA_CALL_SAFE(cudaFree(MatrixPower));
    CUDA_CALL_SAFE(cudaFree(MatrixTemp[0]));
    CUDA_CALL_SAFE(cudaFree(MatrixTemp[1]));
    free(MatrixOut);

    printf("==> header: kernel_time (ms),writefile_time (ms),d2h_memcpy_time (ms),readfile_time (ms),h2d_memcpy_time (ms)\n");
    printf("==> data: %f,%f,%f,%f,%f\n", kernel_time, writefile_time, d2h_memcpy_time, readfile_time, h2d_memcpy_time);
}
