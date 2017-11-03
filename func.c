#include "slave.h"
#include <stdio.h>
#include "defi.h"
#include <string.h>

extern RayItem data[NUM_RAYS];
extern Point3 gravity[NUM_LABELS];
extern SLAVE_CNT part[NUM_SLAVE_CORE][NUM_LABELS];

#define LEN (NUM_RAYS/NUM_SLAVE_CORE+1)

__thread_local volatile unsigned long get_reply,put_reply;
__thread_local RayItem slice[LEN];
__thread_local Point3 _gravity[NUM_LABELS];
__thread_local SLAVE_CNT _part[NUM_LABELS];


/* 1,load partial data into slice
 * 2,load gravity into _gravity
 * 3,update [slice labels] & [_part]
 * 4,replace data with slice
 * 5,replace part with _part
 * */
void func() {
        int id=athread_get_id(-1);
        get_reply=put_reply=0;
        // load partial data
        int c_size=min(NUM_RAYS-id*LEN,LEN);
        if(c_size>LEN) {
                printf("Abort! assertion failed at file:%s,line:%d\n",__FILE__,__LINE__);
                exit(-1);
        }
        athread_get(PE_MODE,&data[id*LEN],slice,sizeof(RayItem)*c_size,&get_reply,0,0,0);
        while(get_reply!=1);
        // load gravity
        athread_get(PE_MODE,gravity,_gravity,sizeof(_gravity),&get_reply,0,0,0);
        while(get_reply!=2);

        // update [slice labels] & [_part]
        memset(_part,0,sizeof(_part));
        int i;
        for(i=0; i<c_size; i++) {
                Float curD,minD=FLT_MAX;
                unsigned short j,ptr = 0xffff;
                //slice[i].label=slice[i].bunch=-1;
                for(j=0; j<NUM_LABELS; j++) {
                        curD=get_D(_gravity[j],slice[i].ray.o);
                        if(curD<minD) {
                                minD=curD;
                                ptr=j;
                        }
                }
                if(ptr==0xffff) {
                        printf("Abort! assertion failed at file:%s,line:%d\n",__FILE__,__LINE__);
                        exit(-1);
                }
                slice[i].label=ptr;
                _part[ptr].sum.x+=slice[i].ray.o.x;
                _part[ptr].sum.y+=slice[i].ray.o.y;
                _part[ptr].sum.z+=slice[i].ray.o.z;
                _part[ptr].n++;
        }

        // replace date with slice
        athread_put(PE_MODE,slice,&data[id*LEN],sizeof(RayItem)*c_size,&put_reply,0,0);
        while(put_reply!=1);
        // replace part with _part
        athread_put(PE_MODE,_part,part[id],sizeof(_part),&put_reply,0,0);
        while(put_reply!=2);
}

