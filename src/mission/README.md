# Mission Execution State Machine (`src/mission/`)

This directory implements the central Finite State Machine (FSM) governing AGV operational states, safety interlocks, and mission transitions.

---

## 1. Operational State Machine

```
              +-------------------------------------------------+
              |                     BOOTUP                      |
              +-------------------------------------------------+
                                       |
                                       v
    +-----------------------------> [ IDLE ] <----------------------------+
    |                                  |                                  |
    |          +-----------------------+-----------------------+          |
    |          |                       |                       |          |
    |          v                       v                       v          |
    |     [ MANUAL ]              [ TEACH ]               [ REPEAT ]      |
    |          |                       |                       |          |
    |          +-----------------------+-----------------------+          |
    |                                  |                                  |
    |             (Mission Stop / Normal Completion)                   |
    |                                                                     |
    |                                  v                                  |
    +---------------------------- [ E-STOP ] <----------------------------+
                                (Fault / User Abort)
```

---

## 2. State Descriptions

- **`IDLE`**: Motors are stopped and unpowered. System broadcasts telemetry at 20Hz and waits for UI commands.
- **`MANUAL`**: Direct manual teleoperation via virtual joystick or hardware inputs.
- **`TEACH`**: Recording mode. The AGV records odometry deltas and RFID checkpoints into an active trajectory file.
- **`REPEAT`**: Autonomous execution mode. The AGV plays back a recorded trajectory or follows a computed Dijkstra shortest path between RFID nodes.
- **`ESTOP`**: Hardware fail-safe state. Motor PWM is killed, electronic braking is applied, and all ongoing autonomous missions are terminated immediately.

---

## 3. Safety Interlocks & Collision Avoidance

During `REPEAT` mode:
1. **Obstacle Detection**: If any ToF sensor reads $< \text{stop\_distance}$, the FSM enters a temporary `PAUSED_OBSTACLE` state, commanding zero velocity.
2. **Auto-Resume**: When the obstacle clears for $> 1.0\text{s}$, the AGV automatically resumes tracking its trajectory.
3. **Emergency Disconnect**: If UART communication with the STM32 drops for $> 1.5\text{s}$, the FSM immediately forces `ESTOP`.
