#include "limits.h"             /** INT_MIN     INT_MAX*/
#include "float.h"              /** FLT_MIN     FLT_MAX*/
#include <math.h>               /** sqrt*/
#include <stdbool.h>    /** bool*/

#define RAYS_PER_PIXEL          4/*4096*/               /***/
#define BLOCK_WIDTH             32              /** width of render block*/
#define NUM_RAYS                (BLOCK_WIDTH*BLOCK_WIDTH*RAYS_PER_PIXEL)     /** number of all rays*/

#define NUM_SLAVE_CORE          64                      /** number of slave cores*/

#define NUM_LABELS              8/**NUM_SLAVE_CORE*/    /** for Vertex cluster*/
#define NUM_BUNCHES             8       /** for Vector cluster*/

#define MAX_VERTEX_CLUSTER_ROUND        8/*4096*/       /** ▒*/
#define MAX_VECTOR_CLUSTER_ROUND        8

#define NUM_CLUSTERS            (NUM_LABELS*NUM_BUNCHES)        /** HINT:related to the width of render blocks*/

#ifndef NULL
#define NULL    0
#endif // !NULL

#ifndef Float
typedef float Float;
#endif // !Float

/*#ifndef Boolean
typedef enum Boolean { False, True } Boolean;
#endif // !bool*/

#ifndef Point2
typedef struct{
        Float x, y;
}Point2;
#endif // !Point2

#ifndef Point3
typedef struct{
        Float x, y, z;
}Point3;
#endif // !Point3

#ifndef Ray
typedef struct {
        /** !!!*/
        Point3 o;/** position*/
        Point3 d;/** direction*/
}Ray;
#endif // !Ray

#ifndef Spectrum
typedef struct{
        /** !!!*/
        int val;
}Spectrum;
#endif // !Spectrum

#ifndef RadianceQueryRecord
typedef struct{
        /** !!!*/
        int val;
}RadianceQueryRecord;
#endif // !RadianceQueryRecord

#ifndef RayItem
typedef struct{
        Ray ray;
        Float eta;
        unsigned short label;
        unsigned short bunch;
        bool hasNext, valid, nullChain, scattered;
        Spectrum throughput, Li;
        Point2 pixel;
        RadianceQueryRecord rRec;
}RayItem;
#endif // !RayItem

/** for count*/
#ifndef SLAVE_CNT
typedef struct{
        Point3 sum;
        int n;
}SLAVE_CNT;
#endif // !SLAVE_CNT

#ifndef min
#define min(a,b)        ((a)>(b)?(b):(a))
#endif // !min

#ifndef max
#define max(a,b)        ((a)>(b)?(a):(b))
#endif // !max

/** get the square distance of two Points
* */
Float get_D(const Point3 a, const Point3 b) {
        Float D = (a.x - b.x)*(a.x - b.x) + (a.y - b.y)*(a.y - b.y) + (a.z - b.z)*(a.z - b.z);
        return D;
}

/**
*/
Float Sqrt(Float x) {
        Float xhalf = 0.5f*x;
        int i = *(int*)&x;      // get bits for floating VALUE
        i = 0x5f375a86 - (i >> 1); // gives initial guess y0
        x = *(Float*)&i;        // convert bits BACK to float
        x = x*(1.5f - xhalf*x*x); // Newton step, repeating increases accuracy
        x = x*(1.5f - xhalf*x*x); // Newton step, repeating increases accuracy
        x = x*(1.5f - xhalf*x*x); // Newton step, repeating increases accuracy
        return 1 / x;
}

/** return a normallized vector
* */
Point3 norm(Point3 a) {
        Point3 d = a;
        Float dis = a.x*a.x + a.y*a.y + a.z*a.z;
        Float b = Sqrt(dis == 0 ? 1 : dis);
        d.x /= b; d.y /= b; d.z /= b;
        return d;
}

/**
* */
Float dot(const Point3 a, const Point3 b) {
        Float d = a.x*b.x + a.y*b.y + a.z*b.z;
        return d;
}

/**
* */
bool/*Boolean*/ isValid(const Point3 v) {
        if ((v.x == FLT_MAX) && (v.y == FLT_MAX) && (v.z == FLT_MAX)) return false/*False*/;
        else return true/*True*/;
}


/** set Point3 item inValid
* */
void setInvalid(Point3* v) {
        v->x = v->y = v->z = FLT_MAX;
}
