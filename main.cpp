#include <stdio.h>
#include <iostream>
#include "definition.h"

RayItem tmpdata[NUM_RAYS]/** 用于临时存储光线数据 */, data[NUM_RAYS];/** 光线数据*/
volatile int host_nRays/** 用于记录光线数组中可用数量*/, order[NUM_LABELS]/** gravity排序标准*/, lock/** 协调从核进行位置认领，禁止优化*/, vptr[NUM_LABELS]/** 位置簇*/, cptr[NUM_CLUSTERS]/** 所有类别指针*/;
SLAVE_CNT slave_res[NUM_SLAVE_CORE][NUM_LABELS];      /** 用于存储从核计算后的结果*/
Point3 gravity[NUM_LABELS]/**位置聚类后各类重心*/, tmpgravity[NUM_LABELS]/**gravity的备份，用于从核*/, pregravity[NUM_LABELS]/**上一次位置聚类后各类重心*/, direction[NUM_BUNCHES];	/** 各朝向簇的方向*/

extern "C" {
#include <athread.h>

	void slave_compute(void);
	void slave_vertex_cluster(void);
	void slave_refresh_order(void);
	void slave_init_tmpdata(void);
	//void slave_vector_cluster_by_vertex(void);
	void slave_vector_cluster(void);
	void slave_sort_data_by_cluster(void);
	void slave_sort_data_by_vertex(void);

	void slave_func(void);
}

/** 统计slave_res并将均值写入gravity，双重循环，当前由主核实现，可交给从核完成
 *  @param
 *  1.slave_res
 *  2.gravity
 *
 *  @proc
 *  fresh gravity
 * */
void mean_gravity() {
	for (int j = 0; j < NUM_LABELS; j++) {
		/** 初始化对应位gravity*/
		gravity[j].x = gravity[j].y = gravity[j].z = (Float)0;
		int n = 0;
		for (int i = 0; i < NUM_SLAVE_CORE; i++) {
			gravity[j].x += slave_res[i][j].sum.x;
			gravity[j].y += slave_res[i][j].sum.y;
			gravity[j].z += slave_res[i][j].sum.z;
			n += slave_res[i][j].n;
		}
		gravity[j].x /= n;
		gravity[j].y /= n;
		gravity[j].z /= n;
	}
}

/** 按照order顺序将gravity和order排序，以快排方式实现
 *  @param
 *  1.order数组
 *  2.gravity数组
 *
 *  @proc
 *  将order和gravity同时排序
 * */
void sort_gravity_by_order(int s, int e) {
	if (s > e) return;
	int i = s, j = e;
	while (i < j) {
		while (order[j] >= order[s] && i < j) j--;
		while (order[i] <= order[s] && i < j) i++;
		std::swap(order[i], order[j]);
		std::swap(gravity[i], gravity[j]);
	}
	std::swap(order[s], order[i]);
	std::swap(gravity[s], gravity[i]);
	sort_gravity_by_order(s, i - 1);
	sort_gravity_by_order(i + 1, e);
}


/** 主核中负责任务分配的函数
 *  @param
 *  1.data数组
 *  2.data数组有效长度
 *
 *  @proc
 *  主从协同对光线聚类、排序、渲染
 * */
void distribute() {
	/** 直接划分光线到从核*/
	lock = 1;
	athread_init();
	//* 初始化gravity
	//init_gravity();
	while (host_nRays > 0) {
		for (int i = 0; i < MAX_VERTEX_CLUSTER_ROUND; i++) {
			/** 位置聚类*/
			/** PART I Vertex Cluster*/
			__real_athread_spawn((void*)slave_vertex_cluster, 0); /** 进行位置聚类*/
			athread_join();

			__real_athread_spawn((void*)slave_compute, 0);/** 从核分段统计*/
			athread_join();

			mean_gravity();/** 主核更新gravity数组*/

			memcpy(tmpgravity, gravity, sizeof(Point3)*NUM_LABELS);/** 更新tmpgravity*/
			__real_athread_spawn((void*)slave_refresh_order, 0);/** 从核参照pregravity和tmpgravity,按次序认领重心*/
			athread_join();

			sort_gravity_by_order(0, NUM_LABELS);/** 将gravity排序*/

			int vertexptr[NUM_LABELS];/** 将data分成NUM_LABELS段*/
			for (int p = 0; p < NUM_LABELS; p++) vertexptr[p] = p*(NUM_RAYS / NUM_LABELS);

			__real_athread_spawn((void*)slave_init_tmpdata, 0); /** tmpdata所有值初始置为0*/
			athread_join();

			/** 初始化vptr*/
			for (int v = 0; v < NUM_LABELS; v++) vptr[v] = v*NUM_LABELS;

			/** 对data重排序使成为形如123123123的锯齿形*/
			__real_athread_spawn((void*)slave_sort_data_by_vertex, 0);
			athread_join();

			memcpy(data, tmpdata, sizeof(Point3)*NUM_RAYS);

			/** PART II Vector Cluster*/
			__real_athread_spawn((void*)slave_vector_cluster, 0);
			athread_join();

			__real_athread_spawn((void*)slave_init_tmpdata, 0); /** tmpdata所有值初始置为0*/
			athread_join();

			for (int p = 0; p < NUM_CLUSTERS; p++) cptr[p] = p*NUM_CLUSTERS;
			__real_athread_spawn((void*)slave_sort_data_by_cluster, 0);
			athread_join();

			memcpy(data, tmpdata, sizeof(Point3)*NUM_RAYS);
		}
		/** PART III Ray Render*/
		host_nRays--;
	}
	athread_halt();/** 结束线程库*/
}

void init_data() {
	for (int i = 0; i < NUM_RAYS; i++) {
		data[i].label = i%NUM_LABELS;
		data[i].bunch = i%NUM_BUNCHES;
		data[i].ray.o.x = data[i].ray.d.x = (Float)(rand() % 1000);
		data[i].ray.o.y = data[i].ray.d.y = (Float)(rand() % 1000);
		data[i].ray.o.z = data[i].ray.d.z = (Float)(rand() % 1000);
	}
}


/** for test*/
void mfunc() {
	athread_init();
	while (host_nRays > 0) {
		for (int i = 0; i < 5; i++) {
			__real_athread_spawn((void*)slave_func, 0);
			athread_join();

			__real_athread_spawn((void*)slave_init_tmpdata, 0); // tmpdata所有值初始置为0，由主核完成？
			athread_join();
		}
		host_nRays--;
	}
	athread_halt();
}

int main() {
	std::cout << "start" << std::endl;
	host_nRays = 5;
	//init_data();
	distribute();
	//mfunc();
	std::cout << "done" << std::endl;

	return 0;
}

