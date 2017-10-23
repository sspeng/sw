#include "slave.h"
#include <stdio.h>
#include "definition.h"


extern RayItem data[NUM_RAYS], tmpdata[NUM_RAYS];
extern unsigned int host_nRays;
extern Point3 gravity[NUM_LABELS];

/******Vertex Cluster*********/
__thread_local RayItem slice[NUM_RAYS / NUM_SLAVE_CORE + 1];
__thread_local Point3 _gravity[NUM_LABELS];
__thread_local volatile unsigned long get_reply, put_reply;
__thread_local int id;

/** For Vertex Cluster
 *  @param
 *  1.data & its valid length
 *  2.gravity
 *
 *  @proc
 *  Specify a unique group id for each valid RayItem
 *
 * */
void mark_vertex() {
	//printf("slave_start\n");
	int len = sizeof(slice) / sizeof(RayItem);
	id = athread_get_id(-1);
	get_reply = put_reply = 0;
	athread_get(PE_MODE, &data[id*len], slice, sizeof(slice), &get_reply, 0, 0, 0);
	athread_get(PE_MODE, gravity, _gravity, sizeof(_gravity), &get_reply, 0, 0, 0);
	while (get_reply != 2);

	int idx, cur, ptr = 0;
	for (idx = 0; idx < len; idx++) {
		Float minD = FLT_MAX, curD;
		for (cur = 0; cur < NUM_LABELS; cur++) {
			curD = get_D(norm(_gravity[cur]), norm(slice[idx].ray.o));
			if (curD < minD) {
				ptr = cur;
				minD = curD;
			}
		}
		slice[idx].label = ptr;
	}

	athread_put(PE_MODE, slice, &data[id*len], sizeof(slice), &put_reply, 0, 0);
	while (put_reply != 1);
}


/** For Render
 *  @param
 *
 *  @proc
 *
 * */
void render() {
	//printf("func->render\n");
}




