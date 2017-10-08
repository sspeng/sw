#include <string.h>	/** memset*/
#include "slave.h"
#include <stdio.h>
#include <stdlib.h>
#include "definition.h"


/** 从核变量声明*/
extern RayItem data[NUM_RAYS]/** 所有光线数据，注意访问越界*/, tmpdata[NUM_RAYS];
extern SLAVE_CNT slave_res[NUM_SLAVE_CORE][NUM_LABELS]/** 位于主核中，存储从核计算后的结果*/;
extern Point3 tmpgravity[NUM_LABELS], gravity[NUM_LABELS], pregravity[NUM_LABELS], direction[NUM_BUNCHES];	/** 各朝向簇的方向*/
extern int order[NUM_LABELS]/** 主核记录各类位置聚类后各类重心*/, host_nRays/** 主核记录有效光线数*/, vptr[NUM_LABELS]/** 位置簇*/, cptr[NUM_CLUSTERS]/**朝向簇*/;
extern volatile int lock;/** 用于从核协调的信号量*/

__thread_local int slave_nRays;/** 从核记录有效光线数，从主核上读取*/
__thread_local SLAVE_CNT part[NUM_LABELS];			/** 返回结构体定义*/
__thread_local volatile unsigned long get_reply, put_reply;


/************************************************
***************从核分段统计主核数据*************
************************************************/

/** 此函数用于协助主核统计各类光线位置和
* 	@param
* 	1.所有光线数据data
* 	2.data的有效长度
*
* 	@return
* 	1.结构体数组，保存各类部分光线重心和数量
**/
void compute() {
	int id = athread_get_id(-1), idx;
	get_reply = put_reply = 0;
	/** 初始化*/
	for (idx = 0; idx < NUM_LABELS; idx++) {
		part[idx].sum.x = part[idx].sum.y = part[idx].sum.z = (Float)0;
		part[idx].n = 0;
	}
	/** 使用所有从核进行赋值*/
	for (idx = id; idx < NUM_RAYS; idx += NUM_SLAVE_CORE) {
		int label = data[idx].label;/** 当前光线位置类别*/
		part[label].sum.x += data[idx].ray.o.x;
		part[label].sum.y += data[idx].ray.o.y;
		part[label].sum.z += data[idx].ray.o.z;
		part[label].n++;
	}
	athread_put(PE_MODE, &part, &slave_res[id], sizeof(SLAVE_CNT)*NUM_LABELS, &put_reply, 0, 0);
	while (!put_reply);
}


/************************************************
*******************初始化数据部分****************
*************************************************/

/** 此函数负责将tmpdata初始化
*  @param
*  1.tmpdata
*/
void init_tmpdata() {
	int id = athread_get_id(-1), idx;
	/** 使用所有从核赋值*/
	for (idx = id; idx < NUM_RAYS; idx += NUM_SLAVE_CORE) {
		tmpdata[idx].ray.o.x = tmpdata[idx].ray.o.y = tmpdata[idx].ray.o.z = (Float)0;
	}
}


/************************************************
********************重心排序部分*****************
*************************************************/

/** 以pregravity为标准，将最靠近当前位置的gravity位置更新
*  更新结果写入到order数组中
*  @param
*  1.gravity
*  2.pregravity
*  3.lock
*
*  @proc
*  根据lock情况判断
*
*  @return
*  排序标准数组 order
*
* */
void refresh_order() {
	int id = athread_get_id(-1), idx, l;
	Float minD = FLT_MAX;
	lock--;		/**加锁*/
	while (lock < 0);	/** 等候锁释放*/
	for (l = id; l < NUM_LABELS; l += NUM_SLAVE_CORE) {
		int ptr = 0;
		for (idx = 0; idx < NUM_LABELS; idx++) {
			if (!isValid(tmpgravity[idx])) continue;
			Float D = get_D(tmpgravity[idx], pregravity[l]);
			if (D < minD) {
				minD = D;
				ptr = idx;
			}
		}
		order[l] = ptr;
		tmpgravity[ptr].x = tmpgravity[ptr].y = tmpgravity[ptr].z = FLT_MAX;
	}
	/**建议:在此行添加barrier指令*/
	lock++;		/**释放锁*/
}


/************************************************
*******************位置聚类部分******************
*************************************************/

/** 空间聚类
*  @param
*  1.data数组
*  2.data有效长度host_nRays
*
*  @proc
*  直接修改data中的label标记
*
*/
void vertex_cluster() {
	int id = athread_get_id(-1), idx, c, ptr[NUM_LABELS];
	get_reply = put_reply = 0;
	athread_get(PE_MODE, &host_nRays, &slave_nRays, sizeof(int), &get_reply, 0, 0, 0);
	while (!get_reply);
	memset(ptr, 0, sizeof(int)*NUM_LABELS);
	int minptr = 0;			/** 取最小距离时的类别*/
	for (idx = id; idx < NUM_RAYS; idx += NUM_SLAVE_CORE/** 此处为利用所有从核，以从核数为准*/) {
		Float minD = FLT_MAX;	/** 循环时保留的最小距离*/
		for (c = 0; c < NUM_LABELS; c++) {
			if (ptr[c] == (slave_nRays / NUM_LABELS + 1)) continue;
			Float D = get_D(gravity[c], data[idx].ray.o);
			if (D < minD) {
				minD = D;
				minptr = c;	/** 使用minptr减小访主次数*/
			}
		}
		data[idx].label = minptr;/** 直接修改data中的类别标记*/
		ptr[minptr]++;
	}
}


/************************************************
*******************朝向聚类部分******************
*************************************************/

/**此函数负责对已经进行位置聚类且按照相应顺序排列的
* 光线数据进行朝向聚类
*
* @param
* 1.所有光线数据data
* 2.光线数据的有效长度
*
* @proc
* 将光线中的朝向标记直接修改
*
* */
void vector_cluster() {
	int id = athread_get_id(-1), idx, b, ptr[NUM_BUNCHES];
	get_reply = put_reply = 0;
	athread_get(PE_MODE, &host_nRays, &slave_nRays, sizeof(int), &get_reply, 0, 0, 0);
	while (!get_reply);
	int minptr = 0;
	memset(ptr, 0, sizeof(int)*NUM_BUNCHES);
	for (idx = id; idx < slave_nRays; idx += NUM_LABELS/** 此处为访问位置类中相同类别，以位置类数量为准*/) {
		Float minD = FLT_MAX;
		for (b = 0; b < NUM_BUNCHES; b++) {
			if (ptr[b] == (slave_nRays / NUM_BUNCHES + 1)) continue;
			Float D = dot(norm(data[idx].ray.d), norm(direction[b]));
			if (D < minD) {
				minD = D;
				minptr = b;
			}
		}
		data[idx].bunch = minptr;/** 修改光线中的类别标记*/
		ptr[minptr]++;
	}
}


/************************************************
*******************位置排序部分******************
*************************************************/
/** data现被分为NUM_LABELS段，此函数实现将各类重排序排成锯齿形
*  @param
*  1.tmpdata
*  2.data
*
*  @proc
*  直接修改tmpdata
*/
void sort_data_by_vertex() {
	int id = athread_get_id(-1), idx, l;
	get_reply = put_reply = 0;
	athread_get(PE_MODE, &host_nRays, &slave_nRays, sizeof(int), &get_reply, 0, 0, 0);
	while (!get_reply);
	for (idx = id; idx < NUM_RAYS; idx += NUM_SLAVE_CORE) {
		if (vptr[data[idx].label] < (data[idx].label + 1)*NUM_RAYS / NUM_LABELS) tmpdata[vptr[data[idx].label]++] = data[idx];
		else {
			Float minD = FLT_MAX;
			int minptr = 0;
			for (l = 0; l < NUM_LABELS; l++) {
				Float D = get_D(gravity[l], data[idx].ray.o);
				if (D < minD&& vptr[l] < (l + 1)*NUM_RAYS / NUM_LABELS) {
					minD = D;
					minptr = l;
				}
			}
			tmpdata[vptr[minptr]++] = data[idx];
		}
	}
}


/************************************************
*******************空间排序部分******************
*************************************************/
/** data现被分为NUM_CLUSTERS段，此函数实现将各类重排序排成锯齿形
*  @param
*  1.tmpdata
*  2.data
*
*  @proc
*  直接修改tmpdata
*/
void sort_data_by_cluster() {
	int id = athread_get_id(-1), idx, l;
	get_reply = put_reply = 0;
	athread_get(PE_MODE, &host_nRays, &slave_nRays, sizeof(int), &get_reply, 0, 0, 0);
	while (!get_reply);
	for (idx = id; idx < NUM_RAYS; idx += NUM_SLAVE_CORE) {
		int c = data[idx].label*data[idx].bunch;
		if (cptr[c] < ((c + 1)*slave_nRays / (NUM_CLUSTERS)/** 防止宏替换*/)) tmpdata[cptr[c]++] = data[idx];
		else {
			Float minD = FLT_MAX;
			int minptr = 0;
			for (l = 0; l < NUM_LABELS; l++) {
				Float D = get_D(data[idx].ray.o, gravity[l]);
				if (D < minD&&cptr[data[idx].bunch*l] < ((c + 1)*slave_nRays / (NUM_CLUSTERS)/** 防止宏替换*/)) {
					minD = D;
					minptr = l;
				}
			}
			tmpdata[cptr[minptr*data[idx].bunch]++] = data[idx];
		}
	}
}



/************************************************
*******************从核渲染部分******************
*************************************************/


int func() {
	int id = athread_get_id(-1), idx;
	//for (idx = id; idx < NUM_RAYS; idx += NUM_SLAVE_CORE) {
	printf("%d,", id);
	//}
}
