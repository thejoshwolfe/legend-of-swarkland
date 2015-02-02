#include "path_finding.hpp"

#include "swarkland.hpp"

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

bool find_path(Coord start, Coord end, List<Coord> & output_path) {
    Matrix<bool> visited(map_size.y, map_size.x);
    Matrix<Coord> next_node(map_size.y, map_size.x);
    for (int y = 0; y < map_size.y; y++)
        for (int x = 0; x < map_size.x; x++)
            visited[Coord(x, y)] = false;

    List<Coord> queue;
    queue.add(end);
    visited[end] = true;
    int index = 0;
    while (index < queue.size()) {
        Coord node = queue.at(index++);
        if (node == start) {
            // reconstruct the path
            while (node != end) {
                node = next_node[node];
                output_path.add(node);
            }
            return true;
        }
        for (int i = 0; i < 8; i++) {
            Coord direction = directions[i];
            Coord neighbor(node.x + direction.x, node.y + direction.y);
            if (neighbor.y < 0 || neighbor.y >= map_size.y || neighbor.x < 0 || neighbor.x >= map_size.x)
                continue;
            if (visited[neighbor])
                continue;
            visited[neighbor] = true;
            next_node[neighbor] = node;
            if (neighbor == start || neighbor == end || is_valid_move(neighbor))
                queue.add(neighbor);
        }
    }
    return false;
}
