#ifndef __PATHFINDING_H
#define __PATHFINDING_H
#include <user/track_data.h>

/**
Position for XMarksTheSpot stopping.
*/
struct Position {
    track_node *node;
    int offset;
};

#define MAX_PATH_LENGTH TRACK_MAX // very generous
/**
PathBuffer for passing around paths.
Array of track_node indecies, ordered from destination (0) to source (length).
*/
typedef struct PathBuffer {
    int train_num;   /* Engineer: train_number for reservation planning */
    track_node *tracknodes[MAX_PATH_LENGTH];
    int cost;
    int length;
} PathBuffer;

/**
planRoute
Returns -1 if no path exists, otherwise the length of the path.
*/
int planRoute(track_node *start, track_node *end, PathBuffer *pb);
void printPath(PathBuffer *pb);
int distanceBetween(track_node *from, track_node *to);
track_edge *getNextEdge(track_node *node);
track_node *getNextNode(track_node *currentNode);
track_node *getNextSensor(track_node *node);

#endif
