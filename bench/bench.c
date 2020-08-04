#include "bench.h"
#include "../include/types.h"
#include "../include/settings.h"
#include "../include/utils/kvssd.h"
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <getopt.h>
int32_t LOCALITY;
float TARGETRATIO;
float OVERLAP;
int KEYLENGTH;
int VALUESIZE;
bool last_ack;
int seq_padding_opt;
extern int32_t write_stop;

bench_master_t *bm;
#ifndef KVSSD
void latency(uint32_t,uint32_t,bench_monitor_t*);

void rand_latency(uint32_t start, uint32_t end,int percentage, bench_monitor_t *m);
void seq_latency(uint32_t start, uint32_t end,int percentage, bench_monitor_t *m);
#endif

uint32_t keygenerator(uint32_t range);
uint32_t keygenerator_type(uint32_t range, int type);
pthread_mutex_t bench_lock;


uint8_t *bitmap;
static void bitmap_set(uint32_t key){
	uint32_t block=key/8;
	uint8_t offset=key%8;

	bitmap[block]|=(1<<offset);
}

static bool bitmap_get(uint32_t key){
	uint32_t block=key/8;
	uint8_t offset=key%8;

	return bitmap[block]&(1<<offset);
}

// bench_init 
//void bench_init(int bench_num){
void bench_init(){

	OVERLAP=0.0;
	bm=(bench_master_t*)malloc(sizeof(bench_master_t));
	bm->bench_mon=(bench_monitor_t*)malloc(sizeof(bench_monitor_t)*BENCHNUM);
	memset(bm->bench_mon,0,sizeof(bench_monitor_t)*BENCHNUM);

	bm->bench=(bench_t*)malloc(sizeof(bench_t) * BENCHNUM);
	memset(bm->bench,0,sizeof(bench_t)*BENCHNUM);

	bm->bench_stat=(bench_stat_t*)malloc(sizeof(bench_stat_t) * BENCHNUM);
	memset(bm->bench_stat,0,sizeof(bench_stat_t)*BENCHNUM);

	bm->li=(lower_info*)malloc(sizeof(lower_info)*BENCHNUM);
	memset(bm->li,0,sizeof(lower_info)*BENCHNUM);

	bm->bench_num=0; 
	//bm->n_curr=0; 
	pthread_mutex_init(&bench_lock,NULL);

	for(int i=0; i<BENCHNUM; i++){
		bm->bench_mon[i].empty=true;
		bm->bench_mon[i].type=NOR;
	}
	
	bitmap=(uint8_t*)malloc(sizeof(uint8_t)*(RANGE));
	bm->error_cnt=0;
}

// EUNJI
//void add_bench(bench_type type, uint32_t start, uint32_t end, uint64_t ops){
void bench_add(bench_type type, uint32_t start, uint32_t end, uint64_t ops, char* trc_fname)
{
	int idx = bm->bench_num;
	bm->bench_num++;

	bm->bench[idx].type = type;
	bm->bench[idx].start = start;
	bm->bench[idx].end = end;

	// trace-driven 
	if(type == TRACE){
		char cmd[20];
		uint32_t lba;
		int ops = 0;

		assert(trc_fname);
		strcpy(bm->bench[idx].trc_fname, trc_fname);

		// file open
		FILE* fp;
		if(!(fp = fopen(trc_fname, "r"))){
			printf("Failed to open file %s.\n", trc_fname);
			return;
		}
		bm->bench[idx].trc_fp = fp;

		fseek(fp, 0, SEEK_SET);
		while(fscanf(fp, "%s %d\n", cmd, &lba) != EOF) ops++;
		fseek(fp, 0, SEEK_SET);

		bm->bench[idx].ops = ops;
	}
	// synthetic 
	else {
		bm->bench[idx].ops = ops%2?(ops/2)*2:ops;
	}

	for(int j=0;j<ALGOTYPE;j++){
		for(int k=0;k<LOWERTYPE;k++){
			bm->bench_stat[idx].ftl_poll[j][k].min = UINT64_MAX;
			//bm->bench_stat[i].ftl_npoll[j][k].min = UINT64_MAX;
		}
	}
	if(type==NOR){
		bench_t *_bench = &bm->bench[idx];
		bench_monitor_t *_bmon = &bm->bench_mon[idx];

		_bmon->issued_ops=0;
		_bmon->completed_ops=0;
		_bmon->tot_ops=_bench->ops;
		_bmon->type=_bench->type;
	}

#ifndef KVSSD
	printf("bench range:%u ~ %u\n",start,end);
#endif
}


// bench_free 
void bench_free(){
	free(bm->bench_mon);
	free(bm->bench);
	free(bm->bench_stat);
	free(bm->li);
	free(bm);
}


#if 0
bench_op *bench_make_ondemand(){
	int idx=bm->n_curr;
	bench_t *_meta=&bm->bench[idx];
	bench_monitor_t * _m=&bm->bench_mon[idx];

	if(!_m->tot_ops){
		_m->issued_ops=0;
		_m->completed_ops=0;
		_m->empty=false;
		_m->tot_ops=_meta->ops;
		_m->ondemand=true;
		_m->type=_meta->type;
		_m->dbody=(bench_op**)malloc(sizeof(bench_op*)*_meta->ops);
		measure_init(&_m->benchTime);
		MS(&_m->benchTime);
	}
	
	bench_op *op=(bench_op*)malloc(sizeof(bench_op));
	_m->dbody[_m->issued_ops]=op;
	uint32_t start=_meta->start;
	uint32_t end=_meta->end;
	uint32_t t_k=0;
	switch(_meta->type){
		case FILLRAND:
			abort();
			break;
		case SEQSET:
				op->type=FS_SET_T;
				op->length=LPAGESIZE;
				t_k=start+(_m->issued_ops%(end-start));
				break;
		case SEQGET:
				op->type=FS_GET_T;
				op->length=LPAGESIZE;
				t_k=start+(_m->issued_ops%(end-start));
				break;
		case RANDSET:
				op->type=FS_SET_T;
				op->length=LPAGESIZE;
				t_k=start+rand()%(end-start);
				break;
		case RANDGET:
				op->type=FS_GET_T;
				op->length=LPAGESIZE;
				t_k=start+rand()%(end-start);
				break;

		default:
				break;
	}
	op->mark=idx;
#ifdef KVSSD
	op->key.len=my_itoa(t_k,&op->key.key);
#else
	op->key=t_k;
#endif
	return op;
}
#endif

bench_op *get_bench_with_synth(){
#if 0
	bench_op *op;
	bench_monitor_t * bmon;
	int idx; 


	// run benchmarks 
	for (idx = 0; idx < bm->bench_num; idx++) {

		bmon = &bm->bench_mon[idx];
		assert(bmon);

		bmon->issued_ops = 0;
		bmon->completed_ops = 0;
		bmon->empty = false;
		bmon->tot_ops = bm->ops;
		bmon->ondemand = true;
		bmon->type = bm->type;
		bmon->dbody=(bench_op**)malloc(sizeof(bench_op*)*bm->ops);
		measure_init(&bmon->benchTime);
		MS(&bmon->benchTime);


		while(1) {

			// check finished 
			if(bmon->issued_ops == bmon->tot_ops){
				while(!bench_is_finish_n(bm->n_curr)){
						write_stop = false;
				}
				printf("\rtesting...... [100%%] done!\n");
				printf("\n");
				//sleep(5);

				free(mon->dbody);

				break;
			}


#if 0
	if(_m->tot_ops<100){
		float body=_m->tot_ops;
		float head=_m->issued_ops;
		printf("\r testing.....[%f%%]",head/body*100);
	}
	else if(_m->issued_ops%(_m->tot_ops<100?_m->tot_ops:PRINTPER*(_m->tot_ops/10000))==0){
#ifdef PROGRESS
		printf("\r testing...... [%.2lf%%]",(double)(_m->issued_ops)/(_m->tot_ops/100));
		fflush(stdout);
#endif
	}
#endif
    if (_m->issued_ops == _m->tot_ops -1) {
        last_ack = true;
    }

		op = bench_make_ondemand();
		mon->issued_ops++;

		return op;
#endif
	return NULL;
}

//EUNJI 
void do_bench_with_synth(int idx)
{

}


//bench_op* get_bench_with_trace()
void do_bench_with_trace(int idx)
{
	printf("do_bench_with_trace\n");

	bench_t* bench = &bm->bench[idx];
	bench_monitor_t* bmon = &bm->bench_mon[idx];
	char cmd[10];
	uint32_t lba;
	FILE*fp = bench->trc_fp;
	
	bmon->issued_ops = 0;
	bmon->completed_ops = 0;
	bmon->empty = false;
	bmon->tot_ops = bench->ops;
//	bmon->ondemand = true;
	bmon->type = bench->type;
	//bmon->dbody=(bench_op**)malloc(sizeof(bench_op*)*bm->ops);
	measure_init(&bmon->benchTime);
	MS(&bmon->benchTime);

	while(!(fscanf(fp, "%s %d\n", cmd, &lba) == EOF)) 
	{
#ifdef EUNJI_DEBUG
		printf("%s %d\n", cmd, lba);
#endif
		uint32_t key = (lba + bench->start) % (bench->end);
		uint32_t length = PAGESIZE;
		int mark = idx;

		if(cmd[0] == 'R')	{
			inf_make_req(FS_GET_T, key, NULL, length, mark);
		}
		else if(cmd[0] == 'W') {
			inf_make_req(FS_SET_T, key, NULL, length, mark);
		}
#if 0
		else if(cmd[0] == 'S') op.type = FS_FLUSH_T;
		else {
			printf("Unknown cmd: %s\n", cmd);
			continue;
		}
#endif

		bmon->issued_ops++;

	}
	return;
}

// run_bench 
void run_bench()
{
	for(int i = 0; i < bm->bench_num; i++) {
		bench_t* bench = &bm->bench[i];
		if (bench->type == TRACE)
			do_bench_with_trace(i);
		else
			do_bench_with_synth(i);
	}
	return;
}


extern bool force_write_start;
bool bench_is_finish_n(volatile int n){
	if(bm->bench_mon[n].completed_ops==bm->bench_mon[n].tot_ops){
		bm->bench_mon[n].finish=true;
		return true;
	}
	
	if(n+1==bm->bench_num){
	//	force_write_start=true;	
		write_stop=0;
	}
	return false;
}

bool bench_is_finish(){
	for(int i=0; i<bm->bench_num; i++){
		if(!bench_is_finish_n(i)){
			return false;
		}
	}
	return true;
}
void bench_print(){
	bench_stat_t *bdata=NULL;
	bench_monitor_t *_m=NULL;
	for(int i=0; i<bm->bench_num; i++){
		printf("\n\n");
		_m=&bm->bench_mon[i];
		bdata=&bm->bench_stat[i];
#ifdef CDF
		bench_cdf_print(_m->tot_ops,_m->type,bdata);
#endif
		bench_type_cdf_print(bdata);
		
		printf("--------------------------------------------\n");
		printf("|            bench type:                   |\n");
		printf("--------------------------------------------\n");
#ifdef BENCH
#if 0
		printf("----algorithm----\n");
		for(int j=0; j<10; j++){
			printf("[%d~%d(usec)]: %ld\n",j*100,(j+1)*100,bdata->algo_mic_per_u100[j]);
		}
		printf("[over_ms]: %ld\n",bdata->algo_mic_per_u100[10]);
		printf("[over__s]: %ld\n",bdata->algo_mic_per_u100[11]);


		printf("----lower----\n");
		for(int j=0; j<10; j++){
			printf("[%d~%d(usec)]: %ld\n",j*100,(j+1)*100,bdata->lower_mic_per_u100[j]);
		}
		printf("[over_ms]: %ld\n",bdata->lower_mic_per_u100[10]);
		printf("[over__s]: %ld\n",bdata->lower_mic_per_u100[11]);

		printf("----average----\n");
		uint64_t total=0,avg_sec=0, avg_usec=0;
		total=(bdata->algo_sec*1000000+bdata->algo_usec)/_m->tot_ops;
		avg_sec=total/1000000;
		avg_usec=total%1000000;
		printf("[avg_algo]: %ld.%ld\n",avg_sec,avg_usec);

		total=(bdata->lower_sec*1000000+bdata->lower_usec)/_m->tot_ops;
		avg_sec=total/1000000;
		avg_usec=total%1000000;
		printf("[avg_low]: %ld.%ld\n",avg_sec,avg_usec);
		bench_li_print(&bm->li[i],_m);
#endif
#endif
		printf("\n----summary----\n");
		if(_m->type==RANDRW || _m->type==SEQRW){
			uint64_t total_data=(PAGESIZE * _m->tot_ops/2)/1024;
			double total_time2=_m->benchTime.adding.tv_sec+(double)_m->benchTime.adding.tv_usec/1000000;
			double total_time1=_m->benchTime2.adding.tv_sec+(double)_m->benchTime2.adding.tv_usec/1000000;
			double throughput1=(double)total_data/total_time1;
			double throughput2=(double)total_data/total_time2;
			double sr=1-((double)_m->notfound/_m->tot_ops);
			throughput2*=sr;

			printf("[all_time1]: %ld.%ld\n",_m->benchTime.adding.tv_sec,_m->benchTime.adding.tv_usec);
			printf("[all_time2]: %ld.%ld\n",_m->benchTime2.adding.tv_sec,_m->benchTime2.adding.tv_usec);
			printf("[size]: %lf(mb)\n",(double)total_data/1024);

			printf("[FAIL NUM] %ld\n",_m->notfound);
			fprintf(stderr,"[SUCCESS RATIO] %lf\n",sr);
			fprintf(stderr,"[throughput1] %lf(kb/s)\n",throughput1);
			fprintf(stderr,"             %lf(mb/s)\n",throughput1/1024);
			fprintf(stderr,"[IOPS 1] %lf\n",_m->tot_ops/total_time1/2);
			fprintf(stderr,"[throughput2] %lf(kb/s)\n",throughput2);
			fprintf(stderr,"             %lf(mb/s)\n",throughput2/1024);
			fprintf(stderr,"[IOPS 2] %lf\n",_m->tot_ops/total_time2/2);
			fprintf(stderr,"[cache hit cnt,ratio] %ld, %lf\n",_m->cache_hit,(double)_m->cache_hit/(_m->tot_ops/2));
			printf("[READ WRITE CNT] %ld %ld\n",_m->read_cnt,_m->write_cnt);
		}
		else{
			_m->benchTime.adding.tv_sec+=_m->benchTime.adding.tv_usec/1000000;
			_m->benchTime.adding.tv_usec%=1000000;
			printf("[all_time]: %ld.%ld\n",_m->benchTime.adding.tv_sec,_m->benchTime.adding.tv_usec);
			uint64_t total_data=(PAGESIZE * _m->tot_ops)/1024;
			printf("[size]: %lf(mb)\n",(double)total_data/1024);
			double total_time=_m->benchTime.adding.tv_sec+(double)_m->benchTime.adding.tv_usec/1000000;
			double throughput=(double)total_data/total_time;
			double sr=1-((double)_m->notfound/_m->tot_ops);
			throughput*=sr;
			printf("[FAIL NUM] %ld\n",_m->notfound);
			fprintf(stderr,"[SUCCESS RATIO] %lf\n",sr);
			fprintf(stderr,"[throughput] %lf(kb/s)\n",throughput);
			fprintf(stderr,"             %lf(mb/s)\n",throughput/1024);
			printf("[IOPS] %lf\n",_m->tot_ops/total_time);
			if(_m->read_cnt){
				printf("[cache hit cnt,ratio] %ld, %lf\n",_m->cache_hit,(double)_m->cache_hit/(_m->read_cnt));
				printf("[cache hit cnt,ratio dftl] %ld, %lf\n",_m->cache_hit,(double)_m->cache_hit/(_m->read_cnt+_m->write_cnt));
			}
			printf("[READ WRITE CNT] %ld %ld\n",_m->read_cnt,_m->write_cnt);
		}
		printf("error cnt:%d\n",bm->error_cnt);
	}
}

void bench_update_typetime(bench_stat_t *_d, uint8_t a_type,uint8_t l_type, uint64_t time){
	bench_ftl_time *temp;
	temp = &_d->ftl_poll[a_type][l_type];
	temp->total_micro += time;
	temp->max = temp->max < time ? time : temp->max;
	temp->min = temp->min > time ? time : temp->min;
	temp->cnt++;
}

void bench_type_cdf_print(bench_stat_t *_d){
	fprintf(stderr,"a_type\tl_type\tmax\tmin\tavg\tcnt\tpercentage\n");
	for(int i = 0; i < ALGOTYPE; i++){
		for(int j = 0; j < LOWERTYPE; j++){
			if(!_d->ftl_poll[i][j].cnt)
				continue;
			fprintf(stderr,"%d\t%d\t%lu\t%lu\t%.3f\t%lu\t%.5f%%\n",i,j,_d->ftl_poll[i][j].max,_d->ftl_poll[i][j].min,(float)_d->ftl_poll[i][j].total_micro/_d->ftl_poll[i][j].cnt,_d->ftl_poll[i][j].cnt,(float)_d->ftl_poll[i][j].cnt/_d->read_cnt*100);
		}
	}
}

void __bench_time_maker(MeasureTime mt, bench_stat_t *bench_stat,bool isalgo){
	uint64_t *target=NULL;
	if(isalgo){
		target=bench_stat->algo_mic_per_u100;
		bench_stat->algo_sec+=mt.adding.tv_sec;
		bench_stat->algo_usec+=mt.adding.tv_usec;
	}
	else{
		target=bench_stat->lower_mic_per_u100;
		bench_stat->lower_sec+=mt.adding.tv_sec;
		bench_stat->lower_usec+=mt.adding.tv_usec;
	}

	if(mt.adding.tv_sec!=0){
		target[11]++;
		return;
	}

	int idx=mt.adding.tv_usec/1000;
	if(idx>=10){
		target[10]++;
		return;
	}
	idx=mt.adding.tv_usec/1000;
	if(target){
		target[idx]++;
	}
	return;
}

#ifdef CDF
void bench_cdf_print(uint64_t nor, uint8_t type, bench_stat_t *_d){//number of reqeusts
	uint64_t cumulate_number=0;
	if(type>RANDSET)
		nor/=2;
/*	if((type>RANDSET || type%2==1) || type==NOR){
		printf("\n[cdf]write---\n");
		for(int i=0; i<1000000/TIMESLOT+1; i++){
			cumulate_number+=_d->write_cdf[i];
			if(_d->write_cdf[i]==0) continue;
			//printf("%d\t%ld\t%f\n",i * 10,_d->write_cdf[i],(float)cumulate_number/_d->write_cnt);
			if(nor==cumulate_number)
				break;
		}	
	} */
	static int cnt=0;
	cumulate_number=0;
	if((type>RANDSET || type%2==0) || type==NOR || type==FILLRAND){
		printf("\n(%d)[cdf]read---\n",cnt++);
		for(int i=0; i<1000000/TIMESLOT+1; i++){
			cumulate_number+=_d->read_cdf[i];
			if(_d->read_cdf[i]==0) continue;
			fprintf(stderr,"%d,%ld,%f\n",i * 10,_d->read_cdf[i],(float)cumulate_number/_d->read_cnt);	
			if(nor==cumulate_number)
				break;
		}
	}
}
#endif
void bench_reap_nostart(request *const req){
	pthread_mutex_lock(&bench_lock);
	int idx=req->mark;
	bench_monitor_t *_m=&bm->bench_mon[idx];
	_m->completed_ops++;
	//static int cnt=0;
	pthread_mutex_unlock(&bench_lock);
}


// call-back function 
void bench_reap_data(request *const req,lower_info *li){
	//for cdf
#ifdef CDF
	measure_calc(&req->latency_checker);
#endif

	pthread_mutex_lock(&bench_lock);

	if(!req){ 
		pthread_mutex_unlock(&bench_lock);
		return;
	}

	// identify op  
	int idx = req->mark;
	bench_monitor_t *bmon = &bm->bench_mon[idx];
	bench_stat_t *bstat = &bm->bench_stat[idx];

	if(req->type == FS_GET_T || req->type == FS_NOTFOUND_T){
		bench_update_typetime(bstat, req->type_ftl, req->type_lower,req->latency_checker.micro_time);
	}
	
	if(req->type==FS_NOTFOUND_T){
		//bm->error_cnt++;
		bmon->notfound++;
	}

	// completion 
	bmon->completed_ops++;

	if(bmon->tot_ops == bmon->completed_ops){
		MA(&bmon->benchTime);
		bstat->bench = bmon->benchTime;
		memcpy(&bm->li[idx],li,sizeof(lower_info));
		li->refresh(li);
	}
	if(bmon->completed_ops==bmon->tot_ops/2 && (bmon->type==SEQRW || bmon->type==RANDRW)){
		MA(&bmon->benchTime);
		bmon->benchTime2=bmon->benchTime;
		measure_init(&bmon->benchTime);
		MS(&bmon->benchTime);
	}

//	if(bmon->ondemand){
//		free(bmon->dbody[bmon->completed_ops]);
	pthread_mutex_unlock(&bench_lock);


#if 0
#ifdef CDF
	int slot_num=req->latency_checker.micro_time/TIMESLOT;
	if(req->type==FS_GET_T || req->type==FS_RANGEGET_T){
		if(slot_num>=1000000/TIMESLOT){
			bstat->read_cdf[1000000/TIMESLOT]++;
		}
		else{
			bstat->read_cdf[slot_num]++;
		}
		if(bmon->completed_ops%1000000==0){
		//	bench_cdf_print(bmon->completed_ops,bmon->type,bstat);
		}
	}
	else if(req->type==FS_SET_T){
		if(slot_num>=1000000/TIMESLOT){
			bstat->write_cdf[1000000/TIMESLOT]++;
		}
		else{
			bstat->write_cdf[slot_num]++;
		}
	}
#endif

	if(bmon->empty){
		if(req->type==FS_GET_T || req->type==FS_RANGEGET_T){
			bmon->read_cnt++;
			bstat->read_cnt++;
		}
		else if(req->type==FS_SET_T){
			bmon->write_cnt++;
			bstat->write_cnt++;
		}
		bmon->completed_ops++;
		bmon->tot_ops++;
		pthread_mutex_unlock(&bench_lock);
		return;
	}

	if(bmon->tot_ops == bmon->completed_ops+1){
		bstat->bench = bmon->benchTime;
	}
	if(bmon->completed_ops+1==bmon->tot_ops/2 && (bmon->type==SEQRW || bmon->type==RANDRW)){
		MA(&bmon->benchTime);
		bmon->benchTime2=bmon->benchTime;
		measure_init(&bmon->benchTime);
		MS(&bmon->benchTime);
	}

	if(req->type==FS_NOTFOUND_T){
		bmon->notfound++;
	}
	if(bmon->tot_ops==bmon->completed_ops+1){
#ifdef BENCH
		memcpy(&bm->li[idx],li,sizeof(lower_info));
		li->refresh(li);
#endif
		MA(&bmon->benchTime);
	}
	if(bmon->ondemand){
		free(bmon->dbody[bmon->completed_ops]);
	}
	bmon->completed_ops++;
	pthread_mutex_unlock(&bench_lock);

#endif
}


void bench_li_print(lower_info* li,bench_monitor_t *m){
	printf("-----lower_info----\n");
	printf("[write_op]: %ld\n",li->write_op);
	printf("[read_op]: %ld\n",li->read_op);
	printf("[trim_op]:%ld\n",li->trim_op);
	printf("[WAF, RAF]: %lf, %lf\n",(float)li->write_op/m->tot_ops,(float)li->read_op/m->tot_ops);
	printf("[if rw test]: %lf(WAF), %lf(RAF)\n",(float)li->write_op/(m->tot_ops/2),(float)li->read_op/(m->tot_ops/2));
}

uint32_t keygenerator(uint32_t range){
	if(rand()%100<LOCALITY){
		return rand()%(uint32_t)(range*TARGETRATIO);
	}
	else{
		return (rand()%(uint32_t)(range*(1-TARGETRATIO)))+(uint32_t)RANGE*TARGETRATIO;
	}
}

uint32_t keygenerator_type(uint32_t range,int type){
	uint32_t write_shift=(uint32_t)(range*TARGETRATIO*(1.0f-OVERLAP));
	uint32_t res=0;
	if(rand()%100<LOCALITY){
		res=rand()%(uint32_t)(range*TARGETRATIO)+(type==FS_SET_T?write_shift:0);
	}else{
		res=(rand()%(uint32_t)(range*(1-TARGETRATIO)))+(uint32_t)RANGE*TARGETRATIO;
	}
	return res;
}
#ifdef KVSSD

int my_itoa(uint32_t key, char **_target){
	int cnt=1;
	int standard=10;
	int t_key=key;
	while(t_key/10){
		cnt++;
		t_key/=10;
		standard*=10;
	}
	int result=KEYLENGTH==-1?rand()%16+1:KEYLENGTH;
	result*=16;
	result-=sizeof(ppa_t);
	*_target=(char*)malloc(result);
	char *target=*_target;
	t_key=key;

	target[0]='u';
	target[1]='s';
	target[2]='e';
	target[3]='r';
	/*
	for(int i=cnt-1+4; i>=4; i--){
		target[i]=t_key%10+'0';
		t_key/=10;
	}
	for(int i=cnt+4;i<result; i++){
		target[i]='0';
	}*/
	

	
	for(int i=4; i<result-cnt; i++){
		target[i]='0';
	}
	for(int i=result-1; i>=result-cnt; i--){
		target[i]=t_key%10+'0';
		t_key/=10;
	}
	//printf("origin:%d\n",key);
	//printf("%d %s(%d)\n",result,target,strlen(target));
	return result;
}
/*
int my_itoa(uint32_t key, char **_target){
	int cnt=1;
	int standard=10;
	int t_key=key;
	while(t_key/10){
		cnt++;
		t_key/=10;
		standard*=10;
	}
	*_target=(char*)malloc(cnt+1);
	char *target=*_target;
	t_key=key;
	for(int i=cnt-1; i>=0; i--){
		target[i]=t_key%10+'0';
		t_key/=10;
	}
	target[cnt]='\0';
	return cnt;
}*/

int my_itoa_padding(uint32_t key, char **_target,int digit){
	int cnt=1;
	int standard=10;
	int t_key=key;
	while(t_key/10){
		cnt++;
		t_key/=10;
		standard*=10;
	}

	*_target=(char*)malloc(digit+1);
	char *target=*_target;
	t_key=key;
	for(int i=0; i<digit-cnt-1; i++){
		target[i]='0';
	}
	for(int i=digit-1; i>=digit-cnt-1; i--){
		target[i]=t_key%10+'0';
		t_key/=10;
	}
	target[digit]='\0';
	return digit;
}
#endif
void seqget(uint32_t start, uint32_t end,bench_monitor_t *m){
	printf("making seq Get bench!\n");
	for(uint32_t i=0; i<m->tot_ops; i++){
#ifdef KVSSD
		KEYT *t=&m->body[i/m->bech][i%m->bech].key;
		if(seq_padding_opt)t->len=my_itoa_padding(start+(i%(end-start)),&t->key,10);
		else t->len=my_itoa(start+(i%(end-start)),&t->key);
#else
		m->body[i/m->bech][i%m->bech].key=start+(i%(end-start));
#endif
		m->body[i/m->bech][i%m->bech].length=PAGESIZE;
		m->body[i/m->bech][i%m->bech].type=FS_GET_T;
		m->body[i/m->bech][i%m->bech].mark=m->mark;
		m->read_cnt++;
	}
}

void seqset(uint32_t start, uint32_t end,bench_monitor_t *m){
	printf("making seq Set bench!\n");
	for(uint32_t i=0; i<m->tot_ops; i++){
#ifdef KVSSD
		KEYT *t=&m->body[i/m->bech][i%m->bech].key;
		if(seq_padding_opt)t->len=my_itoa_padding(start+(i%(end-start)),&t->key,10);
		else t->len=my_itoa(start+(i%(end-start)),&t->key);
		bitmap_set(start+(i%(end-start)));
#else
		m->body[i/m->bech][i%m->bech].key=start+(i%(end-start));
		bitmap_set(m->body[i/m->bech][i%m->bech].key);
#endif
#ifdef DVALUE
		m->body[i/m->bech][i%m->bech].length=GET_VALUE_SIZE;
#else
		m->body[i/m->bech][i%m->bech].length=PAGESIZE;
#endif
		m->body[i/m->bech][i%m->bech].type=FS_SET_T;
		m->body[i/m->bech][i%m->bech].mark=m->mark;
		m->write_cnt++;
	}
}

void seqrw(uint32_t start, uint32_t end, bench_monitor_t *m){
	printf("making seq Set and Get bench!\n");
	uint32_t i=0;
	for(i=0; i<m->tot_ops/2; i++){
#ifdef KVSSD
		KEYT *t=&m->body[i/m->bech][i%m->bech].key;
		if(seq_padding_opt)t->len=my_itoa_padding(start+(i%(end-start)),&t->key,10);
		else t->len=my_itoa(start+(i%(end-start)),&t->key);
		bitmap_set(start+(i%(end-start)));
#else
		m->body[i/m->bech][i%m->bech].key=start+(i%(end-start));
		bitmap_set(m->body[i/m->bech][i%m->bech].key);
#endif
		m->body[i/m->bech][i%m->bech].type=FS_SET_T;
#ifdef DVALUE
		m->body[i/m->bech][i%m->bech].length=GET_VALUE_SIZE;
#else	
		m->body[i/m->bech][i%m->bech].length=PAGESIZE;
#endif
		m->body[i/m->bech][i%m->bech].mark=m->mark;
		m->write_cnt++;

#ifdef KVSSD
		t=&m->body[(i+m->tot_ops/2)/m->bech][(i+m->tot_ops/2)%m->bech].key;
		if(seq_padding_opt)t->len=my_itoa_padding(start+(i%(end-start)),&t->key,10);
		else t->len=my_itoa(start+(i%(end-start)),&t->key);
#else
		m->body[(i+m->tot_ops/2)/m->bech][(i+m->tot_ops/2)%m->bech].key=start+(i%(end-start));
#endif
		m->body[(i+m->tot_ops/2)/m->bech][(i+m->tot_ops/2)%m->bech].type=FS_GET_T;
		m->body[(i+m->tot_ops/2)/m->bech][(i+m->tot_ops/2)%m->bech].length=PAGESIZE;
		m->body[(i+m->tot_ops/2)/m->bech][(i+m->tot_ops/2)%m->bech].mark=m->mark;
		m->read_cnt++;
	}
}

void randget(uint32_t start, uint32_t end,bench_monitor_t *m){
	printf("making rand Get bench!\n");
	srand(1);
	for(uint32_t i=0; i<m->tot_ops; i++){
#ifdef KVSSD
		KEYT *t=&m->body[i/m->bech][i%m->bech].key;
		t->len=my_itoa(start+rand()%(end-start),&t->key);
#else
		m->body[i/m->bech][i%m->bech].key=start+rand()%(end-start);
#endif
		m->body[i/m->bech][i%m->bech].type=FS_GET_T;
		m->body[i/m->bech][i%m->bech].length=PAGESIZE;
		m->body[i/m->bech][i%m->bech].mark=m->mark;
		m->read_cnt++;
	}
}

void randset(uint32_t start, uint32_t end, bench_monitor_t *m){
	printf("making rand Set bench!\n");
	for(uint32_t i=0; i<m->tot_ops; i++){
#ifdef KVSSD
		uint32_t t_k;
		KEYT *t=&m->body[i/m->bech][i%m->bech].key;
#ifdef KEYGEN
		t_k=keygenerator(end);
#else
		t_k=start+rand()%(end-start);
#endif
		t->len=my_itoa(t_k,&t->key);
		bitmap_set(t_k);
#else
#ifdef KEYGEN
		m->body[i/m->bech][i%m->bech].key=keygenerator(end);
#else
		m->body[i/m->bech][i%m->bech].key=start+rand()%(end-start);
#endif
		bitmap_set(m->body[i/m->bech][i%m->bech].key);
#endif

		m->body[i/m->bech][i%m->bech].length=PAGESIZE;
		m->body[i/m->bech][i%m->bech].mark=m->mark;
		m->body[i/m->bech][i%m->bech].type=FS_SET_T;
		m->write_cnt++;
	}
}

void randrw(uint32_t start, uint32_t end, bench_monitor_t *m){
	printf("making rand Set and Get bench!\n");
	for(uint32_t i=0; i<m->tot_ops/2; i++){
#ifdef KVSSD
		uint32_t t_k;
		KEYT *t=&m->body[i/m->bech][i%m->bech].key;
	#ifdef KEYGEN
		t_k=keygenerator(end);	
	#else
		t_k=start+rand()%(end-start);
	#endif
		bitmap_set(t_k);
		t->len=my_itoa(t_k,&t->key);
#else
	#ifdef KEYGEN
		m->body[i/m->bech][i%m->bech].key=keygenerator(end);
	#else
		m->body[i/m->bech][i%m->bech].key=start+rand()%(end-start);
	#endif
		bitmap_set(m->body[i/m->bech][i%m->bech].key);
#endif

		m->body[i/m->bech][i%m->bech].type=FS_SET_T;
#ifdef DVALUE
		m->body[i/m->bech][i%m->bech].length=GET_VALUE_SIZE;
		if(m->body[i/m->bech][i%m->bech].length>PAGESIZE){
			abort();
		}

//		m->body[i/m->bech][i%m->bech].length=0;
//		m->body[i/m->bech][i%m->bech].length=PAGESIZE-PIECE;
#else	
		m->body[i/m->bech][i%m->bech].length=PAGESIZE;
#endif
		m->body[i/m->bech][i%m->bech].mark=m->mark;
		m->write_cnt++;

#ifdef KVSSD
		t=&m->body[(i+m->tot_ops/2)/m->bech][(i+m->tot_ops/2)%m->bech].key;
		t->len=my_itoa(t_k,&t->key);
#else
		m->body[(i+m->tot_ops/2)/m->bech][(i+m->tot_ops/2)%m->bech].key=m->body[i/m->bech][i%m->bech].key;
#endif
		m->body[(i+m->tot_ops/2)/m->bech][(i+m->tot_ops/2)%m->bech].type=FS_GET_T;
		m->body[(i+m->tot_ops/2)/m->bech][(i+m->tot_ops/2)%m->bech].length=PAGESIZE;
		m->body[(i+m->tot_ops/2)/m->bech][(i+m->tot_ops/2)%m->bech].mark=m->mark;
		m->read_cnt++;
	}
	printf("last set:%lu\n",(m->tot_ops-1)/m->bech);
}
void mixed(uint32_t start, uint32_t end,int percentage, bench_monitor_t *m){
	printf("making mixed bench!\n");
	for(uint32_t i=0; i<m->tot_ops; i++){
#ifdef KVSSD
		uint32_t t_k;
		KEYT *t=&m->body[i/m->bech][i%m->bech].key;
	#ifdef KEYGEN
		t_k=keygenerator(end);	
	#else
		t_k=start+rand()%(end-start);
	#endif
		t->len=my_itoa(t_k,&t->key);
#else
	#ifdef KEYGEN
		m->body[i/m->bech][i%m->bech].key=keygenerator(end);
	#else
		m->body[i/m->bech][i%m->bech].key=start+rand()%(end-start);
	#endif

#endif

		if(rand()%100<percentage){
			m->body[i/m->bech][i%m->bech].type=FS_SET_T;
#ifdef KVSSD
			bitmap_set(t_k);
#else
			bitmap_set(m->body[i/m->bech][i%m->bech].key);
#endif
#ifdef DVALUE
			m->body[i/m->bech][i%m->bech].length=GET_VALUE_SIZE;
#else
			m->body[i/m->bech][i%m->bech].length=PAGESIZE;
#endif
			m->write_cnt++;
		}
		else{
			m->body[i/m->bech][i%m->bech].type=FS_GET_T;
			m->body[i/m->bech][i%m->bech].length=PAGESIZE;
			m->read_cnt++;
		}
		m->body[i/m->bech][i%m->bech].mark=m->mark;
	}
}

#ifndef KVSSD
void seq_latency(uint32_t start, uint32_t end,int percentage, bench_monitor_t *m){
	printf("making latency bench!\n");
	//seqset process
	for(uint32_t i=0; i<m->tot_ops/2; i++){
		m->body[i/m->bech][i%m->bech].key=start+(i%(end-start));
		bitmap_set(m->body[i/m->bech][i%m->bech].key);
#ifdef DVALUE
		m->body[i/m->bech][i%m->bech].length=GET_VALUE_SIZE;
#else
		m->body[i/m->bech][i%m->bech].length=PAGESIZE;
#endif
		m->body[i/m->bech][i%m->bech].type=FS_SET_T;
		m->body[i/m->bech][i%m->bech].mark=m->mark;
		m->write_cnt++;
	}

	for(uint32_t i=m->tot_ops/2; i<m->tot_ops; i++){
#ifdef KEYGEN
		m->body[i/m->bech][i%m->bech].key=keygenerator(end);
#else
		m->body[i/m->bech][i%m->bech].key=start+rand()%(end-start);
#endif
		if(rand()%100<percentage){
			m->body[i/m->bech][i%m->bech].type=FS_SET_T;
			bitmap_set(m->body[i/m->bech][i%m->bech].key);
#ifdef DVALUE
			m->body[i/m->bech][i%m->bech].length=GET_VALUE_SIZE;
#else
			m->body[i/m->bech][i%m->bech].length=PAGESIZE;
#endif
			m->write_cnt++;
		}
		else{
			while(!bitmap_get(m->body[i/m->bech][i%m->bech].key)){
#ifdef KEYGEN
				m->body[i/m->bech][i%m->bech].key=keygenerator(end);
#else
				m->body[i/m->bech][i%m->bech].key=start+rand()%(end-start);
#endif
			}
			m->body[i/m->bech][i%m->bech].type=FS_GET_T;
			m->body[i/m->bech][i%m->bech].length=PAGESIZE;
			m->read_cnt++;
		}
		m->body[i/m->bech][i%m->bech].mark=m->mark;
	}
}


void rand_latency(uint32_t start, uint32_t end,int percentage, bench_monitor_t *m){
	printf("making latency bench!\n");
	for(uint32_t i=0; i<0; i++){
		m->body[i/m->bech][i%m->bech].key=start+rand()%(end-start);
		bitmap_set(m->body[i/m->bech][i%m->bech].key);
#ifdef DVALUE
		m->body[i/m->bech][i%m->bech].length=GET_VALUE_SIZE;
#else
		m->body[i/m->bech][i%m->bech].length=PAGESIZE;
#endif
		m->body[i/m->bech][i%m->bech].type=FS_SET_T;
		m->body[i/m->bech][i%m->bech].mark=m->mark;
		m->write_cnt++;
	}

	for(uint32_t i=0; i<m->tot_ops; i++){
		if(rand()%100<percentage){
#ifdef KEYGEN
			m->body[i/m->bech][i%m->bech].key=keygenerator_type(end,FS_SET_T);
#else
			m->body[i/m->bech][i%m->bech].key=start+rand()%(end-start);
#endif
			m->body[i/m->bech][i%m->bech].type=FS_SET_T;
			bitmap_set(m->body[i/m->bech][i%m->bech].key);
#ifdef DVALUE
			m->body[i/m->bech][i%m->bech].length=GET_VALUE_SIZE;
#else
			m->body[i/m->bech][i%m->bech].length=PAGESIZE;
#endif
			m->write_cnt++;
		}
		else{
#ifdef KEYGEN
			m->body[i/m->bech][i%m->bech].key=keygenerator_type(end,FS_GET_T);
#else
			m->body[i/m->bech][i%m->bech].key=start+rand()%(end-start);
#endif
			while(!bitmap_get(m->body[i/m->bech][i%m->bech].key)){
#ifdef KEYGEN
				m->body[i/m->bech][i%m->bech].key=keygenerator_type(end,FS_GET_T);
#else
				m->body[i/m->bech][i%m->bech].key=start+rand()%(end-start);
#endif
			}
			m->body[i/m->bech][i%m->bech].type=FS_GET_T;
			m->body[i/m->bech][i%m->bech].length=PAGESIZE;
			m->read_cnt++;
		}
		m->body[i/m->bech][i%m->bech].mark=m->mark;
	}
}
#endif

void fillrand(uint32_t start, uint32_t end, bench_monitor_t *m){
	printf("making fillrand Set bench!\n");
	uint32_t* unique_array=(uint32_t*)malloc(sizeof(uint32_t)*(end-start+2));
	for(uint32_t i=start; i<=end; i++ ){
		unique_array[i]=i;
	}
	
	uint32_t range=end-start+1;
	for(uint32_t i=start; i<=end; i++){
		int a=rand()%range;
		int b=rand()%range;
		uint32_t temp=unique_array[a];
		unique_array[a]=unique_array[b];
		unique_array[b]=temp;
	}

	for(uint32_t i=0; i<m->tot_ops; i++){
#ifdef KVSSD
		KEYT *t=&m->body[i/m->bech][i%m->bech].key;
		t->len=my_itoa(unique_array[i%range],&t->key);
		bitmap_set(unique_array[i%range]);
#else
		m->body[i/m->bech][i%m->bech].key=unique_array[i%range];
		bitmap_set(m->body[i/m->bech][i%m->bech].key);
#endif

#ifdef DVALUE
		m->body[i/m->bech][i%m->bech].length=GET_VALUE_SIZE;
#else	
		m->body[i/m->bech][i%m->bech].length=PAGESIZE;
#endif
		m->body[i/m->bech][i%m->bech].mark=m->mark;
		m->body[i/m->bech][i%m->bech].type=FS_SET_T;
		m->write_cnt++;
	}
	free(unique_array);
}

void bench_cache_hit(int mark){
	bench_monitor_t *_m=&bm->bench_mon[mark];
	_m->cache_hit++;
}


char *bench_lower_type(int a){
	switch(a){
		case 0:return "TRIM";
		case 1:return "MAPPINGR";
		case 2:return "MAPPINGW";
		case 3:return "GCMR";
		case 4:return "GCMW";
		case 5:return "DATAR";
		case 6:return "DATAW";
		case 7:return "GCDR";
		case 8:return "GCDW";
		case 9:return "GCMR_DGC";
		case 10:return "GCMW_DGC";
	}
	return NULL;
}

void bench_custom_init(MeasureTime *mt,int idx){
#ifdef CHECKINGTIME
	for(int i=0; i<idx; i++){
		measure_init(&mt[i]);
	}
#endif
}

void bench_custom_start(MeasureTime *mt,int idx){
#ifdef CHECKINGTIME
	MS(&mt[idx]);
#endif
}

void bench_custom_A(MeasureTime *mt,int idx){
#ifdef CHECKINGTIME
	MA(&mt[idx]);
#endif
}

void bench_custom_print(MeasureTime *mt,int idx){
#ifdef CHECKINGTIME
	for(int i=0; i<idx; i++){
		printf("%d:",i);
		measure_adding_print(&mt[i]);
	}
#endif
}

