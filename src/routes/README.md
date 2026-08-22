# Route Trajectory & Topological Graph Navigation (`src/routes/`)

This directory houses the route recording, trajectory replay, and topological shortest-path graph solver for the TARSLIFT AGV.

---

## 1. Topological Graph & Dijkstra Shortest Path Theory

The navigation environment is modeled as a directed graph $G = (V, E)$:
- **Vertices ($V$)**: Ground-truth RFID station tags.
- **Edges ($E$)**: Recorded path trajectories connecting pair $(u, v)$ with distance weight $w(u, v)$.

### Dijkstra Algorithm Implementation
When an operator requests navigation from Start Station $S$ to Target Station $D$:
1. Initialize distance array: $\text{dist}[v] = \infty$ for all $v \in V$, and $\text{dist}[S] = 0$.
2. Maintain a priority queue / unvisited set of nodes.
3. For the current node $u$ with minimum distance:
   $$\text{For each neighbor } v \text{ of } u: \quad \text{if } \text{dist}[u] + w(u, v) < \text{dist}[v] \implies \text{dist}[v] = \text{dist}[u] + w(u, v)$$
4. Reconstruct the shortest edge sequence from $S$ to $D$.
5. Chain each edge's waypoint trajectory and dispatch to the motor controller.

---

## 2. Route JSON Format (`/routes/{route_id}.json`)

```json
{
  "id": "route_001",
  "name": "Station A to Station B",
  "start_rfid": "1342672322",
  "end_rfid": "2451789012",
  "total_distance_mm": 4500,
  "segments": [
    {
      "type": "MOVE",
      "distance_mm": 3000,
      "speed_mm_s": 250
    },
    {
      "type": "TURN",
      "angle_deg": 90.0,
      "speed_deg_s": 45
    },
    {
      "type": "MOVE",
      "distance_mm": 1500,
      "speed_mm_s": 200
    }
  ]
}
```

---

## 3. Waypoint Replay & Trajectory Execution

During playback:
1. The `RouteManager` iterates through each segment.
2. For `MOVE` segments: Sends velocity commands to STM32 and tracks encoder odometry until `current_dist >= target_dist`.
3. For `TURN` segments: Sends rotation commands and monitors MPU6050 Gyro Z integration until `current_yaw >= target_yaw`.
4. Arriving at the destination RFID tag completes the mission and transitions the FSM to `IDLE`.
