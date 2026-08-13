const http = require('http');
const fs = require('fs');
const path = require('path');
const { WebSocketServer } = require('ws');

const PORT = 8080;

// Application State
let agvState = {
    mode: "IDLE",
    state: "STOPPED",
    x: 0.0,
    y: 0.0,
    heading: 0.0,
    speed: 0.0,
    rfid: "START",
    battery: 78.0,
    voltage: 11.6,
    current: 0.45,
    tof: { left: 1500, centre: 1800, right: 1400 },
    motor: { left_rpm: 0, right_rpm: 0, target_rpm: 0 },
    health: {
        esp32: 2, // 2 = REAL (Green)
        stm32: 1, // 1 = DEMO (Blue)
        motor: 1,
        encoder: 1,
        mpu6050: 1,
        rfid: 1,
        tof: 1,
        battery: 1,
        uart: 1
    },
    uptime: 0,
    fs_total: 2097152,
    fs_used: 49152,
    active_route: "",
    checkpoint_idx: 0,
    checkpoint_total: 0,
    mission_progress: 0,
    active_error: { active: false, timestamp: "", source: "", code: "", description: "" }
};

// Global settings state
let settingsState = {
    test_profile: 1, // Default to MOTOR_TEST
    enable_dashboard: true,
    enable_teach_mode: true,
    enable_repeat_mode: false,
    enable_manual_control: true,
    enable_route_manager: false,
    enable_rfid_manager: false,
    enable_error_log: true,
    enable_system_info: true,
    enable_stm32_uart: true,
    enable_websocket: true,
    enable_motor_control: true,
    enable_encoder: false,
    enable_mpu6050: false,
    enable_pid: false,
    rfid_reader: false,
    rfid_manager_flag: false,
    rfid_checkpoints: false,
    tof_sensors: false,
    left_tof: false,
    centre_tof: false,
    right_tof: false,
    obstacle_detection: false,
    battery_monitoring: false,
    ina219: false,
    voltage_monitoring: false,
    current_monitoring: false,
    power_monitoring: false,
    battery_percentage: false,
    low_battery_warning: false,
    critical_battery_warning: false,
    battery_fault_detection: false,
    charging_status: false,
    low_voltage: 10.8,
    critical_voltage: 10.2,
    low_battery_pct: 20,
    critical_battery_pct: 10,
    demo_mode: false,
    wifi_ssid: "TARSLIFT_AGV",
    wifi_password: "12345678"
};

function applyProfileDefaultsMock(profileIndex) {
    settingsState.test_profile = profileIndex;
    
    settingsState.enable_dashboard = true;
    settingsState.enable_teach_mode = false;
    settingsState.enable_repeat_mode = false;
    settingsState.enable_manual_control = false;
    settingsState.enable_route_manager = false;
    settingsState.enable_rfid_manager = false;
    settingsState.enable_error_log = true;
    settingsState.enable_system_info = true;

    settingsState.enable_stm32_uart = false;
    settingsState.enable_websocket = true;

    settingsState.enable_motor_control = false;
    settingsState.enable_encoder = false;
    settingsState.enable_mpu6050 = false;
    settingsState.enable_pid = false;

    settingsState.rfid_reader = false;
    settingsState.rfid_manager_flag = false;
    settingsState.rfid_checkpoints = false;

    settingsState.tof_sensors = false;
    settingsState.left_tof = false;
    settingsState.centre_tof = false;
    settingsState.right_tof = false;
    settingsState.obstacle_detection = false;

    settingsState.battery_monitoring = false;
    settingsState.ina219 = false;
    settingsState.voltage_monitoring = false;
    settingsState.current_monitoring = false;
    settingsState.power_monitoring = false;
    settingsState.battery_percentage = false;
    settingsState.low_battery_warning = false;
    settingsState.critical_battery_warning = false;
    settingsState.battery_fault_detection = false;
    settingsState.charging_status = false;

    settingsState.low_voltage = 10.8;
    settingsState.critical_voltage = 10.2;
    settingsState.low_battery_pct = 20;
    settingsState.critical_battery_pct = 10;
    settingsState.demo_mode = false;

    switch(profileIndex) {
        case 1: // MOTOR_TEST
            settingsState.enable_manual_control = true;
            settingsState.enable_teach_mode = true;
            settingsState.enable_stm32_uart = true;
            settingsState.enable_motor_control = true;
            break;
        case 2: // ENCODER_TEST
            settingsState.enable_stm32_uart = true;
            settingsState.enable_motor_control = true;
            settingsState.enable_encoder = true;
            break;
        case 3: // MPU6050_TEST
            settingsState.enable_stm32_uart = true;
            settingsState.enable_mpu6050 = true;
            break;
        case 4: // RFID_TEST
            settingsState.enable_stm32_uart = true;
            settingsState.rfid_reader = true;
            settingsState.rfid_manager_flag = true;
            break;
        case 5: // TOF_TEST
            settingsState.enable_stm32_uart = true;
            settingsState.tof_sensors = true;
            settingsState.obstacle_detection = true;
            break;
        case 6: // BATTERY_TEST
            settingsState.battery_monitoring = true;
            settingsState.ina219 = true;
            settingsState.voltage_monitoring = true;
            settingsState.current_monitoring = true;
            settingsState.power_monitoring = true;
            settingsState.battery_percentage = true;
            settingsState.low_battery_warning = true;
            settingsState.critical_battery_warning = true;
            settingsState.battery_fault_detection = true;
            break;
        case 7: // TEACH_TEST
            settingsState.enable_manual_control = true;
            settingsState.enable_teach_mode = true;
            settingsState.enable_route_manager = true;
            settingsState.enable_stm32_uart = true;
            settingsState.enable_motor_control = true;
            settingsState.enable_encoder = true;
            settingsState.enable_mpu6050 = true;
            settingsState.rfid_reader = true;
            break;
        case 8: // REPEAT_TEST
            settingsState.enable_repeat_mode = true;
            settingsState.enable_route_manager = true;
            settingsState.enable_stm32_uart = true;
            settingsState.enable_motor_control = true;
            settingsState.enable_encoder = true;
            settingsState.enable_mpu6050 = true;
            settingsState.rfid_reader = true;
            settingsState.tof_sensors = true;
            settingsState.battery_monitoring = true;
            break;
        case 9: // FULL_SYSTEM
            settingsState.enable_teach_mode = true;
            settingsState.enable_repeat_mode = true;
            settingsState.enable_manual_control = true;
            settingsState.enable_route_manager = true;
            settingsState.enable_rfid_manager = true;
            settingsState.enable_stm32_uart = true;
            settingsState.enable_motor_control = true;
            settingsState.enable_encoder = true;
            settingsState.enable_mpu6050 = true;
            settingsState.enable_pid = true;
            settingsState.rfid_reader = true;
            settingsState.rfid_manager_flag = true;
            settingsState.rfid_checkpoints = true;
            settingsState.tof_sensors = true;
            settingsState.left_tof = true;
            settingsState.centre_tof = true;
            settingsState.right_tof = true;
            settingsState.obstacle_detection = true;
            settingsState.battery_monitoring = true;
            settingsState.ina219 = true;
            settingsState.voltage_monitoring = true;
            settingsState.current_monitoring = true;
            settingsState.power_monitoring = true;
            settingsState.battery_percentage = true;
            settingsState.low_battery_warning = true;
            settingsState.critical_battery_warning = true;
            settingsState.battery_fault_detection = true;
            break;
        case 0: // CUSTOM
        default:
            settingsState.enable_teach_mode = true;
            settingsState.enable_repeat_mode = true;
            settingsState.enable_manual_control = true;
            settingsState.enable_route_manager = true;
            settingsState.enable_rfid_manager = true;
            settingsState.demo_mode = true;
            break;
    }
}

function enforceSettingsDependenciesMock() {
    if (!settingsState.battery_monitoring) {
        settingsState.ina219 = false;
        settingsState.voltage_monitoring = false;
        settingsState.current_monitoring = false;
        settingsState.power_monitoring = false;
        settingsState.battery_percentage = false;
        settingsState.low_battery_warning = false;
        settingsState.critical_battery_warning = false;
        settingsState.battery_fault_detection = false;
    }
    if (!settingsState.rfid_reader) {
        settingsState.rfid_manager_flag = false;
        settingsState.rfid_checkpoints = false;
    }
    if (!settingsState.tof_sensors) {
        settingsState.left_tof = false;
        settingsState.centre_tof = false;
        settingsState.right_tof = false;
        settingsState.obstacle_detection = false;
    }
}

// Simulation variables
let manualDirection = "STOP";
let manualSpeedPercent = 50;
let lastUpdate = Date.now();
let startTime = Date.now();

// Teach Recording Data
let teachRoute = null;
let teachPoints = [];
let teachDistance = 0.0;

// Repeat Playback Data
let repeatRoute = null;
let repeatCheckpoints = [];
let repeatProgressTimer = null;


// RFID scanning trigger
let isRfidScanning = false;
let rfidScanTimer = null;

// Mock Web Serial Console logs
let mockSerialLogs = [
    "Welcome to TARSLIFT AGV Live Debug Console. Type /help for list of command parameters."
];

setInterval(() => {
    const d = new Date();
    const timeStr = `${d.getHours().toString().padStart(2, '0')}:${d.getMinutes().toString().padStart(2, '0')}:${d.getSeconds().toString().padStart(2, '0')}`;
    if (Math.random() > 0.5) {
        mockSerialLogs.push(`[${timeStr}] STM32 Coprocessor ping ok. Telemetry frequency: 5Hz`);
    } else {
        mockSerialLogs.push(`[${timeStr}] Diagnostics check: Heap Free: 184,240 bytes`);
    }
    if (mockSerialLogs.length > 50) {
        mockSerialLogs.shift();
    }
}, 8000);

// Update Loop (Runs at 20Hz internally for smooth integration)
setInterval(() => {
    const now = Date.now();
    const dt = (now - lastUpdate) / 1000;
    lastUpdate = now;

    agvState.uptime = Math.floor((now - startTime) / 1000);

    // Sync health stats with settings
    agvState.health.esp32 = 2; // REAL
    agvState.health.uart = settingsState.enable_stm32_uart ? 2 : 0;
    agvState.health.stm32 = settingsState.enable_stm32_uart ? 2 : 0;
    agvState.health.motor = settingsState.enable_motor_control ? (settingsState.demo_mode ? 1 : 2) : 0;
    agvState.health.encoder = settingsState.enable_encoder ? (settingsState.demo_mode ? 1 : 2) : 0;
    agvState.health.mpu6050 = settingsState.enable_mpu6050 ? (settingsState.demo_mode ? 1 : 2) : 0;
    agvState.health.rfid = settingsState.rfid_reader ? (settingsState.demo_mode ? 1 : 2) : 0;
    agvState.health.tof = settingsState.tof_sensors ? (settingsState.demo_mode ? 1 : 2) : 0;
    agvState.health.battery = settingsState.battery_monitoring ? (settingsState.demo_mode ? 1 : 2) : 0;

    // Simulate battery drainage
    if (settingsState.demo_mode && settingsState.battery_state !== 0) {
        agvState.battery -= 0.001 * dt;
        if (agvState.battery < 0) agvState.battery = 100.0;
        agvState.voltage = 9.0 + (agvState.battery / 100.0) * 3.6;
    } else if (settingsState.battery_state === 0) {
        agvState.battery = 0;
        agvState.voltage = 0.0;
    }

    // Simulate ToF sensors fluctuating
    if (settingsState.demo_mode && settingsState.tof_state !== 0) {
        agvState.tof.left = Math.max(150, Math.min(2000, agvState.tof.left + Math.floor(Math.random() * 41) - 20));
        agvState.tof.centre = Math.max(150, Math.min(2000, agvState.tof.centre + Math.floor(Math.random() * 51) - 25));
        agvState.tof.right = Math.max(150, Math.min(2000, agvState.tof.right + Math.floor(Math.random() * 41) - 20));
    } else {
        agvState.tof.left = 0;
        agvState.tof.centre = 0;
        agvState.tof.right = 0;
    }

    // Handle teach mode driving
    if ((agvState.mode === "TEACH" && agvState.state === "RECORDING") || (agvState.mode === "IDLE" && manualDirection !== "STOP")) {
        const speedVal = (manualSpeedPercent / 100.0) * 0.5; // Max 0.5 m/s

        // When driving in IDLE mode, set mode to MANUAL for dashboard feedback
        if (agvState.mode === "IDLE" && manualDirection !== "STOP") {
            agvState.state = "MANUAL_DRIVE";
        }

        if (manualDirection === "FORWARD") {
            agvState.speed = speedVal;
            agvState.motor.left_rpm = manualSpeedPercent * 2;
            agvState.motor.right_rpm = manualSpeedPercent * 2;
            agvState.motor.target_rpm = manualSpeedPercent * 2;

            const rad = (agvState.heading * Math.PI) / 180;
            const dx = agvState.speed * Math.cos(rad) * dt;
            const dy = agvState.speed * Math.sin(rad) * dt;
            agvState.x += dx;
            agvState.y += dy;
            if (agvState.mode === "TEACH") {
                teachDistance += Math.sqrt(dx * dx + dy * dy);
                teachPoints.push({
                    x: Math.round(agvState.x * 100) / 100,
                    y: Math.round(agvState.y * 100) / 100,
                    heading: Math.round(agvState.heading * 10) / 10,
                    rfid: agvState.rfid
                });
            }
        } 
        else if (manualDirection === "REVERSE") {
            agvState.speed = -speedVal;
            agvState.motor.left_rpm = -manualSpeedPercent * 2;
            agvState.motor.right_rpm = -manualSpeedPercent * 2;
            agvState.motor.target_rpm = -manualSpeedPercent * 2;

            const rad = (agvState.heading * Math.PI) / 180;
            const dx = agvState.speed * Math.cos(rad) * dt;
            const dy = agvState.speed * Math.sin(rad) * dt;
            agvState.x += dx;
            agvState.y += dy;
            if (agvState.mode === "TEACH") {
                teachDistance += Math.sqrt(dx * dx + dy * dy);
                teachPoints.push({
                    x: Math.round(agvState.x * 100) / 100,
                    y: Math.round(agvState.y * 100) / 100,
                    heading: Math.round(agvState.heading * 10) / 10,
                    rfid: agvState.rfid
                });
            }
        } 
        else if (manualDirection === "LEFT") {
            agvState.speed = 0;
            agvState.motor.left_rpm = -manualSpeedPercent;
            agvState.motor.right_rpm = manualSpeedPercent;
            agvState.motor.target_rpm = manualSpeedPercent;
            agvState.heading = (agvState.heading - 45 * dt + 360) % 360;
        } 
        else if (manualDirection === "RIGHT") {
            agvState.speed = 0;
            agvState.motor.left_rpm = manualSpeedPercent;
            agvState.motor.right_rpm = -manualSpeedPercent;
            agvState.motor.target_rpm = manualSpeedPercent;
            agvState.heading = (agvState.heading + 45 * dt) % 360;
        } 
        else {
            agvState.speed = 0;
            agvState.motor.left_rpm = 0;
            agvState.motor.right_rpm = 0;
            agvState.motor.target_rpm = 0;
            if (agvState.mode === "IDLE") {
                agvState.state = "STOPPED";
            }
        }
    }

    // Proximity RFID detection in manual driving
    if (agvState.mode === "TEACH" || agvState.mode === "IDLE") {
        const checkPoints = [
            { x: 0.0, y: 0.0, rfid: "START" },
            { x: 2.0, y: 0.0, rfid: "LAB" },
            { x: 2.0, y: 2.0, rfid: "STORAGE" },
            { x: 0.0, y: 2.0, rfid: "OFFICE" }
        ];

        let closest = "NONE";
        checkPoints.forEach(cp => {
            const dx = agvState.x - cp.x;
            const dy = agvState.y - cp.y;
            const dist = Math.sqrt(dx * dx + dy * dy);
            if (dist < 0.3) {
                closest = cp.rfid;
            }
        });
        agvState.rfid = closest;
    }
}, 50);

// HTTP Static and API Server
const server = http.createServer((req, res) => {
    // Add CORS headers for testing
    res.setHeader('Access-Control-Allow-Origin', '*');
    res.setHeader('Access-Control-Allow-Methods', 'GET, POST, OPTIONS');
    res.setHeader('Access-Control-Allow-Headers', 'Content-Type');

    if (req.method === 'OPTIONS') {
        res.writeHead(200);
        res.end();
        return;
    }

    const parsedUrl = new URL(req.url, `http://${req.headers.host}`);
    const pathname = parsedUrl.pathname;

    // 1. GET /api/status
    if (pathname === '/api/status' && req.method === 'GET') {
        res.writeHead(200, { 'Content-Type': 'application/json' });
        res.end(JSON.stringify({
            mode: agvState.mode,
            state: agvState.state,
            x: agvState.x,
            y: agvState.y,
            heading: agvState.heading,
            speed: agvState.speed,
            rfid: agvState.rfid,
            battery: Math.round(agvState.battery)
        }));
        return;
    }

    // 2. GET /api/routes
    if (pathname === '/api/routes' && req.method === 'GET') {
        res.writeHead(200, { 'Content-Type': 'application/json' });
        const routesDir = path.join(__dirname, 'data', 'routes');
        if (!fs.existsSync(routesDir)) {
            res.end(JSON.stringify([]));
            return;
        }

        const files = fs.readdirSync(routesDir);
        const routes = [];
        files.forEach(file => {
            if (file.endsWith('.json')) {
                try {
                    const content = fs.readFileSync(path.join(routesDir, file), 'utf8');
                    routes.push(JSON.parse(content));
                } catch (e) {
                    console.error("Corrupted route file", file);
                }
            }
        });
        res.end(JSON.stringify(routes));
        return;
    }

    // 3. GET /api/rfid
    if (pathname === '/api/rfid' && req.method === 'GET') {
        res.writeHead(200, { 'Content-Type': 'application/json' });
        const filePath = path.join(__dirname, 'data', 'rfid', 'tags.json');
        if (fs.existsSync(filePath)) {
            res.end(fs.readFileSync(filePath, 'utf8'));
        } else {
            res.end(JSON.stringify({ tags: [] }));
        }
        return;
    }

    // 4. GET /api/errors
    if (pathname === '/api/errors' && req.method === 'GET') {
        res.writeHead(200, { 'Content-Type': 'application/json' });
        const logPath = path.join(__dirname, 'data', 'logs', '2026-08-13.log');
        let logContent = "";
        if (fs.existsSync(logPath)) {
            logContent = fs.readFileSync(logPath, 'utf8');
        }

        // Count category statistics
        const categories = { UART: 0, RFID: 0, MPU6050: 0, ToF: 0, Encoder: 0, Motor: 0, Battery: 0, System: 0 };
        let total = 0;
        if (logContent) {
            logContent.split('\n').forEach(line => {
                if (!line.trim()) return;
                total++;
                const parts = line.split('\t');
                const source = parts[1];
                if (categories[source] !== undefined) categories[source]++;
                else categories.System++;
            });
        }

        res.end(JSON.stringify({
            total: total,
            categories: categories,
            log: logContent
        }));
        return;
    }

    // 5. GET /api/system
    if (pathname === '/api/system' && req.method === 'GET') {
        res.writeHead(200, { 'Content-Type': 'application/json' });
        res.end(JSON.stringify({
            uptime: agvState.uptime,
            heap: 184240,
            rssi: -45,
            fs_total: agvState.fs_total,
            fs_used: agvState.fs_used,
            health: agvState.health
        }));
        return;
    }

    // 6. POST Commands
    if (req.method === 'POST') {
        let body = '';
        req.on('data', chunk => body += chunk);
        req.on('end', () => {
            let json = {};
            if (body) {
                try { json = JSON.parse(body); } catch (e) {}
            }

            // POST /api/teach/start
            if (pathname === '/api/teach/start') {
                agvState.mode = "TEACH";
                agvState.state = "RECORDING";
                agvState.x = 0.0;
                agvState.y = 0.0;
                agvState.heading = 0.0;
                agvState.speed = 0.0;
                teachDistance = 0.0;
                teachPoints = [{ x: 0.0, y: 0.0, heading: 0.0, rfid: json.start || "START" }];
                
                teachRoute = {
                    id: "route_" + Date.now(),
                    name: json.name || "Warehouse to Office",
                    start: json.start || "START",
                    destination: json.destination || "OFFICE",
                    checkpoints: [json.start || "START"],
                    distance: 0.0,
                    duration: 0,
                    created: new Date().toISOString().split('T')[0],
                    trajectory: []
                };
                agvState.active_route = teachRoute.name;

                res.writeHead(200, { 'Content-Type': 'application/json' });
                res.end(JSON.stringify({ status: "success", message: "Teaching started" }));
                return;
            }

            // POST /api/teach/stop
            if (pathname === '/api/teach/stop') {
                if (teachRoute) {
                    teachRoute.distance = Math.round(teachDistance * 100) / 100;
                    teachRoute.duration = Math.floor((Date.now() - startTime) / 1000);
                    teachRoute.trajectory = teachPoints;
                    
                    // Add intermediate and final checkpoints
                    teachPoints.forEach(pt => {
                        if (pt.rfid && pt.rfid !== "NONE" && !teachRoute.checkpoints.includes(pt.rfid)) {
                            teachRoute.checkpoints.push(pt.rfid);
                        }
                    });
                    if (!teachRoute.checkpoints.includes(teachRoute.destination)) {
                        teachRoute.checkpoints.push(teachRoute.destination);
                    }

                    // Save route file
                    const filePath = path.join(__dirname, 'data', 'routes', `${teachRoute.id}.json`);
                    fs.writeFileSync(filePath, JSON.stringify(teachRoute, null, 2));

                    agvState.mode = "IDLE";
                    agvState.state = "STOPPED";
                    agvState.active_route = "";
                    teachRoute = null;

                    res.writeHead(200, { 'Content-Type': 'application/json' });
                    res.end(JSON.stringify({ status: "success", message: "Route saved successfully" }));
                } else {
                    res.writeHead(400, { 'Content-Type': 'application/json' });
                    res.end(JSON.stringify({ status: "error", message: "No active teaching session" }));
                }
                return;
            }

            // POST /api/route/delete
            if (pathname === '/api/route/delete') {
                const routeId = json.id;
                const filePath = path.join(__dirname, 'data', 'routes', `${routeId}.json`);
                if (fs.existsSync(filePath)) {
                    fs.unlinkSync(filePath);
                    res.writeHead(200, { 'Content-Type': 'application/json' });
                    res.end(JSON.stringify({ status: "success", message: "Route deleted" }));
                } else {
                    res.writeHead(400, { 'Content-Type': 'application/json' });
                    res.end(JSON.stringify({ status: "error", message: "Route not found" }));
                }
                return;
            }

            // POST /api/repeat/start
            if (pathname === '/api/repeat/start') {
                const routeId = json.id;
                const filePath = path.join(__dirname, 'data', 'routes', `${routeId}.json`);
                if (fs.existsSync(filePath)) {
                    const content = fs.readFileSync(filePath, 'utf8');
                    repeatRoute = JSON.parse(content);
                    repeatCheckpoints = repeatRoute.checkpoints;

                    agvState.mode = "REPEAT";
                    agvState.state = "RUNNING";
                    agvState.active_route = repeatRoute.name;
                    agvState.checkpoint_idx = 0;
                    agvState.checkpoint_total = repeatCheckpoints.length;
                    agvState.rfid = repeatCheckpoints[0];
                    agvState.mission_progress = 0;

                    // Simulate route progress timeline
                    if (repeatProgressTimer) clearInterval(repeatProgressTimer);
                    let step = 0;
                    repeatProgressTimer = setInterval(() => {
                        if (agvState.state === "RUNNING") {
                            step++;
                            agvState.checkpoint_idx = Math.min(step, repeatCheckpoints.length - 1);
                            agvState.rfid = repeatCheckpoints[agvState.checkpoint_idx];
                            agvState.mission_progress = Math.round((agvState.checkpoint_idx / (repeatCheckpoints.length - 1)) * 100);

                            // Simulate movement path
                            agvState.x = 1.0 + Math.cos(step * 0.5);
                            agvState.y = 1.0 + Math.sin(step * 0.5);
                            agvState.heading = (step * 30) % 360;

                            if (step >= repeatCheckpoints.length - 1) {
                                clearInterval(repeatProgressTimer);
                                agvState.state = "COMPLETE";
                                agvState.mode = "IDLE";
                                agvState.active_route = "";
                            }
                        }
                    }, 5000); // Progress to next checkpoint every 5 seconds

                    res.writeHead(200, { 'Content-Type': 'application/json' });
                    res.end(JSON.stringify({ status: "success", message: "Mission started" }));
                } else {
                    res.writeHead(400, { 'Content-Type': 'application/json' });
                    res.end(JSON.stringify({ status: "error", message: "Route not found" }));
                }
                return;
            }

            // POST /api/repeat/pause
            if (pathname === '/api/repeat/pause') {
                if (agvState.mode === "REPEAT") {
                    agvState.state = "PAUSED";
                    res.writeHead(200, { 'Content-Type': 'application/json' });
                    res.end(JSON.stringify({ status: "success", message: "Mission paused" }));
                } else {
                    res.writeHead(400, { 'Content-Type': 'application/json' });
                    res.end(JSON.stringify({ status: "error", message: "Not in repeat mode" }));
                }
                return;
            }

            // POST /api/repeat/resume
            if (pathname === '/api/repeat/resume') {
                if (agvState.mode === "REPEAT") {
                    agvState.state = "RUNNING";
                    res.writeHead(200, { 'Content-Type': 'application/json' });
                    res.end(JSON.stringify({ status: "success", message: "Mission resumed" }));
                } else {
                    res.writeHead(400, { 'Content-Type': 'application/json' });
                    res.end(JSON.stringify({ status: "error", message: "Not in repeat mode" }));
                }
                return;
            }

            // POST /api/repeat/stop
            if (pathname === '/api/repeat/stop') {
                if (agvState.mode === "REPEAT") {
                    if (repeatProgressTimer) clearInterval(repeatProgressTimer);
                    agvState.mode = "IDLE";
                    agvState.state = "STOPPED";
                    agvState.active_route = "";
                    res.writeHead(200, { 'Content-Type': 'application/json' });
                    res.end(JSON.stringify({ status: "success", message: "Mission stopped" }));
                } else {
                    res.writeHead(400, { 'Content-Type': 'application/json' });
                    res.end(JSON.stringify({ status: "error", message: "Not in repeat mode" }));
                }
                return;
            }

            // POST /api/manual/move
            if (pathname === '/api/manual/move') {
                manualDirection = json.direction || "STOP";
                manualSpeedPercent = json.speed || 50;
                res.writeHead(200, { 'Content-Type': 'application/json' });
                res.end(JSON.stringify({ status: "success", message: "Manual move command sent" }));
                return;
            }

            // POST /api/manual/stop
            if (pathname === '/api/manual/stop') {
                manualDirection = "STOP";
                res.writeHead(200, { 'Content-Type': 'application/json' });
                res.end(JSON.stringify({ status: "success", message: "Manual stop command sent" }));
                return;
            }

            // POST /api/rfid/add
            if (pathname === '/api/rfid/add') {
                const filePath = path.join(__dirname, 'data', 'rfid', 'tags.json');
                let tagsDb = { tags: [] };
                if (fs.existsSync(filePath)) {
                    tagsDb = JSON.parse(fs.readFileSync(filePath, 'utf8'));
                }

                const existingIdx = tagsDb.tags.findIndex(t => t.uid === json.uid);
                if (existingIdx !== -1) {
                    tagsDb.tags[existingIdx].name = json.name;
                    tagsDb.tags[existingIdx].location = json.location;
                } else {
                    tagsDb.tags.push({ uid: json.uid, name: json.name, location: json.location });
                }

                fs.writeFileSync(filePath, JSON.stringify(tagsDb, null, 2));
                res.writeHead(200, { 'Content-Type': 'application/json' });
                res.end(JSON.stringify({ status: "success", message: "RFID tag added" }));
                return;
            }

            // POST /api/rfid/scan
            if (pathname === '/api/rfid/scan') {
                isRfidScanning = true;
                if (rfidScanTimer) clearTimeout(rfidScanTimer);
                
                // Simulate card scanning detection after 2 seconds
                rfidScanTimer = setTimeout(() => {
                    const uids = ["04A7329B6C", "04B2187F21", "04C59133A2", "04D8421190"];
                    const randomUid = uids[Math.floor(Math.random() * uids.length)];
                    agvState.rfid = randomUid;
                    isRfidScanning = false;
                    console.log(`RFID Mock: Detected tag ${randomUid}`);
                }, 2000);

                res.writeHead(200, { 'Content-Type': 'application/json' });
                res.end(JSON.stringify({ status: "success", message: "RFID scanning initialized" }));
                return;
            }

            // POST /api/rfid/delete
            if (pathname === '/api/rfid/delete') {
                const filePath = path.join(__dirname, 'data', 'rfid', 'tags.json');
                if (fs.existsSync(filePath)) {
                    const tagsDb = JSON.parse(fs.readFileSync(filePath, 'utf8'));
                    tagsDb.tags = tagsDb.tags.filter(t => t.uid !== json.uid);
                    fs.writeFileSync(filePath, JSON.stringify(tagsDb, null, 2));
                }
                res.writeHead(200, { 'Content-Type': 'application/json' });
                res.end(JSON.stringify({ status: "success", message: "RFID tag deleted" }));
                return;
            }

            // POST /api/errors/clear
            if (pathname === '/api/errors/clear') {
                const logPath = path.join(__dirname, 'data', 'logs', '2026-08-13.log');
                if (fs.existsSync(logPath)) {
                    fs.writeFileSync(logPath, "");
                }
                res.writeHead(200, { 'Content-Type': 'application/json' });
                res.end(JSON.stringify({ status: "success", message: "Error logs cleared" }));
                return;
            }

            // POST /api/settings
            if (pathname === '/api/settings') {
                if (json.test_profile !== undefined) {
                    settingsState.test_profile = json.test_profile;
                    if (json.test_profile > 0) {
                        applyProfileDefaultsMock(json.test_profile);
                    }
                }
                if (settingsState.test_profile === 0) { // Custom
                    Object.assign(settingsState, json);
                } else {
                    // Even on presets, thresholds can be edited
                    if (json.low_voltage !== undefined) settingsState.low_voltage = json.low_voltage;
                    if (json.critical_voltage !== undefined) settingsState.critical_voltage = json.critical_voltage;
                    if (json.low_battery_pct !== undefined) settingsState.low_battery_pct = json.low_battery_pct;
                    if (json.critical_battery_pct !== undefined) settingsState.critical_battery_pct = json.critical_battery_pct;
                    if (json.wifi_ssid !== undefined) settingsState.wifi_ssid = json.wifi_ssid;
                    if (json.wifi_password !== undefined) settingsState.wifi_password = json.wifi_password;
                    if (json.demo_mode !== undefined) settingsState.demo_mode = json.demo_mode;
                }
                enforceSettingsDependenciesMock();
                res.writeHead(200, { 'Content-Type': 'application/json' });
                res.end(JSON.stringify({ status: "success", message: "Settings saved" }));
                return;
            }

            // POST /api/serial/command
            if (pathname === '/api/serial/command') {
                const cmd = json.command;
                mockSerialLogs.push("> " + cmd);
                
                if (cmd.startsWith("/")) {
                    const trimmed = cmd.trim().toLowerCase();
                    if (trimmed === "/help") {
                        mockSerialLogs.push("Available Commands:");
                        mockSerialLogs.push("  /help   - Show this help message");
                        mockSerialLogs.push("  /scan   - Simulate an RFID card scan");
                        mockSerialLogs.push("  /clear  - Clear today's persistent error logs");
                        mockSerialLogs.push("  /stop   - Terminate current repeat mission");
                        mockSerialLogs.push("  /reboot - Restart the ESP32-S3 microcontroller");
                    } else if (trimmed === "/reboot") {
                        mockSerialLogs.push("Rebooting mock server system...");
                        setTimeout(() => {
                            mockSerialLogs = ["[00:00:00] TARSLIFT AGV - ESP32-S3 High-Level Controller", "[00:00:01] WiFi: AP started successfully. IP address: 192.168.4.1", "[00:00:02] DemoManager: Initializing mock telemetry mode...", "[00:00:02] System: Initialization completed successfully."];
                        }, 1000);
                    } else if (trimmed === "/scan") {
                        mockSerialLogs.push("Triggering RFID card scanner simulation...");
                        isRfidScanning = true;
                        setTimeout(() => {
                            const tags = ["04A7329B6C", "04B2187F21", "04C59133A2", "04D8421190"];
                            const sel = tags[Math.floor(Math.random() * tags.length)];
                            agvState.rfid = sel;
                            mockSerialLogs.push(`[00:00:00] RFID Tag detected: ${sel}`);
                        }, 1500);
                    } else if (trimmed === "/clear") {
                        mockSerialLogs.push("Persistent error logs cleared successfully.");
                    } else if (trimmed === "/stop") {
                        agvState.state = "STOPPED";
                        agvState.mode = "IDLE";
                        mockSerialLogs.push("Mission stop command broadcasted.");
                    } else {
                        mockSerialLogs.push("Error: Unknown command. Type /help for assistance.");
                    }
                } else {
                    mockSerialLogs.push("Error: Command must start with '/'. Type /help for assistance.");
                }
                
                if (mockSerialLogs.length > 50) {
                    mockSerialLogs.shift();
                }
                
                res.writeHead(200, { 'Content-Type': 'application/json' });
                res.end(JSON.stringify({ status: "success", message: "Command received" }));
                return;
            }
        });
        return;
    }

    // GET /api/settings
    if (pathname === '/api/settings' && req.method === 'GET') {
        res.writeHead(200, { 'Content-Type': 'application/json' });
        res.end(JSON.stringify(settingsState));
        return;
    }

    // Serve static files from /data/
    let filePath = path.join(__dirname, 'data', pathname === '/' ? 'index.html' : pathname);
    
    // Safety check to keep path within directory
    if (!filePath.startsWith(path.join(__dirname, 'data'))) {
        res.writeHead(403);
        res.end("Forbidden");
        return;
    }

    const extname = path.extname(filePath);
    let contentType = 'text/html';
    switch (extname) {
        case '.js': contentType = 'text/javascript'; break;
        case '.css': contentType = 'text/css'; break;
        case '.json': contentType = 'application/json'; break;
        case '.png': contentType = 'image/png'; break;
        case '.jpg': contentType = 'image/jpg'; break;
    }

    fs.readFile(filePath, (err, content) => {
        if (err) {
            if (err.code === 'ENOENT') {
                res.writeHead(404);
                res.end('File Not Found');
            } else {
                res.writeHead(500);
                res.end(`Server Error: ${err.code}`);
            }
        } else {
            res.writeHead(200, { 'Content-Type': contentType });
            res.end(content, 'utf-8');
        }
    });
});

// Create WebSocket Server on the same HTTP server
const wss = new WebSocketServer({ server });

wss.on('connection', (ws) => {
    console.log('WebSocket: Client connected to mock server');
    
    // Broadcast loop (5Hz)
    const broadcastInterval = setInterval(() => {
        if (ws.readyState === ws.OPEN) {
            const payload = Object.assign({}, agvState);
            payload.serial_logs = mockSerialLogs;
            if (settingsState.battery_monitoring) {
                payload.battery_enabled = true;
                payload.voltage = settingsState.voltage_monitoring ? (settingsState.demo_mode ? agvState.voltage.toFixed(1) + " V" : "11.6 V") : "N/A";
                payload.current = settingsState.current_monitoring ? (settingsState.demo_mode ? agvState.current.toFixed(2) + " A" : "0.82 A") : "N/A";
                
                if (settingsState.voltage_monitoring && settingsState.current_monitoring && settingsState.power_monitoring) {
                    const v = settingsState.demo_mode ? agvState.voltage : 11.6;
                    const c = settingsState.demo_mode ? agvState.current : 0.82;
                    payload.power = (v * c).toFixed(1) + " W";
                } else {
                    payload.power = "N/A";
                }
                
                payload.battery = settingsState.battery_percentage ? (settingsState.demo_mode ? Math.round(agvState.battery) + "%" : "78%") : "N/A";

                let batStatus = "NORMAL";
                const v = settingsState.demo_mode ? agvState.voltage : 11.6;
                const pct = settingsState.demo_mode ? agvState.battery : 78;
                if (settingsState.battery_fault_detection && (v < 5.0 || v > 15.0)) {
                    batStatus = "FAULT";
                } else if (settingsState.critical_battery_warning && (v <= settingsState.critical_voltage || pct <= settingsState.critical_battery_pct)) {
                    batStatus = "CRITICAL";
                } else if (settingsState.low_battery_warning && (v <= settingsState.low_voltage || pct <= settingsState.low_battery_pct)) {
                    batStatus = "LOW";
                }
                payload.battery_status = batStatus;
            } else {
                payload.battery_enabled = false;
                payload.voltage = "N/A";
                payload.current = "N/A";
                payload.power = "N/A";
                payload.battery = "N/A";
                payload.battery_status = "DISABLED";
            }

            ws.send(JSON.stringify(payload));
        }
    }, 200);

    ws.on('close', () => {
        clearInterval(broadcastInterval);
        console.log('WebSocket: Client disconnected from mock server');
    });
});

server.listen(PORT, () => {
    console.log(`================================================================`);
    console.log(`  TARSLIFT AGV MOCK DASHBOARD SERVER RUNNING`);
    console.log(`  URL: http://localhost:${PORT}`);
    console.log(`  WebSocket URL: ws://localhost:${PORT}/ws`);
    console.log(`================================================================`);
});

