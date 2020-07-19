#ifndef __H_BENCH__
#define __H_BENCH__
#include "../include/settings.h"
#include "../include/container.h"
#include "measurement.h"
#include "../interface/interface.h"
#include <stdio.h>
#include <assert.h>
#include <stdlib.h>
#include <string.h>

#define PRINTPER 1
#define ALGOTYPE 300
#define LOWERTYPE 300
#define BENCHNUM 16

#ifdef CDF
#define TIMESLOT 10 //micro sec
#endif

#define GET_VALUE_SIZE \
	(((VALUESIZE==-1)?(rand()%(NPCINPAGE)-1)+1:VALUESIZE)*PIECE)

#define BENCHSETSIZE (1024+1)

// host request 
typedef struct{
	FSTYPE type;
	KEYT key;
	V_PTR value;
	uint32_t range;
	uint32_t length;
	int mark;
}bench_op;

typedef struct{
	uint32_t start;
	uint32_t end;
	uint64_t ops;
	bench_type type;
//#ifdef EUNJI
	char trc_fname[100];
	FILE* trc_fp;
//#endif

}bench_t;
//}bench_t;

typedef struct{
	uint64_t total_micro;
	uint64_t cnt;
	uint64_t max;
	uint64_t min;
}bench_ftl_time;

typedef struct{
	uint64_t algo_mic_per_u100[12];
	uint64_t lower_mic_per_u100[12];
	uint64_t algo_sec,algo_usec;
	uint64_t lower_sec,lower_usec;
#ifdef CDF
	uint64_t write_cdf[1000000/TIMESLOT+1];
	uint64_t read_cdf[1000000/TIMESLOT+1];
#endif
	uint64_t read_cnt,write_cnt;
	bench_ftl_time ftl_poll[ALGOTYPE][LOWERTYPE];
	MeasureTime bench;
}bench_stat_t;


// bench 
typedef struct{
	bench_op *body[BENCHSETSIZE];
	bench_op **dbody;

	uint32_t bech;
	uint32_t benchsetsize;
	uint64_t nth_bench;

	volatile uint64_t tot_ops;//request to be thrown 
	volatile uint64_t issued_ops;//request throw num
	volatile uint64_t completed_ops;//request end num
	//volatile uint64_t m_num;//request to be thrown 
	//volatile uint64_t n_num;//request throw num
	//volatile uint64_t r_num;//request end num
	bool finish;
	bool empty;
//	bool ondemand;
	int mark;
	uint64_t notfound;
	uint64_t write_cnt;
	uint64_t read_cnt;

	bench_type type;
	MeasureTime benchTime;
	MeasureTime benchTime2;
	uint64_t cache_hit;

} bench_monitor_t;

// bench has multiple workloads. 
typedef struct{
	int bench_num;
	//int n_fill;
	//int n_curr;

	// workload set 
	bench_t *bench;
	bench_monitor_t *bench_mon;
	bench_stat_t *bench_stat;
	lower_info *li;
	uint32_t error_cnt;
}bench_master_t;

void bench_init();
void bench_add(bench_type type,uint32_t start, uint32_t end,uint64_t number, char* trc_fname);
//bench_op* get_bench();

void bench_refresh(bench_type, uint32_t start, uint32_t end, uint64_t number);
void bench_free();

void bench_print();
void bench_li_print(lower_info *,bench_monitor_t *);
bool bench_is_finish_n(int n);
bool bench_is_finish();

void bench_cache_hit(int mark);
void bench_reap_data(request *const,lower_info *);
void bench_reap_nostart(request *const);
char *bench_lower_type(int);

void bench_custom_init(MeasureTime *mt, int idx);
void bench_custom_start(MeasureTime *mt,int idx);
void bench_custom_A(MeasureTime *mt,int idx);
void bench_custom_print(MeasureTime *mt, int idx);
int bench_set_params(int argc, char **argv,char **targv);
//bench_op* get_bench_with_synth();	// EUNJI
void do_bench_with_synth(int idx); // EUNJI 
void do_bench_with_trace(int idx); // EUNJI 
void run_bench();

#ifdef CDF
void bench_cdf_print(uint64_t, uint8_t istype, bench_stat_t*);
#endif
void bench_update_ftltime(bench_stat_t *_d, request *const req);
void bench_type_cdf_print(bench_stat_t *_d);
void free_bnech_all();
void free_bench_one(bench_op *);
#endif

void seqget(uint32_t, uint32_t,bench_monitor_t *);
void seqset(uint32_t,uint32_t,bench_monitor_t*);
void seqrw(uint32_t,uint32_t,bench_monitor_t *);
void randget(uint32_t,uint32_t,bench_monitor_t*);
void fillrand(uint32_t,uint32_t,bench_monitor_t*);
void randset(uint32_t,uint32_t,bench_monitor_t*);
void randrw(uint32_t,uint32_t,bench_monitor_t*);
void mixed(uint32_t,uint32_t,int percentage,bench_monitor_t*);
int my_itoa(uint32_t key, char **_target);


