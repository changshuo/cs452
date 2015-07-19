#include <user/pathfinding.h>
#include <debug.h>
#include <user/turnout.h>
#include <utils.h>
#include <heap.h>
#include <user/syscall.h>

typedef struct PathNode {
    struct PathNode *from;
    track_node *tn;
    int cost;
} PathNode;

/**
Because there is no malloc, a large buffer is preallocated for PathNode.
*/
#define EXPLORE_SIZE 4096
static PathNode g_nodes[EXPLORE_SIZE];

static inline void setPathNode(PathNode *p, PathNode *f, track_node *t, int c) {
    p->from = f;
    p->tn = t;
    p->cost = c;
}

static inline int nodeCost(PathNode *pn) {
    return pn->cost;
}

DECLARE_HEAP(PQHeap, PathNode*, nodeCost, 12, <);

/**
Returns the number of track_node pointers to from src to dst.
*/
int planRoute(track_node *src, track_node *dst, PathBuffer *pb) {
    uassert(src);
    uassert(dst);
    uassert(pb);
    memset(pb->tracknodes, 0, MAX_PATH_LENGTH);
    pb->length = 0;
    memset(g_nodes, 0, EXPLORE_SIZE);
    int num_nodes = 0;
    int train_num = pb->train_num;

    /**
    At any time in the PQ, it only contains the horizon PathNode.
    The PathNode with the minimum pathcost is popped from the PQ,
    and is followed for one more node, and pushed back onto the PQ.
    If the node followed was a branch, two child PathNode are created,
    one for each possible direction.
    Otherwise PathNode is created for the next track_node.
    */
    PathNode *start = &g_nodes[num_nodes++];
    PathNode *end;
    setPathNode(start, 0, src, 0);

    PQHeap pq;
    pq.count = 0;

    PQHeapPush(&pq, start);
    while (1) {
        uassert(num_nodes < EXPLORE_SIZE);

        PathNode *popd = PQHeapPop(&pq);
        if (dst == popd->tn) {
            end = popd;
            break;
        }

        if (popd->tn->type == NODE_EXIT) {
            continue;
        }

        if (popd->tn->owner != -1 && popd->tn->owner != train_num) {
            /**
            Make the cost effectively infinite high, so it is not considered.
            77950 is sum of all distances on tracks A & B.
            */
            popd->cost += 77950;
        }

        if (popd->tn->type == NODE_BRANCH) {
            PathNode *straight = &g_nodes[num_nodes++];
            setPathNode(straight,
                        popd,
                        popd->tn->edge[DIR_STRAIGHT].dest,
                        popd->cost + popd->tn->edge[DIR_STRAIGHT].dist);
            PQHeapPush(&pq, straight);

            PathNode *curved = &g_nodes[num_nodes++];
            setPathNode(curved,
                        popd,
                        popd->tn->edge[DIR_CURVED].dest,
                        popd->cost + popd->tn->edge[DIR_CURVED].dist);

            PQHeapPush(&pq, curved);
        }
        else {
            PathNode *ahead = &g_nodes[num_nodes++];
            setPathNode(ahead,
                        popd,
                        popd->tn->edge[DIR_AHEAD].dest,
                        popd->cost + popd->tn->edge[DIR_AHEAD].dist);
            PQHeapPush(&pq, ahead);
        }
    }

    /**
    We have found a path to dest, accessible via end.
    Construct the path from end to start, filling in PathBuffer.
    */
    PathNode *at = end;
    int path_length = 0;
    do {
        uassert(path_length < MAX_PATH_LENGTH);
        pb->tracknodes[path_length++] = at->tn;
        at = at->from;
    } while (at != start);
    pb->length = path_length;

    return path_length;
}

int shortestRoute(track_node *src, track_node *dst, PathBuffer *pathb) {
    PathBuffer pb[4];
    planRoute(src, dst, &pb[0]);
    planRoute(src, dst->reverse, &pb[1]);
    planRoute(src->reverse, dst, &pb[2]);
    planRoute(src->reverse, dst->reverse, &pb[3]);

    PathBuffer *lowest = &pb[0];
    for (int i = 1; i < 4; i++)
        if (lowest->length > pb[i].length)
            lowest = &pb[i];

    memcpy(pathb, lowest, sizeof(PathBuffer));
    return lowest->length;
}

void printPath(PathBuffer *pb) {
    String s;
    sinit(&s);
    sputstr(&s, "Path:\n\r");
    for (int i = 0; i < pb->length; i++) {
        sputstr(&s, "-> [");
        sputstr(&s, pb->tracknodes[i]->name);
        sputstr(&s, ",");
        sputint(&s, pb->tracknodes[i]->idx, 10);
        sputstr(&s, "] ");
    }
    sputstr(&s, "\n\r");
    PutString(COM2, &s);
}

track_edge *getNextEdge(track_node *node) {
    if(node->type == NODE_BRANCH) {
        return &node->edge[turnoutIsCurved(node->num)];
    }
    else if (node->type == NODE_EXIT) {
        return 0;
    }
    return &node->edge[DIR_AHEAD];
}

track_node *getNextNode(track_node *currentNode) {
    track_edge *next_edge = getNextEdge(currentNode);
    return (next_edge == 0 ? 0 : next_edge->dest);
}

track_node *getNextSensor(track_node *node) {
    uassert(node != 0);
    for (;;) {
        node = getNextNode(node);
        if (node == 0 || node->type == NODE_SENSOR) return node;
    }
    return 0;
}

// returns positive distance between two nodes in micrometers
// or -1 if the landmark is not found
int distanceBetween(track_node *from, track_node *to) {
    uassert(from != 0);
    uassert(to != 0);

    int totalDistance = 0;
    track_node *nextNode = from;
    for(int i = 0; i < TRACK_MAX / 2; i++) {
        // get next edge & node
        track_edge *nextEdge = getNextEdge(nextNode);
        nextNode = getNextNode(nextNode);
        if (nextEdge == 0 || nextNode == 0) break;

        totalDistance += nextEdge->dist;

        if (nextNode == to) return 1000 * totalDistance;
    }
    // unable to find the
    return -1;
}
