#include <iostream>
#include <stdlib.h>
#include "defi.h"
#include <string.h>
#include <assert.h>

using namespace std;

RayItem data[NUM_RAYS],_data[NUM_RAYS];
Point3 gravity[NUM_LABELS];
SLAVE_CNT part[NUM_SLAVE_CORE][NUM_LABELS];


extern "C"{
#include <athread.h>
        void slave_func(void);
}

void init_data(){
        // init label & bunch
        for(int i=0;i<NUM_RAYS;i++){
                data[i].label=rand()%NUM_LABELS;
                data[i].bunch=rand()%NUM_BUNCHES;
                data[i].ray.o.x=(Float)(rand()%1000);
                data[i].ray.o.y=(Float)(rand()%1000);
                data[i].ray.o.z=(Float)(rand()%1000);
        }
}

void sortVertex(RayItem data[NUM_RAYS],int validLen,int iterVertex = 10){
        // 1,init gravity
        for(int i=0;i<NUM_LABELS;i++){
                gravity[i].x=data[NUM_RAYS/NUM_LABELS*i].ray.o.x;
                gravity[i].y=data[NUM_RAYS/NUM_LABELS*i].ray.o.y;
                gravity[i].z=data[NUM_RAYS/NUM_LABELS*i].ray.o.z;
        }
        for(int i=0;i<iterVertex;i++){
                memset(part,0,sizeof(part));
                // 2,slave cores mark labels
                __real_athread_spawn((void*)slave_func,0);
                athread_join();

                // 3,count part & update gravity
                memset(gravity,0,sizeof(gravity));
                for(int j=0;j<NUM_LABELS;j++){
                        int n = 0;
                        for(int k=0;k<NUM_SLAVE_CORE;k++){
                                gravity[j].x+=part[k][j].sum.x;
                                gravity[j].y+=part[k][j].sum.y;
                                gravity[j].z+=part[k][j].sum.z;
                                n+=part[k][j].n;
                        }
                        n=n==0?1:n;
                        gravity[j].x/=n;
                        gravity[j].y/=n;
                        gravity[j].z/=n;
                }
        }
        // 4,sort data by label
        int cnt[NUM_LABELS],off[NUM_LABELS];
        memset(cnt,0,sizeof(cnt));
        off[0]=0;
        for(int i=0;i<NUM_RAYS;i++){
                if(data[i].label==0xffff) continue;
                assert(data[i].label>=0&&data[i].label<NUM_LABELS);
                cnt[data[i].label]++;
        }

        for(int i=1;i<NUM_LABELS;i++) off[i]=off[i-1]+cnt[i-1];
        for(int i=0;i<NUM_RAYS;i++) _data[off[data[i].label]++]=data[i];
        memcpy(data,_data,sizeof(_data));
}


int main(){
        athread_init();

        init_data();
        sortVertex(data,NUM_RAYS,1);
        for(int i=0;i<NUM_RAYS;i++){
                printf("%d:[%x,%x]\n",i+1,data[i].label,data[i].bunch);
        }
        athread_halt();
        return 0;
}

