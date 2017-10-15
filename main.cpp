#include <iostream>
#include <assert.h>
#include <string.h>
#include <stdio.h>
#include <cmath>
#include "definition.h"

RayItem data[NUM_RAYS], tmpdata[NUM_RAYS];
int curOrder[NUM_LABELS], preOrder[NUM_LABELS];
volatile unsigned int host_nRays;
volatile bool canSkip;
int lptr[NUM_LABELS], bptr[NUM_BUNCHES];

Point3 gravity[NUM_LABELS], direction[NUM_BUNCHES];

using namespace std;

/*extern "C" {
#include <athread.h>

	void slave_mark_vertex(void);
	void slave_render(void);
}*/

void init_data() {
	for (int i = 0; i < NUM_RAYS; i++) {
		data[i].ray.o.x = data[i].ray.d.x = i % 500;
		data[i].ray.o.y = data[i].ray.d.y = i % 1000;
		data[i].ray.o.z = data[i].ray.d.z = i % 300;
		data[i].label = NUM_BUNCHES - i%NUM_BUNCHES - 1;
		data[i].bunch = rand() % NUM_BUNCHES;
	}
	//for(int i=0;i<NUM_RAYS;i++) printf("[%d,%d]\n",data[i].label,data[i].bunch);
}

void distribute() {
	/** Init*/
//	athread_init();
	for (int i = 0; i < NUM_LABELS; i++) gravity[i] = data[i*(NUM_RAYS / NUM_LABELS)].ray.o;

	/** Cluster*/
	while (host_nRays > 0) {
		cout << "host_nRays = " << host_nRays << endl;
		/** Vertex Cluster*/
		for (int i = 0; i < MAX_VERTEX_CLUSTER_ROUND && !canSkip; i++) {
			/** update item label*/
			/*athread_init();
			__real_athread_spawn((void*)slave_mark_vertex, 0);
			athread_join();
			athread_halt();*/
			int ptr;
			Float minD = FLT_MAX, curD;
			for (int j = 0; j < NUM_RAYS; j++) {
				for (int lb = 0; lb < NUM_LABELS; lb++) {
					curD = get_D(norm(gravity[lb]), norm(data[j].ray.o));
					if (curD < minD) {
						minD = curD;
						ptr = lb;
					}
				}
				data[i].label = ptr;
			}

			/** update gravity*/
			SLAVE_CNT _gravity[NUM_LABELS];
			//cout<<"123"<<endl;
			for (int j = 0; j < NUM_LABELS; j++) _gravity[j].sum.x = _gravity[j].sum.y = _gravity[j].sum.z = (Float)(_gravity[j].n = 0);
			//cout<<"345"<<endl;
			//cout<<NUM_RAYS<<endl;
			for (int j = 0; j < NUM_RAYS; j++) {
				//cout<<j<<"       "<<data[j].label<<endl;
				_gravity[data[j].label].sum.x += data[j].ray.o.x;
				_gravity[data[j].label].sum.y += data[j].ray.o.y;
				_gravity[data[j].label].sum.z += data[j].ray.o.z;
				_gravity[data[j].label].n++;
//				printf("%d\n", _gravity[data[j].label].n++);
			}
			for (int j = 0; j < NUM_LABELS; j++) {
				_gravity[j].n = (_gravity[j].n==0)?1:(int)_gravity[j].n;
				gravity[j].x = _gravity[j].sum.x / _gravity[j].n;
				gravity[j].y = _gravity[j].sum.y / _gravity[j].n;
				gravity[j].z = _gravity[j].sum.z / _gravity[j].n;
			}
			//if(memcmp(curOrder,preOrder)==0) canSkip=true;
		}
		/** Align & Format data*/
		int len1 = NUM_RAYS / NUM_LABELS + 1;
		for (int i = 0; i < NUM_LABELS; i++) lptr[i] = i*len1;
		for (int i = 0; i < NUM_RAYS; i++) {
			int l = data[i].label;
			assert(l < NUM_LABELS);
			if (l < 0) continue;
			if (lptr[l] < min((l + 1)*len1, NUM_RAYS))
				tmpdata[lptr[l]++] = data[i];
			else {
				int ptr;
				for (int j = 0; j < NUM_LABELS; j++) {
					assert(lptr[j] <= min((j + 1)*len1, NUM_RAYS));
					if (lptr[j] == min((j + 1)*len1, NUM_RAYS)) continue;
					else {
						ptr = j;
						break;
					}
				}
				tmpdata[lptr[ptr]++] = data[i];
			}
		}
		memcpy(data, tmpdata, sizeof(data));
		canSkip = false;

		/** Vector Cluster*/
		for (int l = 0; l < NUM_LABELS; l++) {
			for (int i = 0; i < MAX_VECTOR_CLUSTER_ROUND && !canSkip; i++) {
				/** update item bunch*/
				/*int ptr;
				Float maxD=FLT_MIN,curD;
				for (int j = l*len1; j < min((l + 1)*len1, NUM_RAYS); j++) {
					for(int lb=0; lb<NUM_BUNCHES; lb++) {
						curD=dot(norm(data[j].ray.d),norm(direction[lb]));
						if(curD>maxD) {
							maxD=curD;
							ptr=lb;
						}
					}
					data[j].bunch=ptr;
				}*/

				/** update direction*/
				SLAVE_CNT _direction[NUM_BUNCHES];
				for (int j = 0; j < NUM_BUNCHES; j++) _direction[j].sum.x = _direction[j].sum.y = _direction[j].sum.z = (Float)(_direction[j].n = 0);
				for (int j = l*len1; j < min((l + 1)*len1, NUM_RAYS); j++) {
					_direction[data[j].bunch].sum.x += data[j].ray.d.x;
					_direction[data[j].bunch].sum.y += data[j].ray.d.y;
					_direction[data[j].bunch].sum.z += data[j].ray.d.z;
					_direction[data[j].bunch].n++;
				}
				for (int j = 0; j < NUM_BUNCHES; j++) {
					_direction[j].n = max(1, _direction[j].n);
					direction[j].x = _direction[j].sum.x / _direction[j].n;
					direction[j].y = _direction[j].sum.y / _direction[j].n;
					direction[j].z = _direction[j].sum.z / _direction[j].n;
				}
			}

			/** Align and Format data*/
			int len2 = len1 / NUM_BUNCHES + 1;
			for (int i = 0; i < NUM_BUNCHES; i++) bptr[i] = i*len2;
			for (int i = l*len1; i < min((l + 1)*len1, NUM_RAYS); i++) {
				int b = data[i].bunch;
				assert(b < NUM_BUNCHES);
				if (b < 0) continue;/** invalid item*/
				if (bptr[b] < min((b + 1)*len2, len1))
					tmpdata[bptr[b]++] = data[i];
				else {
					int ptr = 0;
					for (int j = 0; j < NUM_BUNCHES; j++) {
						assert(bptr[j] <= (j + 1)*len2);
						if (bptr[j] == min((j + 1)*len2, len1)) continue;
						else {
							ptr = j;
							break;
						}
					}
					tmpdata[bptr[ptr]++] = data[i];
				}
				memcpy(&data[l*len1], tmpdata, (min(NUM_RAYS, (l + 1)*len1) - l*len1) * sizeof(RayItem));
			}
			canSkip = false;
		}

		/** Render*/
		/*__real_athread_spawn((void*)slave_render, 0);
		athread_join();*/

		host_nRays--;
	}
//	athread_halt();
}


int main(int argc,char **argv) {
	int mpiid,numprocs;
//	MPI_Init();
	cout << "start" << endl;
	init_data();
	host_nRays = 10;
	distribute();
	cout << "done" << endl;
	return 0;
}
