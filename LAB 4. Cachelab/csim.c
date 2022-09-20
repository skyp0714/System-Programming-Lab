#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <unistd.h>
#include <getopt.h>
#include <string.h>
#include "cachelab.h"


int hit_count, miss_count, eviction_count;
int trace_on, s, E, b, LRU_cnt;
char * tracefile;
//define cache structure
typedef struct{
    int val;
    int tag;
    int LRU_cnt;
}cache_line;
cache_line **cache;

void param_set(int argc, char**argv);
void do_trace();
void eval(unsigned long long addr);

int main(int argc, char ** argv)
{
    param_set(argc, argv);
    do_trace();

    printSummary(hit_count, miss_count, eviction_count);
    return 0;
}

// parsing arguments and setting cache
void param_set(int argc, char**argv){
    int opt, i, j;
    while(-1 != (opt = getopt(argc, argv, "hvs:E:b:t:"))){
        switch(opt){
            case 'h':
                break;
            case 'v':
                trace_on = 1;
                break;
            case 's':
                s = atoi(optarg);
                break;
            case 'E':
                E = atoi(optarg);
                break;
            case 'b':
                b = atoi(optarg);
                break;
            case 't':
                tracefile = optarg;
                break;
            default:
                printf("wrong argument!");
                break;
        }
    }
    cache = (cache_line**)malloc(sizeof(cache_line*)*(1<<s));
    for(i=0; i<(1<<s); i++){
        cache[i] = (cache_line*)malloc(sizeof(cache_line)*E);
        for(j=0;j<E;j++){
            cache[i][j].val = 0;
            cache[i][j].tag = 0;
            cache[i][j].LRU_cnt = 0;
        }
    }
}
// parsing memory instruction and trace cache hit/miss/eviction
void do_trace(){
    FILE *ptrace= fopen(tracefile, "r");
    char identifier;
    unsigned long long addr;
    int size;
    while(fscanf(ptrace, "%c %llx, %d\n", &identifier, &addr, &size)>0){
        switch(identifier){
            case  'I':
                break;
            case  'L':
                if(trace_on) printf("%c, %llx, %d", identifier, addr, size);
                eval(addr);
                if(trace_on) printf("\n");
                break;
            case  'S':
                if(trace_on) printf("%c, %llx, %d", identifier, addr, size);
                eval(addr);
                if(trace_on) printf("\n");
                break;
            case  'M':
                if(trace_on) printf("%c, %llx, %d", identifier, addr, size);
                eval(addr);
                eval(addr);
                if(trace_on) printf("\n");
                break;
            default:
                printf("Invalid instruction type\n");
                break;    
        }
        //LRU_cnt increases for every instruction counts
        LRU_cnt++;
    }
    fclose(ptrace);
}
// determine addr is in cache or not
void eval(unsigned long long addr){
    int i;
    int tag = addr >> (s+b);
    int idx = (addr >> b) - (tag << s);
    int empty_line, full=1;
    int evict_line, min_LRU_cnt = LRU_cnt;
    for(i=0;i<E;i++){
        if((cache[idx][i].tag == tag) && cache[idx][i].val){             /* Cache Hit */
            if(trace_on) printf(" hit");
            cache[idx][i].LRU_cnt = LRU_cnt;
            hit_count++;
            return;
        }
        if(!cache[idx][i].val){                                         /* determine whether cache line is full */
            empty_line = i;
            full = 0;
        }else if(cache[idx][i].LRU_cnt <= min_LRU_cnt){                 /* determine LRU line */
            min_LRU_cnt = cache[idx][i].LRU_cnt;
            evict_line = i;
        }
    }
    
    // cache miss!

    if(full){         /* Cache Miss Eviction */
        if(trace_on) printf(" miss eviction");
        cache[idx][evict_line].tag = tag;
        cache[idx][evict_line].LRU_cnt = LRU_cnt;
        miss_count++;
        eviction_count++;
    }else{                  /* Cache Miss */
        if(trace_on) printf(" miss");
        cache[idx][empty_line].val = 1;
        cache[idx][empty_line].tag = tag;
        cache[idx][empty_line].LRU_cnt = LRU_cnt;
        miss_count++;
    }
    return;
}