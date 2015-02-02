#include "path_finding.hpp"

#include "swarkland.hpp"
#include "heap.hpp"

#include <math.h>

static bool is_valid_move(Coord location) {
    if (actual_map_tiles[location].tile_type == TileType_WALL)
        return false;
    if (find_individual_at(location) != NULL)
        return false;
    return true;
}

// start with the cardinal directions, because these are more "direct"
static const Coord directions[] = {
    Coord(-1,  0),
    Coord( 0, -1),
    Coord( 1,  0),
    Coord( 0,  1),
    Coord(-1, -1),
    Coord( 1, -1),
    Coord( 1,  1),
    Coord(-1,  1),
};

struct Node {
    Coord coord;
    float f;
    float g;
    float h;
    Node * parent;
};

static float heuristic(Coord start, Coord end) {
    float delta_x = (float)(end.x - start.x);
    float delta_y = (float)(end.y - start.y);
    return sqrtf(delta_x * delta_x + delta_y * delta_y);
}

static int compare_nodes(Node *a, Node *b) {
    return signf(a->f - b->f);
}

bool find_path(Coord start, Coord end, List<Coord> & output_path) {
    Matrix<bool> closed_set(map_size.y, map_size.x);
    closed_set.set_all(false);

    Heap<Node*> open_heap(compare_nodes);
    Matrix<bool> open_set(map_size.y, map_size.x);
    open_set.set_all(false);

    Matrix<Node> nodes(map_size.y, map_size.x);
    Node *start_node = &nodes[start];
    start_node->coord = start;
    start_node->h = heuristic(start, end);
    start_node->g = 0.0;
    start_node->f = start_node->g + start_node->h;
    start_node->parent = NULL;
    open_heap.insert(start_node);
    open_set[start_node->coord] = true;
    bool found_goal = false;
    Node *best_node = start_node;
    while (open_heap.size() > 0) {
        Node *node = open_heap.extract_min();
        open_set[node->coord] = false;

        if (node->h < best_node->h)
            best_node = node;
        if (node->coord == end) {
            found_goal = true;
            break;
        }
        // not done yet
        closed_set[node->coord] = true;
        for (int i = 0; i < 8; i++) {
            Coord direction = directions[i];
            Coord neighbor_coord(node->coord.x + direction.x, node->coord.y + direction.y);
            if (neighbor_coord.y < 0 || neighbor_coord.y >= map_size.y ||
                    neighbor_coord.x < 0 || neighbor_coord.x >= map_size.x)
            {
                continue;
            }

            if (neighbor_coord != end && !is_valid_move(neighbor_coord))
                continue;
            if (closed_set[neighbor_coord])
                continue;

            Node *neighbor = &nodes[neighbor_coord];
            neighbor->coord = neighbor_coord;

            float g_from_this_node = node->g + 1.0f;
            if (!open_set[neighbor->coord] || g_from_this_node < neighbor->g) {
                neighbor->parent = node;
                neighbor->g = g_from_this_node;
                neighbor->h = heuristic(neighbor->coord, end);
                neighbor->f = neighbor->g + neighbor->h;
                if (!open_set[neighbor->coord]) {
                    open_heap.insert(neighbor);
                    open_set[neighbor->coord] = true;
                }
            }
        }
    }
    // construct path
    // take a double dump
    List<Coord> backwards_path;
    Node *it = best_node;
    while (it != NULL) {
        backwards_path.add(it->coord);
        it = it->parent;
    }
    for (int i = backwards_path.size() - 2; i >= 0; i--) {
        output_path.add(backwards_path.at(i));
    }
    return found_goal;
}
