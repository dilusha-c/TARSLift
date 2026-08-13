/* ============================================================================
   TARSLIFT AGV - DASHBOARD CONTROLLER JS
   ============================================================================ */

// System IP Configuration
const gatewayIp = window.location.host || "192.168.4.1";
const wsUrl = `ws://${gatewayIp}/ws`;
const apiUrl = `http://${gatewayIp}`;

// App State
let wsConn = null;
let currentActiveView = "dashboard";
let manualDriveSpeed = 50;
let lastDriveCommand = "STOP";
let lastCommandTime = 0;
const COMMAND_THROTTLE_MS = 150; // Max frequency for manual commands

// Scanning RFID State
let isScanningRFID = false;

// Premium Toast Notification System
function showToast(message, type = "success") {
    const container = document.getElementById("toast-container");
    if (!container) return;

    const toast = document.createElement("div");
    toast.className = `toast ${type}`;
    
    let titleText = "System Update";
    if (type === "error") titleText = "Action Failed";
    else if (type === "warning") titleText = "System Warning";
    else if (type === "info") titleText = "Diagnostics Info";

    toast.innerHTML = `
        <span class="toast-title">${titleText}</span>
        <span class="toast-msg">${message}</span>
    `;

    container.appendChild(toast);

    // Auto dismiss after 3.5 seconds
    setTimeout(() => {
        toast.style.opacity = "0";
        toast.style.transform = "translateX(100%) scale(0.9)";
        setTimeout(() => {
            toast.remove();
        }, 300);
    }, 3500);
}

// Override default blocking alerts
window.alert = (msg) => showToast(msg, "info");

// DOM Elements
const views = document.querySelectorAll(".view");
const navItems = document.querySelectorAll(".nav-item");
const mobileMenuToggle = document.getElementById("mobile-menu-toggle");
const sidebar = document.getElementById("sidebar");
const demoModeIndicator = document.getElementById("demo-mode-indicator");

// Startup init
document.addEventListener("DOMContentLoaded", () => {
    initViewRouter();
    initWebsocket();
    initManualControls();
    initRouteManager();
    initRFIDManager();
    initErrorLog();
    initSettingsManager();
    initSerialMonitor();
    
    // Mobile responsive toggle
    mobileMenuToggle.addEventListener("click", () => {
        sidebar.classList.toggle("open");
    });
});

/* ============================================================================
   VIEW ROUTER
   ============================================================================ */
function initViewRouter() {
    navItems.forEach(item => {
        item.addEventListener("click", () => {
            const targetView = item.getAttribute("data-view");
            switchView(targetView);
            sidebar.classList.remove("open"); // Close mobile sidebar if open
        });
    });
}

function switchView(viewId) {
    currentActiveView = viewId;
    
    // Toggle navigation buttons active state
    navItems.forEach(item => {
        if (item.getAttribute("data-view") === viewId) {
            item.classList.add("active");
        } else {
            item.classList.remove("active");
        }
    });

    // Toggle active view panel
    views.forEach(view => {
        if (view.id === `view-${viewId}`) {
            view.classList.add("active");
        } else {
            view.classList.remove("active");
        }
    });

    // Run view specific update hooks
    if (viewId === "routes") loadRoutesList();
    if (viewId === "rfid") loadRFIDList();
    if (viewId === "errors") loadErrorLogs();
    if (viewId === "repeat") populateRouteDropdown();
}

/* ============================================================================
   WEBSOCKETS TELEMETRY STREAM
   ============================================================================ */
function initWebsocket() {
    console.log(`Connecting to WebSocket stream at: ${wsUrl}`);
    wsConn = new WebSocket(wsUrl);

    wsConn.onopen = () => {
        console.log("WebSocket Connection Established.");
        document.getElementById("status-esp32").className = "status-dot green";
        document.getElementById("lbl-esp32").innerText = "ONLINE";
    };

    wsConn.onclose = () => {
        console.warn("WebSocket Connection Lost. Attempting reconnect...");
        document.getElementById("status-esp32").className = "status-dot red";
        document.getElementById("lbl-esp32").innerText = "OFFLINE";
        
        // Auto reconnect after 2.5 seconds
        setTimeout(initWebsocket, 2500);
    };

    wsConn.onerror = (err) => {
        console.error("WebSocket Error: ", err);
    };

    wsConn.onmessage = (event) => {
        try {
            const tele = JSON.parse(event.data);
            updateDashboardTelemetry(tele);
        } catch (e) {
            console.error("Failed to parse telemetry JSON", e);
        }
    };
}

function updateDashboardTelemetry(tele) {
    // 1. Top bar Status updates
    document.getElementById("lbl-mode").innerText = tele.mode;
    document.getElementById("lbl-mode").className = `mode-badge ${tele.mode.toLowerCase()}`;
    
    document.getElementById("lbl-battery").innerText = tele.battery;
    document.getElementById("lbl-voltage").innerText = tele.voltage;
    
    const stmConnected = (tele.health.stm32 === 2);
    document.getElementById("status-stm32").className = stmConnected ? "status-dot green" : "status-dot gray";
    document.getElementById("lbl-stm32").innerText = stmConnected ? "CONNECTED" : "DISCONNECTED";

    // 2. Demo Mode indicator
    if (tele.rssi === -45 || tele.health.esp32 === 1) {
        demoModeIndicator.style.display = "block";
    } else {
        demoModeIndicator.style.display = "none";
    }

    // 3. Current active error banner
    const errBanner = document.getElementById("error-banner");
    const errTitle = document.getElementById("error-title");
    const errDesc = document.getElementById("error-desc");
    
    if (tele.active_error && tele.active_error.active) {
        errBanner.className = "error-banner active-error";
        errTitle.innerText = `${tele.active_error.source} ERROR: ${tele.active_error.code}`;
        errDesc.innerText = `[${tele.active_error.timestamp}] ${tele.active_error.description}`;
    } else {
        errBanner.className = "error-banner ok";
        errTitle.innerText = "NO ACTIVE ERRORS";
        errDesc.innerText = "All systems running normally.";
    }

    // 4. View Specific Updates
    if (currentActiveView === "dashboard") {
        document.getElementById("dash-mode").innerText = tele.mode;
        document.getElementById("dash-state").innerText = tele.state;
        
        document.getElementById("dash-x").innerText = `${tele.x} m`;
        document.getElementById("dash-y").innerText = `${tele.y} m`;
        document.getElementById("dash-heading").innerText = `${tele.heading}°`;

        document.getElementById("dash-rfid-uid").innerText = tele.rfid === "NONE" ? "NO TAG" : tele.rfid;
        document.getElementById("dash-rfid-name").innerText = tele.rfid === "NONE" ? "No active checkpoint" : tele.rfid;

        document.getElementById("dash-bat-pct").innerText = tele.battery;
        document.getElementById("dash-bat-volt").innerText = tele.voltage;
        document.getElementById("dash-bat-curr").innerText = tele.current;
        document.getElementById("dash-bat-power").innerText = tele.power;
        
        const statusEl = document.getElementById("dash-bat-status");
        if (statusEl) {
            const statusMap = {
                NORMAL: "🟢 NORMAL",
                LOW: "🟡 LOW BATTERY",
                CRITICAL: "🔴 CRITICAL BATTERY",
                FAULT: "🔴 BATTERY SENSOR FAULT",
                DISABLED: "⚪ DISABLED"
            };
            statusEl.innerText = statusMap[tele.battery_status] || tele.battery_status;
            statusEl.className = "val";
            if (tele.battery_status === "NORMAL") statusEl.style.color = "var(--color-green)";
            else if (tele.battery_status === "LOW") statusEl.style.color = "var(--color-yellow)";
            else if (tele.battery_status === "CRITICAL" || tele.battery_status === "FAULT") statusEl.style.color = "var(--color-red)";
            else statusEl.style.color = "var(--text-muted)";
        }

        document.getElementById("dash-tof-l").innerText = `${tele.tof.left} mm`;
        document.getElementById("dash-tof-c").innerText = `${tele.tof.centre} mm`;
        document.getElementById("dash-tof-r").innerText = `${tele.tof.right} mm`;

        document.getElementById("dash-motor-l").innerText = `${tele.motor.left_rpm} RPM`;
        document.getElementById("dash-motor-r").innerText = `${tele.motor.right_rpm} RPM`;
        document.getElementById("dash-motor-target").innerText = `${tele.motor.target_rpm} RPM`;

        // Update health grid status indicators
        const healthContainer = document.getElementById("health-grid");
        healthContainer.innerHTML = "";
        
        const labels = {
            esp32: "ESP32-S3", stm32: "STM32F103", motor: "DC Motors", 
            encoder: "Wheel Encoders", mpu6050: "MPU6050 IMU", rfid: "RFID Reader", 
            tof: "ToF Array", battery: "INA219 Monitor", uart: "UART Bus"
        };

        for (const [key, stateCode] of Object.entries(tele.health)) {
            let dotClass = "gray";
            let stateName = "DISABLED";
            
            if (stateCode === 2) { dotClass = "green"; stateName = "REAL"; }
            else if (stateCode === 1) { dotClass = "blue"; stateName = "DEMO"; }
            else if (stateCode === 3) { dotClass = "red"; stateName = "ERROR"; }

            healthContainer.innerHTML += `
                <div class="health-item">
                    <span class="health-dot ${dotClass}"></span>
                    <span>${labels[key] || key}</span>
                    <span class="state-label ${dotClass}">${stateName}</span>
                </div>
            `;
        }
    } 
    else if (currentActiveView === "teach") {
        document.getElementById("teach-lbl-distance").innerText = `${tele.x.toFixed(2)} m`;
        document.getElementById("teach-lbl-speed").innerText = `${tele.speed.toFixed(2)} m/s`;
        document.getElementById("teach-lbl-heading").innerText = `${tele.heading.toFixed(1)}°`;
        document.getElementById("teach-lbl-checkpoint").innerText = tele.rfid === "NONE" ? "No Tag" : tele.rfid;
        
        // Disable/enable controls based on current mode
        const isRecording = (tele.mode === "TEACH" && tele.state === "RECORDING");
        document.getElementById("btn-teach-start").disabled = isRecording;
        document.getElementById("btn-teach-stop").disabled = !isRecording;
        document.getElementById("btn-teach-save").disabled = !isRecording;
        document.getElementById("btn-teach-cancel").disabled = !isRecording;
    } 
    else if (currentActiveView === "repeat") {
        document.getElementById("repeat-lbl-active-cp").innerText = tele.rfid;
        document.getElementById("repeat-lbl-progress").innerText = `${tele.mission_progress}%`;
        document.getElementById("repeat-lbl-speed").innerText = `${tele.speed.toFixed(2)} m/s`;
        document.getElementById("repeat-lbl-coords").innerText = `${tele.x.toFixed(2)}, ${tele.y.toFixed(2)} m`;
        document.getElementById("repeat-lbl-heading").innerText = `${tele.heading.toFixed(1)}°`;

        const isRunning = (tele.mode === "REPEAT" && tele.state === "RUNNING");
        const isPaused = (tele.mode === "REPEAT" && tele.state === "PAUSED");
        
        document.getElementById("btn-repeat-start").disabled = (tele.mode === "REPEAT");
        document.getElementById("btn-repeat-stop").disabled = (tele.mode !== "REPEAT");
        document.getElementById("btn-repeat-pause").disabled = !isRunning;
        document.getElementById("btn-repeat-pause").style.display = isRunning ? "inline-flex" : (isPaused ? "none" : "inline-flex");
        document.getElementById("btn-repeat-resume").style.display = isPaused ? "inline-flex" : "none";

        // Update active flowchart nodes highlights
        const nodes = document.querySelectorAll(".flowchart-node");
        if (nodes.length > 0) {
            nodes.forEach((node, idx) => {
                if (idx < tele.checkpoint_idx) {
                    node.className = "flowchart-node passed";
                } else if (idx === tele.checkpoint_idx) {
                    node.className = "flowchart-node active";
                } else {
                    node.className = "flowchart-node";
                }
            });
        }
    } 
    else if (currentActiveView === "system") {
        document.getElementById("sys-esp-ip").innerText = gatewayIp.split(":")[0];
        document.getElementById("sys-esp-rssi").innerText = `${tele.rssi} dBm`;
        document.getElementById("sys-esp-heap").innerText = `${tele.heap.toLocaleString()} bytes`;
        
        const totalKB = (tele.fs_total / 1024).toFixed(1);
        const usedKB = (tele.fs_used / 1024).toFixed(1);
        const freeKB = ((tele.fs_total - tele.fs_used) / 1024).toFixed(1);
        
        document.getElementById("sys-fs-total").innerText = `${totalKB} KB`;
        document.getElementById("sys-fs-used").innerText = `${usedKB} KB`;
        document.getElementById("sys-fs-free").innerText = `${freeKB} KB`;
        
        // Calculate uptime string HH:MM:SS
        const u = tele.uptime;
        const h = Math.floor(u / 3600).toString().padStart(2, '0');
        const m = Math.floor((u % 3600) / 60).toString().padStart(2, '0');
        const s = (u % 60).toString().padStart(2, '0');
        document.getElementById("sys-esp-uptime").innerText = `${h}:${m}:${s}`;

        const uartOk = (tele.health.uart === 2);
        document.getElementById("sys-stm-conn").innerText = stmConnected ? "CONNECTED" : "DISCONNECTED";
        document.getElementById("sys-stm-uart").innerText = uartOk ? "UART2 (Active)" : "DISABLED";

        // Update live serial monitor logs
        if (tele.serial_logs && Array.isArray(tele.serial_logs)) {
            const terminal = document.getElementById("serial-terminal");
            if (terminal) {
                const currentText = tele.serial_logs.join("\n");
                if (terminal.dataset.lastLogs !== currentText) {
                    terminal.textContent = currentText;
                    terminal.dataset.lastLogs = currentText;
                    terminal.scrollTop = terminal.scrollHeight;
                }
            }
        }
    }

    // Capture scanning tag value dynamically during scan operations
    if (isScanningRFID && tele.rfid !== "NONE" && tele.rfid !== "") {
        document.getElementById("tag-uid").value = tele.rfid;
        document.getElementById("tag-scan-hint").innerText = `Card Detected: ${tele.rfid}`;
        document.getElementById("tag-scan-hint").style.color = "var(--color-green)";
        isScanningRFID = false;
    }
}

/* ============================================================================
   MANUAL TELEOPERATION (TEACH MODE driving)
   ============================================================================ */
function initManualControls() {
    const slider = document.getElementById("speed-slider");
    const speedLabel = document.getElementById("lbl-speed-pct");

    slider.addEventListener("input", (e) => {
        manualDriveSpeed = e.target.value;
        speedLabel.innerText = `${manualDriveSpeed}%`;
    });

    // Touch DPAD event bindings
    const bindDpad = (btnId, dir) => {
        const btn = document.getElementById(btnId);
        const startAction = (e) => {
            e.preventDefault();
            btn.classList.add("active");
            sendMoveCommand(dir);
        };
        const endAction = (e) => {
            e.preventDefault();
            btn.classList.remove("active");
            sendMoveCommand("STOP");
        };
        btn.addEventListener("mousedown", startAction);
        btn.addEventListener("mouseup", endAction);
        btn.addEventListener("mouseleave", endAction);
        
        btn.addEventListener("touchstart", startAction);
        btn.addEventListener("touchend", endAction);
    };

    bindDpad("dpad-up", "FORWARD");
    bindDpad("dpad-down", "REVERSE");
    bindDpad("dpad-left", "LEFT");
    bindDpad("dpad-right", "RIGHT");
    
    // Stop Button
    const stopBtn = document.getElementById("dpad-stop");
    stopBtn.addEventListener("click", () => sendMoveCommand("STOP"));

    // Keyboard WASD event handlers
    const keyMap = {
        "KeyW": "FORWARD", "ArrowUp": "FORWARD",
        "KeyS": "REVERSE", "ArrowDown": "REVERSE",
        "KeyA": "LEFT", "ArrowLeft": "LEFT",
        "KeyD": "RIGHT", "ArrowRight": "RIGHT",
        "Space": "STOP"
    };

    let activeKeys = {};

    window.addEventListener("keydown", (e) => {
        // Prevent key controls if user is writing in a form text box
        if (e.target.tagName === "INPUT" || e.target.tagName === "TEXTAREA") return;
        
        // Space should stop in any mode
        if (e.code === "Space") {
            e.preventDefault();
            sendMoveCommand("STOP");
            return;
        }

        const cmd = keyMap[e.code];
        if (cmd && !activeKeys[e.code]) {
            e.preventDefault();
            activeKeys[e.code] = true;
            sendMoveCommand(cmd);
        }
    });

    window.addEventListener("keyup", (e) => {
        if (keyMap[e.code]) {
            e.preventDefault();
            delete activeKeys[e.code];
            
            // If no driving keys are pressed, trigger a STOP
            const keysLeft = Object.keys(activeKeys).some(k => keyMap[k] && keyMap[k] !== "STOP");
            if (!keysLeft) {
                sendMoveCommand("STOP");
            }
        }
    });
}

function sendMoveCommand(direction) {
    // Throttling commands to not flood the ESP32 Serial/Network buffers
    const now = Date.now();
    if (direction !== "STOP" && (now - lastCommandTime < COMMAND_THROTTLE_MS)) return;
    
    if (direction === lastDriveCommand && direction !== "STOP") return;
    
    lastDriveCommand = direction;
    lastCommandTime = now;

    console.log(`Sending manual move: ${direction} @ speed: ${manualDriveSpeed}%`);
    
    let path = direction === "STOP" ? "/api/manual/stop" : "/api/manual/move";
    let body = direction === "STOP" ? null : JSON.stringify({ direction: direction, speed: parseInt(manualDriveSpeed) });

    fetch(apiUrl + path, {
        method: "POST",
        headers: { "Content-Type": "application/json" },
        body: body
    })
    .catch(err => console.error("Drive command failed", err));
}

/* ============================================================================
   ROUTE MANAGER & TEACH MODE BUTTONS
   ============================================================================ */
function initRouteManager() {
    const btnOpenCreate = document.getElementById("btn-open-create-route");
    const modalCreate = document.getElementById("create-route-modal");
    const btnCloseModal = document.getElementById("btn-modal-close");
    const btnModalTeach = document.getElementById("btn-modal-start-teach");

    btnOpenCreate.addEventListener("click", () => {
        modalCreate.classList.add("active");
    });

    btnCloseModal.addEventListener("click", () => {
        modalCreate.classList.remove("active");
    });

    btnModalTeach.addEventListener("click", () => {
        const name = document.getElementById("modal-route-name").value;
        const start = document.getElementById("modal-route-start").value;
        const dest = document.getElementById("modal-route-destination").value;
        const desc = document.getElementById("modal-route-desc").value;

        if (name.length < 3) {
            alert("Route Name must be at least 3 characters.");
            return;
        }

        // POST /api/teach/start
        fetch(`${apiUrl}/api/teach/start`, {
            method: "POST",
            headers: { "Content-Type": "application/json" },
            body: JSON.stringify({ name: name, start: start, destination: dest, description: desc })
        })
        .then(res => res.json())
        .then(data => {
            if (data.status === "success") {
                modalCreate.classList.remove("active");
                
                // Copy settings to Teach mode view inputs
                document.getElementById("teach-route-name").value = name;
                document.getElementById("teach-route-start").value = start;
                document.getElementById("teach-route-destination").value = dest;
                
                // Direct the user to the Teach view tab
                switchView("teach");
            } else {
                alert("Failed: " + data.message);
            }
        })
        .catch(err => alert("Connection error: " + err));
    });

    // Teach Mode Screen Command Controls
    document.getElementById("btn-teach-start").addEventListener("click", () => {
        const name = document.getElementById("teach-route-name").value;
        const start = document.getElementById("teach-route-start").value;
        const dest = document.getElementById("teach-route-destination").value;

        fetch(`${apiUrl}/api/teach/start`, {
            method: "POST",
            headers: { "Content-Type": "application/json" },
            body: JSON.stringify({ name: name, start: start, destination: dest, description: "" })
        });
    });

    document.getElementById("btn-teach-stop").addEventListener("click", () => {
        fetch(`${apiUrl}/api/teach/stop`, { method: "POST" })
        .then(res => res.json())
        .then(data => {
            if (data.status === "success") {
                alert("Route saved successfully!");
                switchView("routes");
            } else {
                alert("Error: " + data.message);
            }
        });
    });

    document.getElementById("btn-teach-cancel").addEventListener("click", () => {
        if (confirm("Are you sure you want to cancel recording? Paths will be discarded.")) {
            // Trigger teleoperation cancel (or we just stop manual driving)
            fetch(`${apiUrl}/api/manual/stop`, { method: "POST" });
            switchView("routes");
        }
    });
}

function loadRoutesList() {
    const tableBody = document.querySelector("#routes-table tbody");
    tableBody.innerHTML = `<tr><td colspan="8" class="text-center placeholder-text">Loading saved routes...</td></tr>`;

    fetch(`${apiUrl}/api/routes`)
    .then(res => res.json())
    .then(routes => {
        tableBody.innerHTML = "";
        
        if (routes.length === 0) {
            tableBody.innerHTML = `<tr><td colspan="8" class="text-center placeholder-text">No routes created yet. Make one using teach mode!</td></tr>`;
            return;
        }

        routes.forEach(route => {
            const tr = document.createElement("tr");
            tr.innerHTML = `
                <td><b>${route.name}</b></td>
                <td><span class="state-label green">${route.start}</span></td>
                <td><span class="state-label blue">${route.destination}</span></td>
                <td>${route.checkpoints ? route.checkpoints.length : 2}</td>
                <td>${route.distance} m</td>
                <td>${route.duration}s</td>
                <td>${route.created}</td>
                <td class="table-actions">
                    <button class="btn btn-secondary btn-sm" onclick="runRouteFromManager('${route.id}')">RUN</button>
                    <button class="btn btn-outline btn-sm text-red" onclick="deleteRouteFromManager('${route.id}')">DELETE</button>
                </td>
            `;
            tableBody.appendChild(tr);
        });
    })
    .catch(err => {
        tableBody.innerHTML = `<tr><td colspan="8" class="text-center text-red">Failed to communicate with API.</td></tr>`;
    });
}

function runRouteFromManager(routeId) {
    switchView("repeat");
    setTimeout(() => {
        const selector = document.getElementById("repeat-route-selector");
        selector.value = routeId;
        // Trigger flowchart load
        loadFlowchartPreview(routeId);
    }, 200);
}

function deleteRouteFromManager(routeId) {
    if (confirm("Permanently delete route " + routeId + "?")) {
        fetch(`${apiUrl}/api/route/delete`, {
            method: "POST",
            headers: { "Content-Type": "application/json" },
            body: JSON.stringify({ id: routeId })
        })
        .then(() => loadRoutesList());
    }
}

// Global hook exposures for tables action buttons
window.runRouteFromManager = runRouteFromManager;
window.deleteRouteFromManager = deleteRouteFromManager;

/* ============================================================================
   REPEAT MODE PLAYER
   ============================================================================ */
function populateRouteDropdown() {
    const selector = document.getElementById("repeat-route-selector");
    selector.innerHTML = `<option value="">-- Choose Taught Route --</option>`;

    fetch(`${apiUrl}/api/routes`)
    .then(res => res.json())
    .then(routes => {
        routes.forEach(r => {
            const opt = document.createElement("option");
            opt.value = r.id;
            opt.innerText = r.name;
            selector.appendChild(opt);
        });
    });

    selector.removeEventListener("change", handleDropdownRouteSelection);
    selector.addEventListener("change", handleDropdownRouteSelection);
}

function handleDropdownRouteSelection(e) {
    const routeId = e.target.value;
    if (routeId) {
        loadFlowchartPreview(routeId);
    } else {
        document.getElementById("route-flowchart-container").innerHTML = `
            <p class="placeholder-text">Please select and start a route to display mission map progression.</p>
        `;
    }
}

function loadFlowchartPreview(routeId) {
    const container = document.getElementById("route-flowchart-container");
    container.innerHTML = `<p class="placeholder-text">Fetching path points...</p>`;

    fetch(`${apiUrl}/api/routes`)
    .then(res => res.json())
    .then(routes => {
        const route = routes.find(r => r.id === routeId);
        if (!route) return;

        container.innerHTML = "";
        const checkpoints = route.checkpoints || [route.start, route.destination];

        checkpoints.forEach((cp, idx) => {
            const node = document.createElement("div");
            node.className = "flowchart-node";
            node.innerHTML = `
                <span class="node-title">${cp}</span>
                <span class="node-desc">${idx === 0 ? "START" : (idx === checkpoints.length - 1 ? "GOAL" : "WAYPOINT")}</span>
                <span class="node-check">✓</span>
            `;
            container.appendChild(node);

            // Add arrows between nodes
            if (idx < checkpoints.length - 1) {
                const arrow = document.createElement("div");
                arrow.className = "flowchart-arrow";
                arrow.innerText = "➔";
                container.appendChild(arrow);
            }
        });
    });
}

// Bind Repeat controls
document.getElementById("btn-repeat-start").addEventListener("click", () => {
    const routeId = document.getElementById("repeat-route-selector").value;
    if (!routeId) {
        alert("Please select a route to repeat.");
        return;
    }

    fetch(`${apiUrl}/api/repeat/start`, {
        method: "POST",
        headers: { "Content-Type": "application/json" },
        body: JSON.stringify({ id: routeId })
    })
    .then(res => res.json())
    .then(data => {
        if (data.status === "error") {
            // Show hardware requirements safety block alert
            const safetyAlert = document.getElementById("repeat-safety-alert");
            const safetyMsg = document.getElementById("repeat-safety-msg");
            
            safetyAlert.style.display = "block";
            safetyMsg.innerText = data.message;
        } else {
            document.getElementById("repeat-safety-alert").style.display = "none";
        }
    });
});

document.getElementById("btn-repeat-pause").addEventListener("click", () => {
    fetch(`${apiUrl}/api/repeat/pause`, { method: "POST" });
});

document.getElementById("btn-repeat-resume").addEventListener("click", () => {
    fetch(`${apiUrl}/api/repeat/resume`, { method: "POST" });
});

document.getElementById("btn-repeat-stop").addEventListener("click", () => {
    fetch(`${apiUrl}/api/repeat/stop`, { method: "POST" });
});

/* ============================================================================
   RFID TAGS REGISTRY
   ============================================================================ */
function initRFIDManager() {
    const btnOpenAdd = document.getElementById("btn-open-add-tag");
    const modalAdd = document.getElementById("add-tag-modal");
    const btnClose = document.getElementById("btn-tag-close");
    const btnSave = document.getElementById("btn-tag-save");
    const btnScan = document.getElementById("btn-tag-scan");

    btnOpenAdd.addEventListener("click", () => {
        modalAdd.classList.add("active");
        document.getElementById("tag-uid").value = "";
        document.getElementById("tag-name").value = "";
        document.getElementById("tag-location").value = "";
        document.getElementById("tag-scan-hint").innerText = "Click Scan and approach the physical reader.";
        document.getElementById("tag-scan-hint").style.color = "var(--text-muted)";
    });

    btnClose.addEventListener("click", () => {
        modalAdd.classList.remove("active");
        isScanningRFID = false;
    });

    btnScan.addEventListener("click", () => {
        isScanningRFID = true;
        document.getElementById("tag-scan-hint").innerText = "Approaching card to RFID Antenna ... (SCANNING)";
        document.getElementById("tag-scan-hint").style.color = "var(--color-yellow)";
        
        fetch(`${apiUrl}/api/rfid/scan`, { method: "POST" });
    });

    btnSave.addEventListener("click", () => {
        const uid = document.getElementById("tag-uid").value;
        const name = document.getElementById("tag-name").value;
        const loc = document.getElementById("tag-location").value;

        if (!uid || !name) {
            alert("Card UID and Friendly Name are required.");
            return;
        }

        fetch(`${apiUrl}/api/rfid/add`, {
            method: "POST",
            headers: { "Content-Type": "application/json" },
            body: JSON.stringify({ uid: uid, name: name, location: loc })
        })
        .then(res => res.json())
        .then(data => {
            if (data.status === "success") {
                modalAdd.classList.remove("active");
                loadRFIDList();
            } else {
                alert("Error: " + data.message);
            }
        });
    });
}

function loadRFIDList() {
    const tableBody = document.querySelector("#rfid-table tbody");
    tableBody.innerHTML = `<tr><td colspan="5" class="text-center placeholder-text">Loading RFID mappings...</td></tr>`;

    fetch(`${apiUrl}/api/rfid`)
    .then(res => res.json())
    .then(data => {
        tableBody.innerHTML = "";
        const tags = data.tags || [];

        if (tags.length === 0) {
            tableBody.innerHTML = `<tr><td colspan="5" class="text-center placeholder-text">No registered RFID tags. Click add tag to register checkpoints!</td></tr>`;
            return;
        }

        tags.forEach(tag => {
            const tr = document.createElement("tr");
            tr.innerHTML = `
                <td><b class="monospace">${tag.uid}</b></td>
                <td><span class="state-label green">${tag.name}</span></td>
                <td>${tag.location || "N/A"}</td>
                <td><span class="state-label green">ACTIVE</span></td>
                <td class="table-actions">
                    <button class="btn btn-outline btn-sm text-red" onclick="deleteRFIDTag('${tag.uid}')">DELETE</button>
                </td>
            `;
            tableBody.appendChild(tr);
        });
    })
    .catch(err => {
        tableBody.innerHTML = `<tr><td colspan="5" class="text-center text-red">Failed to communicate with API.</td></tr>`;
    });
}

function deleteRFIDTag(uid) {
    if (confirm("Delete registered tag: " + uid + "?")) {
        fetch(`${apiUrl}/api/rfid/delete`, {
            method: "POST",
            headers: { "Content-Type": "application/json" },
            body: JSON.stringify({ uid: uid })
        })
        .then(() => loadRFIDList());
    }
}
window.deleteRFIDTag = deleteRFIDTag;

/* ============================================================================
   DAILY ERROR LOG
   ============================================================================ */
function initErrorLog() {
    document.getElementById("btn-clear-logs").addEventListener("click", () => {
        if (confirm("Are you sure you want to clear today's error log database?")) {
            fetch(`${apiUrl}/api/errors/clear`, { method: "POST" })
            .then(() => loadErrorLogs());
        }
    });

    document.getElementById("btn-view-prev-logs").addEventListener("click", () => {
        alert("LittleFS directory listing features for previous days: logs archived in system flash memory.");
    });
}

function loadErrorLogs() {
    const listContainer = document.getElementById("log-entries-list");
    listContainer.innerHTML = `<p class="placeholder-text text-center">Loading error journal entries...</p>`;

    fetch(`${apiUrl}/api/errors`)
    .then(res => res.json())
    .then(data => {
        document.getElementById("lbl-log-total").innerText = data.total;
        
        // 1. Rebuild log list entries
        listContainer.innerHTML = "";
        
        if (data.total === 0 || !data.log || data.log.trim() === "") {
            listContainer.innerHTML = `<p class="placeholder-text text-center">No errors registered for today.</p>`;
        } else {
            const lines = data.log.split("\n");
            lines.forEach(line => {
                if (line.trim() === "") return;
                const parts = line.split("\t");
                if (parts.length < 4) return;

                const row = document.createElement("div");
                row.className = "log-row";
                row.innerHTML = `
                    <span class="time">${parts[0]}</span>
                    <span class="source">${parts[1]}</span>
                    <span class="code">${parts[2]}</span>
                    <span class="desc">${parts[3]}</span>
                `;
                listContainer.appendChild(row);
            });
        }

        // 2. Build Category Statistics chart
        const chartGrid = document.getElementById("stats-chart-grid");
        chartGrid.innerHTML = "";
        
        const categories = data.categories || {};
        const maxVal = Math.max(...Object.values(categories), 1); // Avoid division by zero

        for (const [cat, count] of Object.entries(categories)) {
            const pct = (count / maxVal) * 80; // Scale height to max 80%

            const barContainer = document.createElement("div");
            barContainer.className = "chart-bar-container";
            barContainer.innerHTML = `
                <span>${count}</span>
                <div class="chart-bar-column" style="height: ${pct}px; background-color: ${count > 0 ? "var(--color-red)" : "var(--border-color)"}"></div>
                <label>${cat}</label>
            `;
            chartGrid.appendChild(barContainer);
        }
    })
    .catch(err => {
        listContainer.innerHTML = `<p class="placeholder-text text-center text-red">Failed to fetch error journal.</p>`;
    });
}

/* ============================================================================
   SETTINGS MANAGER
   ============================================================================ */
function loadSettingsFromServer() {
    fetch(`${apiUrl}/api/settings`)
    .then(res => res.json())
    .then(settings => {
        document.getElementById("set-test-profile").value = settings.test_profile;

        document.getElementById("set-feat-dashboard").checked = settings.enable_dashboard;
        document.getElementById("set-feat-teach").checked = settings.enable_teach_mode;
        document.getElementById("set-feat-repeat").checked = settings.enable_repeat_mode;
        document.getElementById("set-feat-manual").checked = settings.enable_manual_control;
        document.getElementById("set-feat-routes").checked = settings.enable_route_manager;
        document.getElementById("set-feat-rfid").checked = settings.enable_rfid_manager;
        document.getElementById("set-feat-errors").checked = settings.enable_error_log;
        document.getElementById("set-feat-system").checked = settings.enable_system_info;

        document.getElementById("set-comm-uart").checked = settings.enable_stm32_uart;
        document.getElementById("set-comm-ws").checked = settings.enable_websocket;

        document.getElementById("set-mot-control").checked = settings.enable_motor_control;
        document.getElementById("set-mot-encoder").checked = settings.enable_encoder;
        document.getElementById("set-mot-mpu").checked = settings.enable_mpu6050;
        document.getElementById("set-mot-pid").checked = settings.enable_pid;

        document.getElementById("set-rfid-reader").checked = settings.rfid_reader;
        document.getElementById("set-rfid-mgr").checked = settings.rfid_manager_flag;
        document.getElementById("set-rfid-cps").checked = settings.rfid_checkpoints;

        document.getElementById("set-tof-sensors").checked = settings.tof_sensors;
        document.getElementById("set-tof-obstacle").checked = settings.obstacle_detection;

        document.getElementById("set-bat-mon").checked = settings.battery_monitoring;
        document.getElementById("set-bat-ina").checked = settings.ina219;
        document.getElementById("set-bat-volt").checked = settings.voltage_monitoring;
        document.getElementById("set-bat-curr").checked = settings.current_monitoring;
        document.getElementById("set-bat-power").checked = settings.power_monitoring;
        document.getElementById("set-bat-pct").checked = settings.battery_percentage;
        document.getElementById("set-bat-low").checked = settings.low_battery_warning;
        document.getElementById("set-bat-crit").checked = settings.critical_battery_warning;
        document.getElementById("set-bat-fault").checked = settings.battery_fault_detection;
        document.getElementById("set-bat-charge").checked = settings.charging_status;

        document.getElementById("set-thresh-low-v").value = settings.low_voltage;
        document.getElementById("set-thresh-crit-v").value = settings.critical_voltage;
        document.getElementById("set-thresh-low-pct").value = settings.low_battery_pct;
        document.getElementById("set-thresh-crit-pct").value = settings.critical_battery_pct;

        document.getElementById("set-wifi-ssid").value = settings.wifi_ssid || "TARSLIFT_AGV";
        document.getElementById("set-wifi-pass").value = settings.wifi_password || "12345678";
        document.getElementById("set-sys-demo").checked = settings.demo_mode;

        applyUIDependencyRules();
        updateSidebarMenuVisibilities(settings);
    })
    .catch(err => console.error("Failed to load settings", err));
}

function applyUIDependencyRules() {
    const isProfileCustom = parseInt(document.getElementById("set-test-profile").value) === 0;

    const toggles = [
        "set-feat-dashboard", "set-feat-teach", "set-feat-repeat", "set-feat-manual",
        "set-feat-routes", "set-feat-rfid", "set-feat-errors", "set-feat-system",
        "set-comm-uart", "set-comm-ws",
        "set-mot-control", "set-mot-encoder", "set-mot-mpu", "set-mot-pid",
        "set-rfid-reader", "set-rfid-mgr", "set-rfid-cps",
        "set-tof-sensors", "set-tof-obstacle",
        "set-bat-mon", "set-bat-ina", "set-bat-volt", "set-bat-curr", "set-bat-power",
        "set-bat-pct", "set-bat-low", "set-bat-crit", "set-bat-fault", "set-bat-charge",
        "set-sys-demo"
    ];

    toggles.forEach(id => {
        const el = document.getElementById(id);
        if (el) {
            el.disabled = !isProfileCustom;
        }
    });

    // Subsystem dependency cascading
    const batMon = document.getElementById("set-bat-mon").checked;
    const subBat = ["set-bat-ina", "set-bat-volt", "set-bat-curr", "set-bat-power", "set-bat-pct", "set-bat-low", "set-bat-crit", "set-bat-fault"];
    subBat.forEach(id => {
        const el = document.getElementById(id);
        if (el) {
            if (!batMon) {
                el.checked = false;
                el.disabled = true;
            } else if (isProfileCustom) {
                el.disabled = false;
            }
        }
    });

    const rfidMon = document.getElementById("set-rfid-reader").checked;
    const subRfid = ["set-rfid-mgr", "set-rfid-cps"];
    subRfid.forEach(id => {
        const el = document.getElementById(id);
        if (el) {
            if (!rfidMon) {
                el.checked = false;
                el.disabled = true;
            } else if (isProfileCustom) {
                el.disabled = false;
            }
        }
    });

    const tofMon = document.getElementById("set-tof-sensors").checked;
    const subTof = ["set-tof-obstacle"];
    subTof.forEach(id => {
        const el = document.getElementById(id);
        if (el) {
            if (!tofMon) {
                el.checked = false;
                el.disabled = true;
            } else if (isProfileCustom) {
                el.disabled = false;
            }
        }
    });
}

function updateSidebarMenuVisibilities(settings) {
    const map = {
        dashboard: settings.enable_dashboard,
        teach: settings.enable_teach_mode,
        repeat: settings.enable_repeat_mode,
        routes: settings.enable_route_manager,
        rfid: settings.enable_rfid_manager,
        errors: settings.enable_error_log,
        system: settings.enable_system_info
    };

    navItems.forEach(item => {
        const view = item.getAttribute("data-view");
        if (map[view] !== undefined) {
            item.style.display = map[view] ? "flex" : "none";
        }
    });
}

function initSettingsManager() {
    loadSettingsFromServer();

    const origSwitch = window.switchView;
    window.switchView = function(viewId) {
        if (viewId === "settings") {
            loadSettingsFromServer();
        }
        origSwitch(viewId);
    };

    // Form change event listeners
    document.getElementById("set-test-profile").addEventListener("change", (e) => {
        const profileVal = parseInt(e.target.value);
        if (profileVal > 0) {
            fetch(`${apiUrl}/api/settings`, {
                method: "POST",
                headers: { "Content-Type": "application/json" },
                body: JSON.stringify({ test_profile: profileVal })
            })
            .then(res => res.json())
            .then(data => {
                if (data.status === "success") {
                    loadSettingsFromServer();
                }
            });
        } else {
            applyUIDependencyRules();
        }
    });

    document.getElementById("set-bat-mon").addEventListener("change", applyUIDependencyRules);
    document.getElementById("set-rfid-reader").addEventListener("change", applyUIDependencyRules);
    document.getElementById("set-tof-sensors").addEventListener("change", applyUIDependencyRules);

    // Save main settings
    document.getElementById("btn-settings-save").addEventListener("click", () => {
        const profileVal = parseInt(document.getElementById("set-test-profile").value);
        
        const body = {
            test_profile: profileVal,
            wifi_ssid: document.getElementById("set-wifi-ssid").value,
            wifi_password: document.getElementById("set-wifi-pass").value,
            demo_mode: document.getElementById("set-sys-demo").checked
        };

        if (profileVal === 0) { // Custom
            body.enable_dashboard = document.getElementById("set-feat-dashboard").checked;
            body.enable_teach_mode = document.getElementById("set-feat-teach").checked;
            body.enable_repeat_mode = document.getElementById("set-feat-repeat").checked;
            body.enable_manual_control = document.getElementById("set-feat-manual").checked;
            body.enable_route_manager = document.getElementById("set-feat-routes").checked;
            body.enable_rfid_manager = document.getElementById("set-feat-rfid").checked;
            body.enable_error_log = document.getElementById("set-feat-errors").checked;
            body.enable_system_info = document.getElementById("set-feat-system").checked;

            body.enable_stm32_uart = document.getElementById("set-comm-uart").checked;
            body.enable_websocket = document.getElementById("set-comm-ws").checked;

            body.enable_motor_control = document.getElementById("set-mot-control").checked;
            body.enable_encoder = document.getElementById("set-mot-encoder").checked;
            body.enable_mpu6050 = document.getElementById("set-mot-mpu").checked;
            body.enable_pid = document.getElementById("set-mot-pid").checked;

            body.rfid_reader = document.getElementById("set-rfid-reader").checked;
            body.rfid_manager = document.getElementById("set-rfid-mgr").checked;
            body.rfid_checkpoints = document.getElementById("set-rfid-cps").checked;

            body.tof_sensors = document.getElementById("set-tof-sensors").checked;
            body.obstacle_detection = document.getElementById("set-tof-obstacle").checked;

            body.battery_monitoring = document.getElementById("set-bat-mon").checked;
            body.ina219 = document.getElementById("set-bat-ina").checked;
            body.voltage_monitoring = document.getElementById("set-bat-volt").checked;
            body.current_monitoring = document.getElementById("set-bat-curr").checked;
            body.power_monitoring = document.getElementById("set-bat-power").checked;
            body.battery_percentage = document.getElementById("set-bat-pct").checked;
            body.low_battery_warning = document.getElementById("set-bat-low").checked;
            body.critical_battery_warning = document.getElementById("set-bat-crit").checked;
            body.battery_fault_detection = document.getElementById("set-bat-fault").checked;
            body.charging_status = document.getElementById("set-bat-charge").checked;
        }

        fetch(`${apiUrl}/api/settings`, {
            method: "POST",
            headers: { "Content-Type": "application/json" },
            body: JSON.stringify(body)
        })
        .then(res => res.json())
        .then(data => {
            if (data.status === "success") {
                showToast("System configuration successfully saved! Subsystems and menu flags updated.", "success");
                loadSettingsFromServer();
            } else {
                showToast("Save failed: " + data.message, "error");
            }
        })
        .catch(err => showToast("Communication error: " + err, "error"));
    });

    // Save battery thresholds
    document.getElementById("btn-settings-save-battery").addEventListener("click", () => {
        const body = {
            low_voltage: parseFloat(document.getElementById("set-thresh-low-v").value),
            critical_voltage: parseFloat(document.getElementById("set-thresh-crit-v").value),
            low_battery_pct: parseInt(document.getElementById("set-thresh-low-pct").value),
            critical_battery_pct: parseInt(document.getElementById("set-thresh-crit-pct").value)
        };

        fetch(`${apiUrl}/api/settings`, {
            method: "POST",
            headers: { "Content-Type": "application/json" },
            body: JSON.stringify(body)
        })
        .then(res => res.json())
        .then(data => {
            if (data.status === "success") {
                showToast("Battery threshold settings saved successfully!", "success");
                loadSettingsFromServer();
            } else {
                showToast("Save failed: " + data.message, "error");
            }
        })
        .catch(err => showToast("Communication error: " + err, "error"));
    });

    document.getElementById("btn-settings-reset").addEventListener("click", () => {
        if (confirm("Reset to default test profile configuration?")) {
            fetch(`${apiUrl}/api/settings`, {
                method: "POST",
                headers: { "Content-Type": "application/json" },
                body: JSON.stringify({ test_profile: 1 }) // MOTOR TEST
            })
            .then(() => loadSettingsFromServer());
        }
    });
}

/* ============================================================================
   SERIAL MONITOR
   ============================================================================ */
function initSerialMonitor() {
    const serialForm = document.getElementById("serial-command-form");
    const serialInput = document.getElementById("serial-command-input");
    const serialClearBtn = document.getElementById("btn-serial-clear");

    if (serialForm && serialInput) {
        serialForm.addEventListener("submit", (e) => {
            e.preventDefault();
            const cmd = serialInput.value.trim();
            if (!cmd) return;

            fetch(`${apiUrl}/api/serial/command`, {
                method: "POST",
                headers: { "Content-Type": "application/json" },
                body: JSON.stringify({ command: cmd })
            })
            .then(r => r.json())
            .then(res => {
                if (res.status === "success") {
                    serialInput.value = "";
                } else {
                    showToast(res.message || "Error sending command", "error");
                }
            })
            .catch(err => {
                showToast("Failed to transmit serial command", "error");
            });
        });
    }

    if (serialClearBtn) {
        serialClearBtn.addEventListener("click", () => {
            const terminal = document.getElementById("serial-terminal");
            if (terminal) {
                terminal.textContent = "";
                terminal.dataset.lastLogs = "";
            }
        });
    }
}

