#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <limits.h>
#include "../include/FS.h"
#include "../include/settings.h"
#include "../include/types.h"
#include "../bench/bench.h"
#include "interface.h"
#include "../include/utils/kvssd.h"

int main(int argc,char* argv[]){
	/*
	   to use the custom benchmark setting the first parameter of 'inf_init' set false
	   if not, set the parameter as true.
	   the second parameter is not used in anycase.
	 */
	inf_init(0,0,0,NULL);
	
	/*initialize the cutom bench mark*/
	bench_init(); // number of benches 

	/*adding benchmark type for testing*/
//#ifdef EUNJI
	char trc_fname[100];
	strcpy(trc_fname, "./trace/test.trc");

	//bench_add(RANDSET,0,RANGE,RANGE*2, NULL);
//	bench_add(RANDSET,0,100,200, NULL);
//	bench_add(TRACE,0,0,0, trc_fname);
	bench_add(TRACE, 0, RANGE, 0, trc_fname);
	run_bench();

	// 어차피 get_bench 에서 type 에 따라 다르게 읽어옴. 

#if 0

	bench_t bench;
	int idx = 0;

	while(bench = get_bench(idx))
	{
		


	}

#endif

 

#if 0
	bench_op *op;
	while((op=get_bench())){
		if(op->type==FS_SET_T){
			inf_make_req(op->type,op->key,NULL,op->length,op->mark);
		}
		else if(op->type==FS_GET_T){
			inf_make_req(op->type,op->key,NULL,op->length,op->mark);
		}
	}
//#endif
#endif

	inf_free();
	return 0;
}
