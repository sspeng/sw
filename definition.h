#include "limits.h"		/** INT_MIN	INT_MAX*/
#include "float.h"		/** FLT_MIN	FLT_MAX*/
#include "math.h"		/** sqrt*/

#define	RAYS_PER_PIXEL		4/*4096*/		/** 每个像素发射光线数WARNING!!!使用4096导致空间超出*/
#define	BLOCK_WIDTH		32		/** 渲染块边长*/
#define	NUM_RAYS		BLOCK_WIDTH*BLOCK_WIDTH*RAYS_PER_PIXEL	/** 光线最大数量*/

#define NUM_SLAVE_CORE		64			/** 从核总数*/

#define	NUM_LABELS		8/**NUM_SLAVE_CORE*/	/** 空间类数量，应为从核数整数倍速*/
#define NUM_BUNCHES		8	/** 朝向类数量*/

#define	MAX_VERTEX_CLUSTER_ROUND	4096	/** 位置聚类最大循环次数*/
#define MAX_VECTOR_CLUSTER_ROUND	4096	/** 朝向聚类最大循环次数*/

#define NUM_CLUSTERS		NUM_LABELS*NUM_BUNCHES	/** 从核中SLAVE_CNT结构的最大容量，其值为总类数*/

#ifndef NULL
#define	NULL	0
#endif // !NULL

#ifndef Float
typedef float Float;
#endif // !Float

#ifndef Boolean
typedef enum Boolean { False, True } Boolean;
#endif // !bool

#ifndef Point2
typedef struct Point2 {
	Float x, y;
}Point2;
#endif // !Point2

#ifndef Point3
typedef struct Point3 {
	Float x, y, z;
}Point3;
#endif // !Point3

#ifndef Ray
typedef struct Ray {
	/** !!!*/
	Point3 o;/** 光线位置*/
	Point3 d;/** 光线朝向*/
}Ray;
#endif // !Ray

#ifndef Spectrum
typedef struct Spectrum {
	/** !!!*/
}Spectrum;
#endif // !Spectrum

#ifndef RadianceQueryRecord
typedef struct RadianceQueryRecord {
	/** !!!*/
}RadianceQueryRecord;
#endif // !RadianceQueryRecord

#ifndef RayItem
typedef struct RayItem {
	Ray ray;
	Float eta;
	Boolean hasNext, valid, nullChain, scattered;
	Spectrum throughput, Li;
	Point2 pixel;
	RadianceQueryRecord rRec;
	int label/*空间簇*/, bunch/*朝向簇*/;
}RayItem;
#endif // !RayItem

/** 用于记录各类光线重心之和与数量*/
#ifndef SLAVE_CNT
typedef struct SLAVE_CNT {
	Point3 sum;
	int n;
}SLAVE_CNT;
#endif // !SLAVE_CNT

/** 计算两个点之间距离的平方
* */
Float get_D(const Point3 a, const Point3 b) {
	Float D = (a.x - b.x)*(a.x - b.x) + (a.y - b.y)*(a.y - b.y) + (a.z - b.z)*(a.z - b.z);
	return D;
}

/** 给定向量，返回单位向量
* */
Point3 norm(Point3 a) {
	Point3 d = a;
	Float b = sqrt(a.x*a.x + a.y*a.y + a.z*a.z);
	d.x /= b; d.y /= b; d.z /= b;
	return d;
}

/** 此函数计算两个向量点积
* */
Float dot(const Point3 a, const Point3 b) {
	Float d = a.x*b.x + a.y*b.y + a.z*b.z;
	return d;
}

/** Point3是否有效
* */
Boolean isValid(const Point3 v) {
	if ((v.x == FLT_MAX) && (v.y == FLT_MAX) && (v.z == FLT_MAX)) return False;
	else return True;
}
