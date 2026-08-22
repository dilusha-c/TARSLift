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
    initMapRenderer();
    initTouchJoystick();
    initPerfCharts();
    initPIDTuneManager();
    
    // Mobile responsive toggle
    if (mobileMenuToggle) {
        mobileMenuToggle.addEventListener("click", (e) => {
            e.stopPropagation();
            sidebar.classList.toggle("open");
        });
    }

    // Tap outside to close sidebar drawer on phones
    document.addEventListener("click", (e) => {
        if (sidebar && sidebar.classList.contains("open")) {
            if (!sidebar.contains(e.target) && e.target !== mobileMenuToggle && !mobileMenuToggle.contains(e.target)) {
                sidebar.classList.remove("open");
            }
        }
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
            item.blur(); // Release button focus so keys aren't intercepted
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
    
    const stmState = tele.health.stm32;
    const stmDot = document.getElementById("status-stm32");
    const stmLbl = document.getElementById("lbl-stm32");
    if (stmDot && stmLbl) {
        if (stmState === 2) {
            stmDot.className = "status-dot green";
            stmLbl.innerText = "CONNECTED";
        } else if (stmState === 1) {
            stmDot.className = "status-dot blue";
            stmLbl.innerText = "DEMO MODE";
        } else if (stmState === 3) {
            stmDot.className = "status-dot red";
            stmLbl.innerText = "ERROR";
        } else {
            stmDot.className = "status-dot gray";
            stmLbl.innerText = "DISCONNECTED";
        }
    }

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

    // Top status bar Dual IP update
    const lblApIp = document.getElementById("lbl-ap-ip");
    const lblStaIp = document.getElementById("lbl-sta-ip");
    const statusStaDot = document.getElementById("status-sta-dot");

    if (lblApIp) lblApIp.innerText = tele.ap_ip || "192.168.4.1";
    if (lblStaIp) {
        lblStaIp.innerText = tele.sta_ip || "Disconnected";
        if (statusStaDot) {
            statusStaDot.className = tele.sta_connected ? "status-dot green" : "status-dot gray";
        }
    }

    // 4. View Specific Updates
    if (currentActiveView === "dashboard") {
        document.getElementById("dash-mode").innerText = tele.mode;
        document.getElementById("dash-state").innerText = tele.state;
        
        document.getElementById("dash-x").innerText = `${tele.x} m`;
        document.getElementById("dash-y").innerText = `${tele.y} m`;
        document.getElementById("dash-heading").innerText = `${tele.heading}°`;

        // Update 2D Live Canvas Map
        updateMapTelemetry(tele);

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
    else if (currentActiveView === "pid_tune") {
        document.getElementById("live-enc-l-rpm").innerText = tele.enc_l_rpm || 0;
        document.getElementById("live-enc-r-rpm").innerText = tele.enc_r_rpm || 0;
        
        if (tele.enc_l_mms !== undefined) {
            document.getElementById("live-enc-l-mms").innerText = parseFloat(tele.enc_l_mms).toFixed(1);
            document.getElementById("live-enc-r-mms").innerText = parseFloat(tele.enc_r_mms).toFixed(1);
        }
        
        if (typeof updatePidCharts === "function") {
            let target = 0;
            if (tele.motor && tele.motor.target_rpm) target = tele.motor.target_rpm;
            updatePidCharts(target, tele.enc_l_rpm || 0, tele.enc_r_rpm || 0);
        }

        // ToF Updates
        if (tele.tof) {
            document.getElementById("sys-tof-l").innerText = `${tele.tof.left} mm`;
            document.getElementById("sys-tof-c").innerText = `${tele.tof.centre} mm`;
            document.getElementById("sys-tof-r").innerText = `${tele.tof.right} mm`;
        }

        // IMU Updates
        if (tele.heading !== undefined) {
            document.getElementById("sys-imu-heading").innerText = `${tele.heading.toFixed(1)}°`;
            const needle = document.getElementById("compass-needle");
            if (needle) needle.style.transform = `rotate(${tele.heading}deg)`;
        }

        // RFID Updates
        if (tele.rfid !== undefined) {
            document.getElementById("sys-rfid-uid").innerText = tele.rfid === "NONE" ? "NO TAG" : tele.rfid;
        }

        // Raw Encoder Updates for Calibration Tool
        if (tele.raw_enc_l !== undefined && tele.raw_enc_r !== undefined) {
            // Keep a global or attach to the DOM so the calibrator can access it
            window.latestRawEncL = tele.raw_enc_l;
            window.latestRawEncR = tele.raw_enc_r;
            
            const sideSelect = document.getElementById("calib-ppr-side");
            if (sideSelect) {
                const isLeft = sideSelect.value === "left";
                const liveEl = document.getElementById("sys-raw-enc-live");
                if (liveEl) {
                    liveEl.innerText = isLeft ? tele.raw_enc_l : tele.raw_enc_r;
                }
            }
        }
    }
    else if (currentActiveView === "system") {
        const sysEspApIp = document.getElementById("sys-esp-ap-ip");
        const sysEspStaIp = document.getElementById("sys-esp-sta-ip");
        if (sysEspApIp) sysEspApIp.innerText = tele.ap_ip || "192.168.4.1";
        if (sysEspStaIp) sysEspStaIp.innerText = tele.sta_ip || "Disconnected";
        document.getElementById("sys-esp-rssi").innerText = `${tele.rssi} dBm`;
        document.getElementById("sys-esp-heap").innerText = `${tele.heap.toLocaleString()} bytes`;
        
        const totalKB = (tele.fs_total / 1024).toFixed(1);
        const usedKB = (tele.fs_used / 1024).toFixed(1);
        const freeKB = ((tele.fs_total - tele.fs_used) / 1024).toFixed(1);
        
        document.getElementById("sys-fs-total").innerText = `${totalKB} KB`;
        document.getElementById("sys-fs-used").innerText = `${usedKB} KB`;
        document.getElementById("sys-fs-free").innerText = `${freeKB} KB`;
        
        // Update Live Hardware Performance Benchmarks
        const elCpu = document.getElementById("sys-perf-cpu");
        const elLoopMs = document.getElementById("sys-perf-loop-ms");
        const elLoopHz = document.getElementById("sys-perf-loop-hz");
        const elPerfHeap = document.getElementById("sys-perf-heap");
        const elMinHeap = document.getElementById("sys-perf-min-heap");
        const elMaxAlloc = document.getElementById("sys-perf-max-alloc");
        const elPerfFs = document.getElementById("sys-perf-fs");

        if (elCpu && tele.cpu_mhz) elCpu.innerText = `${tele.cpu_mhz} MHz`;
        if (elLoopMs && tele.loop_ms !== undefined) elLoopMs.innerText = `${tele.loop_ms.toFixed(2)} ms`;
        if (elLoopHz && tele.loop_hz !== undefined) elLoopHz.innerText = `${tele.loop_hz.toLocaleString()} Hz`;
        if (elPerfHeap && tele.heap) elPerfHeap.innerText = `${tele.heap.toLocaleString()} bytes`;
        if (elMinHeap && tele.min_heap) elMinHeap.innerText = `${tele.min_heap.toLocaleString()} bytes`;
        if (elMaxAlloc && tele.max_alloc) elMaxAlloc.innerText = `${tele.max_alloc.toLocaleString()} bytes`;
        if (elPerfFs) elPerfFs.innerText = `${usedKB} KB / ${totalKB} KB`;

        // Internal ESP32 Die Temperature Meter
        if (tele.esp_temp !== undefined) {
            const tempC = tele.esp_temp;
            const tempF = (tempC * 9 / 5 + 32).toFixed(1);
            const elTemp = document.getElementById("sys-perf-temp");
            const elBadge = document.getElementById("sys-temp-badge");

            if (elTemp) elTemp.innerText = `${tempC.toFixed(1)} °C / ${tempF} °F`;
            if (elBadge) {
                if (tempC > 70) {
                    elBadge.innerText = "OVERHEAT";
                    elBadge.className = "temp-badge hot";
                } else if (tempC > 50) {
                    elBadge.innerText = "WARM";
                    elBadge.className = "temp-badge warm";
                } else {
                    elBadge.innerText = "NORMAL";
                    elBadge.className = "temp-badge normal";
                }
            }
        }

        // Live CPU & RAM Scrolling Charts
        const cpuPct = tele.cpu_usage !== undefined ? tele.cpu_usage : 8.5;
        const ramPct = tele.ram_usage !== undefined ? tele.ram_usage : (100 - (tele.heap / 327680 * 100));

        const elCpuVal = document.getElementById("lbl-chart-cpu-val");
        const elRamVal = document.getElementById("lbl-chart-ram-val");

        if (elCpuVal) elCpuVal.innerText = `${cpuPct.toFixed(1)}%`;
        if (elRamVal) elRamVal.innerText = `${ramPct.toFixed(1)}%`;

        if (cpuPerfChart) cpuPerfChart.pushValue(cpuPct);
        if (ramPerfChart) ramPerfChart.pushValue(ramPct);

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
            btn.blur();
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
    stopBtn.addEventListener("click", () => {
        sendMoveCommand("STOP");
        stopBtn.blur();
    });

    // Keyboard WASD event handlers
    const keyMap = {
        "KeyW": "FORWARD", "ArrowUp": "FORWARD",
        "KeyS": "REVERSE", "ArrowDown": "REVERSE",
        "KeyA": "LEFT", "ArrowLeft": "LEFT",
        "KeyD": "RIGHT", "ArrowRight": "RIGHT",
        "Space": "STOP"
    };

    const keyToBtnId = {
        "KeyW": "dpad-up", "ArrowUp": "dpad-up",
        "KeyS": "dpad-down", "ArrowDown": "dpad-down",
        "KeyA": "dpad-left", "ArrowLeft": "dpad-left",
        "KeyD": "dpad-right", "ArrowRight": "dpad-right"
    };

    let activeKeys = {};

    window.addEventListener("keydown", (e) => {
        // Prevent key controls if user is writing in a form text box or serial input
        if (e.target.tagName === "INPUT" || e.target.tagName === "TEXTAREA" || e.target.tagName === "SELECT") return;
        
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
            
            // Light up corresponding D-pad button in UI
            const btnId = keyToBtnId[e.code];
            if (btnId) {
                const btn = document.getElementById(btnId);
                if (btn) btn.classList.add("active");
            }
            
            sendMoveCommand(cmd);
        }
    });

    window.addEventListener("keyup", (e) => {
        if (keyMap[e.code]) {
            e.preventDefault();
            delete activeKeys[e.code];
            
            // Turn off corresponding D-pad button lights in UI
            const btnId = keyToBtnId[e.code];
            if (btnId) {
                const btn = document.getElementById(btnId);
                if (btn) btn.classList.remove("active");
            }
            
            // If no driving keys are pressed, trigger a STOP
            const keysLeft = Object.keys(activeKeys).some(k => keyMap[k] && keyMap[k] !== "STOP");
            if (!keysLeft) {
                sendMoveCommand("STOP");
            }
        }
    });

    // Safety: blur listener to stop vehicle immediately if window loses focus
    window.addEventListener("blur", () => {
        activeKeys = {};
        sendMoveCommand("STOP");
        Object.values(keyToBtnId).forEach(id => {
            const btn = document.getElementById(id);
            if (btn) btn.classList.remove("active");
        });
    });

    // Auto-focus window on hover interaction
    document.addEventListener("mouseover", () => {
        if (document.activeElement === document.body || document.activeElement === null) {
            window.focus();
        }
    }, { once: true });
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

function sendManualDriveCommand(direction, speedPct) {
    const now = Date.now();
    
    // We do NOT throttle STOP commands to ensure the robot always stops!
    if (direction !== "STOP" && (now - lastCommandTime < COMMAND_THROTTLE_MS)) return;
    
    // Unlike D-Pad, joystick constantly sends the same direction while dragged.
    // We don't filter out repeated directions so the analog speed can smoothly update.
    lastDriveCommand = direction;
    lastCommandTime = now;

    console.log(`Sending analog joystick move: ${direction} @ speed: ${speedPct}%`);
    
    let path = direction === "STOP" ? "/api/manual/stop" : "/api/manual/move";
    let body = direction === "STOP" ? null : JSON.stringify({ direction: direction, speed: parseInt(speedPct) });

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
    const navMode = document.getElementById("repeat-nav-mode-selector").value;
    let payload = {};

    if (navMode === "0") {
        const routeId = document.getElementById("repeat-route-selector").value;
        if (!routeId) {
            alert("Please select a route to repeat.");
            return;
        }
        payload = { id: routeId, mode: 0 };
    } else {
        const startRfid = document.getElementById("repeat-start-rfid").value.trim();
        const destRfid = document.getElementById("repeat-dest-rfid").value.trim();
        if (!startRfid || !destRfid) {
            alert("Please enter both Start and Destination RFIDs.");
            return;
        }
        payload = { start_rfid: startRfid, dest_rfid: destRfid, mode: 1 };
    }

    fetch(`${apiUrl}/api/repeat/start`, {
        method: "POST",
        headers: { "Content-Type": "application/json" },
        body: JSON.stringify(payload)
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
            // Map will be updated via telemetry
        }
    });
});

// Bind Nav Mode Selector
document.getElementById("repeat-nav-mode-selector").addEventListener("change", (e) => {
    if (e.target.value === "0") {
        document.getElementById("repeat-direct-mode-inputs").style.display = "inline-block";
        document.getElementById("repeat-shortest-mode-inputs").style.display = "none";
    } else {
        document.getElementById("repeat-direct-mode-inputs").style.display = "none";
        document.getElementById("repeat-shortest-mode-inputs").style.display = "inline-block";
        document.getElementById("route-flowchart-container").innerHTML = `<p class="placeholder-text">Shortest Path map will generate upon Start Mission.</p>`;
    }
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

        document.getElementById("set-trim-l-fwd").value = settings.motor_l_fwd_scale;
        document.getElementById("set-trim-r-fwd").value = settings.motor_r_fwd_scale;
        document.getElementById("set-trim-l-turn").value = settings.motor_l_turn_scale;
        document.getElementById("set-trim-r-turn").value = settings.motor_r_turn_scale;

        document.getElementById("lbl-trim-l-fwd").innerText = settings.motor_l_fwd_scale;
        document.getElementById("lbl-trim-r-fwd").innerText = settings.motor_r_fwd_scale;
        document.getElementById("lbl-trim-l-turn").innerText = settings.motor_l_turn_scale;
        document.getElementById("lbl-trim-r-turn").innerText = settings.motor_r_turn_scale;

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

    // Motor Trim slider live labels
    const bindSliderLabel = (sliderId, labelId) => {
        document.getElementById(sliderId).addEventListener("input", (e) => {
            document.getElementById(labelId).innerText = e.target.value;
        });
    };
    bindSliderLabel("set-trim-l-fwd", "lbl-trim-l-fwd");
    bindSliderLabel("set-trim-r-fwd", "lbl-trim-r-fwd");
    bindSliderLabel("set-trim-l-turn", "lbl-trim-l-turn");
    bindSliderLabel("set-trim-r-turn", "lbl-trim-r-turn");

    // Save main settings
    document.getElementById("btn-settings-save").addEventListener("click", () => {
        const profileVal = parseInt(document.getElementById("set-test-profile").value);
        
        const body = {
            test_profile: profileVal,
            wifi_ssid: document.getElementById("set-wifi-ssid").value,
            wifi_password: document.getElementById("set-wifi-pass").value,
            demo_mode: document.getElementById("set-sys-demo").checked,
            motor_l_fwd_scale: parseInt(document.getElementById("set-trim-l-fwd").value),
            motor_r_fwd_scale: parseInt(document.getElementById("set-trim-r-fwd").value),
            motor_l_turn_scale: parseInt(document.getElementById("set-trim-l-turn").value),
            motor_r_turn_scale: parseInt(document.getElementById("set-trim-r-turn").value)
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

    // Settings Sub-Tabs Navigation Toggle
    const settingTabs = document.querySelectorAll(".settings-tab-btn");
    const settingPanels = document.querySelectorAll(".settings-panel");
    settingTabs.forEach(tab => {
        tab.addEventListener("click", () => {
            settingTabs.forEach(t => t.classList.remove("active"));
            tab.classList.add("active");

            const targetTab = tab.getAttribute("data-settings-tab");
            settingPanels.forEach(panel => {
                if (panel.id === `settings-panel-${targetTab}`) {
                    panel.classList.add("active");
                } else {
                    panel.classList.remove("active");
                }
            });
        });
    });

    // Wipe All System Data (Password: 1234)
    const wipeBtn = document.getElementById("btn-system-wipe");
    if (wipeBtn) {
        wipeBtn.addEventListener("click", () => {
            const pw = prompt("⚠️ CRITICAL ACTION ⚠️\nThis erases ALL routes, logs, tags, and settings. Enter administrative password to execute:");
            if (pw === null) return;
            
            if (pw === "1234") {
                showToast("Password verified. Clearing system flash memory...", "info");
                
                fetch(`${apiUrl}/api/system/wipe`, {
                    method: "POST",
                    headers: { "Content-Type": "application/json" },
                    body: JSON.stringify({ password: pw })
                })
                .then(res => res.json())
                .then(data => {
                    if (data.status === "success") {
                        showToast("Flash data erased! Coprocessor rebooting, please wait...", "warning");
                        if (wsConn) wsConn.close();
                        setTimeout(() => {
                            window.location.reload();
                        }, 5000);
                    } else {
                        showToast("Erasing failed: " + data.message, "error");
                    }
                })
                .catch(err => showToast("Communication error during erasure: " + err, "error"));
            } else {
                showToast("Access Denied: Invalid password", "error");
            }
        });
    }

    // Wi-Fi Scanner Handler
    const btnScanWifi = document.getElementById("btn-wifi-scan");
    const scanSelect = document.getElementById("wifi-scan-select");
    const btnSaveLan = document.getElementById("btn-lan-save");

    if (btnScanWifi && scanSelect) {
        btnScanWifi.addEventListener("click", () => {
            btnScanWifi.disabled = true;
            btnScanWifi.innerText = "⏳ SCANNING...";
            scanSelect.innerHTML = `<option value="">Scanning Wi-Fi networks...</option>`;

            fetch(`${apiUrl}/api/wifi/scan`)
                .then(r => r.json())
                .then(networks => {
                    btnScanWifi.disabled = false;
                    btnScanWifi.innerText = "🔍 SCAN";
                    scanSelect.innerHTML = `<option value="">-- Select Scanned Network --</option>`;

                    if (Array.isArray(networks) && networks.length > 0) {
                        networks.forEach(net => {
                            const opt = document.createElement("option");
                            opt.value = net.ssid;
                            const secIcon = net.secure ? "🔒" : "🔓";
                            opt.innerText = `${secIcon} ${net.ssid} (${net.rssi} dBm)`;
                            scanSelect.appendChild(opt);
                        });
                        showToast(`Found ${networks.length} Wi-Fi networks!`, "success");
                    } else {
                        showToast("No Wi-Fi networks found.", "warning");
                    }
                })
                .catch(err => {
                    btnScanWifi.disabled = false;
                    btnScanWifi.innerText = "🔍 SCAN";
                    showToast("Wi-Fi scan failed: " + err, "error");
                });
        });

        scanSelect.addEventListener("change", () => {
            if (scanSelect.value) {
                const lanSsidInput = document.getElementById("set-lan-ssid");
                if (lanSsidInput) lanSsidInput.value = scanSelect.value;
            }
        });
    }

    if (btnSaveLan) {
        btnSaveLan.addEventListener("click", () => {
            const ssid = document.getElementById("set-lan-ssid").value.trim();
            const pass = document.getElementById("set-lan-pass").value.trim();

            if (!ssid) {
                showToast("SSID cannot be empty", "warning");
                return;
            }

            showToast(`Connecting to LAN Wi-Fi '${ssid}'...`, "info");
            fetch(`${apiUrl}/api/wifi/connect`, {
                method: "POST",
                headers: { "Content-Type": "application/json" },
                body: JSON.stringify({ ssid: ssid, password: pass })
            })
            .then(r => r.json())
            .then(res => {
                if (res.status === "success") {
                    showToast("Credentials saved! ESP32 background task connecting...", "success");
                } else {
                    showToast("Connection failed: " + res.message, "error");
                }
            })
            .catch(err => showToast("Error connecting to LAN: " + err, "error"));
        });
    }
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

/* ============================================================================
   2D INTERACTIVE HTML5 CANVAS MAP RENDERER ENGINE
   ============================================================================ */
let mapCanvas = null;
let mapCtx = null;
let mapZoomScale = 75; // Pixels per meter
let mapPanOffsetX = 0; // Pixels
let mapPanOffsetY = 0; // Pixels
let isMapDragging = false;
let mapDragStartX = 0;
let mapDragStartY = 0;
let mapPathTrail = [];
let lastTelemetryState = { x: 0.0, y: 0.0, heading: 0.0, tof_l: 1500, tof_c: 1800, tof_r: 1400 };

const mapCheckpoints = [
    { name: "START", x: 0.0, y: 0.0, uid: "04A7329B6C" },
    { name: "LAB", x: 2.0, y: 0.0, uid: "04B8418C7D" },
    { name: "STORAGE", x: 2.0, y: 2.0, uid: "04C9507D8E" },
    { name: "OFFICE", x: 0.0, y: 2.0, uid: "04DA616E9F" }
];

function initMapRenderer() {
    mapCanvas = document.getElementById("agvMapCanvas");
    if (!mapCanvas) return;
    mapCtx = mapCanvas.getContext("2d");

    // Handle high DPI crisp rendering
    resizeMapCanvas();
    window.addEventListener("resize", resizeMapCanvas);

    // Toolbar Control Buttons
    const btnZoomIn = document.getElementById("map-btn-zoom-in");
    const btnZoomOut = document.getElementById("map-btn-zoom-out");
    const btnReset = document.getElementById("map-btn-reset");
    const btnClearTrail = document.getElementById("map-btn-clear-trail");

    if (btnZoomIn) btnZoomIn.addEventListener("click", () => { mapZoomScale = Math.min(mapZoomScale * 1.25, 200); renderMapCanvas(); });
    if (btnZoomOut) btnZoomOut.addEventListener("click", () => { mapZoomScale = Math.max(mapZoomScale / 1.25, 25); renderMapCanvas(); });
    if (btnReset) btnReset.addEventListener("click", () => { mapZoomScale = 75; mapPanOffsetX = 0; mapPanOffsetY = 0; renderMapCanvas(); });
    if (btnClearTrail) btnClearTrail.addEventListener("click", () => { mapPathTrail = []; renderMapCanvas(); showToast("Map trajectory trail cleared", "info"); });

    // Mouse Panning
    mapCanvas.addEventListener("mousedown", (e) => {
        isMapDragging = true;
        mapDragStartX = e.clientX - mapPanOffsetX;
        mapDragStartY = e.clientY - mapPanOffsetY;
    });

    window.addEventListener("mousemove", (e) => {
        if (!isMapDragging) return;
        mapPanOffsetX = e.clientX - mapDragStartX;
        mapPanOffsetY = e.clientY - mapDragStartY;
        renderMapCanvas();
    });

    window.addEventListener("mouseup", () => { isMapDragging = false; });

    // Touch Panning for Mobile Phones
    mapCanvas.addEventListener("touchstart", (e) => {
        if (e.touches.length === 1) {
            isMapDragging = true;
            mapDragStartX = e.touches[0].clientX - mapPanOffsetX;
            mapDragStartY = e.touches[0].clientY - mapPanOffsetY;
        }
    }, { passive: true });

    mapCanvas.addEventListener("touchmove", (e) => {
        if (isMapDragging && e.touches.length === 1) {
            mapPanOffsetX = e.touches[0].clientX - mapDragStartX;
            mapPanOffsetY = e.touches[0].clientY - mapDragStartY;
            renderMapCanvas();
        }
    }, { passive: true });

    mapCanvas.addEventListener("touchend", () => { isMapDragging = false; });

    renderMapCanvas();
}

function resizeMapCanvas() {
    if (!mapCanvas || !mapCanvas.parentElement) return;
    const rect = mapCanvas.parentElement.getBoundingClientRect();
    const dpr = window.devicePixelRatio || 1;
    mapCanvas.width = rect.width * dpr;
    mapCanvas.height = rect.height * dpr;
    renderMapCanvas();
}

function updateMapTelemetry(tele) {
    if (!tele) return;
    const curX = parseFloat(tele.x) || 0;
    const curY = parseFloat(tele.y) || 0;
    const curHead = parseFloat(tele.heading) || 0;

    lastTelemetryState = {
        x: curX,
        y: curY,
        heading: curHead,
        tof_l: parseInt(tele.tof_left) || 1500,
        tof_c: parseInt(tele.tof_centre) || 1800,
        tof_r: parseInt(tele.tof_right) || 1400
    };

    // Append to path trail if moved > 2 cm
    if (mapPathTrail.length === 0) {
        mapPathTrail.push({ x: curX, y: curY });
    } else {
        const lastPt = mapPathTrail[mapPathTrail.length - 1];
        const dist = Math.hypot(curX - lastPt.x, curY - lastPt.y);
        if (dist > 0.02) {
            mapPathTrail.push({ x: curX, y: curY });
            if (mapPathTrail.length > 500) mapPathTrail.shift(); // Limit to 500 points
        }
    }

    // Update Coordinate Badge text
    const coordBadge = document.getElementById("map-coord-badge");
    if (coordBadge) {
        coordBadge.innerText = `X: ${curX.toFixed(2)}m | Y: ${curY.toFixed(2)}m | ${curHead.toFixed(1)}°`;
    }

    renderMapCanvas();
}

function renderMapCanvas() {
    if (!mapCanvas || !mapCtx) return;

    const width = mapCanvas.width;
    const height = mapCanvas.height;
    const dpr = window.devicePixelRatio || 1;

    mapCtx.save();
    mapCtx.scale(dpr, dpr);

    const cssWidth = width / dpr;
    const cssHeight = height / dpr;

    // 1. Clear Canvas Background
    mapCtx.fillStyle = "#090d16";
    mapCtx.fillRect(0, 0, cssWidth, cssHeight);

    // Origin in Screen Space (Center-bottom padded default)
    const originX = (cssWidth / 2) + mapPanOffsetX;
    const originY = (cssHeight / 2 + 50) + mapPanOffsetY;

    // Helper functions for Coordinate Conversion (Y-up Cartesian to Canvas Y-down)
    const worldToScreenX = (wx) => originX + (wx * mapZoomScale);
    const worldToScreenY = (wy) => originY - (wy * mapZoomScale);

    // 2. Draw Sub-meter Grid (0.2m minor lines)
    mapCtx.lineWidth = 0.5;
    mapCtx.strokeStyle = "rgba(30, 41, 59, 0.5)";
    const minorStep = 0.2 * mapZoomScale;
    for (let x = originX % minorStep; x < cssWidth; x += minorStep) {
        mapCtx.beginPath(); mapCtx.moveTo(x, 0); mapCtx.lineTo(x, cssHeight); mapCtx.stroke();
    }
    for (let y = originY % minorStep; y < cssHeight; y += minorStep) {
        mapCtx.beginPath(); mapCtx.moveTo(0, y); mapCtx.lineTo(cssWidth, y); mapCtx.stroke();
    }

    // 3. Draw 1-Meter Major Grid Lines & Coordinate Labels
    mapCtx.lineWidth = 1.2;
    mapCtx.strokeStyle = "rgba(51, 65, 85, 0.8)";
    mapCtx.fillStyle = "rgba(148, 163, 184, 0.6)";
    mapCtx.font = "10px monospace";

    const meterStep = mapZoomScale;
    const minMetersX = Math.floor((-originX) / meterStep);
    const maxMetersX = Math.ceil((cssWidth - originX) / meterStep);
    const minMetersY = Math.floor((originY - cssHeight) / meterStep);
    const maxMetersY = Math.ceil(originY / meterStep);

    for (let mx = minMetersX; mx <= maxMetersX; mx++) {
        const sx = worldToScreenX(mx);
        mapCtx.beginPath(); mapCtx.moveTo(sx, 0); mapCtx.lineTo(sx, cssHeight); mapCtx.stroke();
        mapCtx.fillText(`${mx}m`, sx + 3, originY - 4);
    }

    for (let my = minMetersY; my <= maxMetersY; my++) {
        const sy = worldToScreenY(my);
        mapCtx.beginPath(); mapCtx.moveTo(0, sy); mapCtx.lineTo(cssWidth, sy); mapCtx.stroke();
        if (my !== 0) mapCtx.fillText(`${my}m`, originX + 4, sy - 3);
    }

    // 4. Draw Origin Axis Crosshair
    mapCtx.lineWidth = 2.0;
    mapCtx.strokeStyle = "#3b82f6";
    mapCtx.beginPath(); mapCtx.moveTo(originX, 0); mapCtx.lineTo(originX, cssHeight); mapCtx.stroke();
    mapCtx.strokeStyle = "#10b981";
    mapCtx.beginPath(); mapCtx.moveTo(0, originY); mapCtx.lineTo(cssWidth, originY); mapCtx.stroke();

    // 5. Draw Dashed Route Path Loop Connecting RFID Checkpoints
    mapCtx.save();
    mapCtx.setLineDash([6, 6]);
    mapCtx.strokeStyle = "rgba(59, 130, 246, 0.35)";
    mapCtx.lineWidth = 2.0;
    mapCtx.beginPath();
    for (let i = 0; i < mapCheckpoints.length; i++) {
        const sx = worldToScreenX(mapCheckpoints[i].x);
        const sy = worldToScreenY(mapCheckpoints[i].y);
        if (i === 0) mapCtx.moveTo(sx, sy);
        else mapCtx.lineTo(sx, sy);
    }
    mapCtx.closePath();
    mapCtx.stroke();
    mapCtx.restore();

    // 6. Draw Glowing RFID Checkpoint Nodes
    for (let i = 0; i < mapCheckpoints.length; i++) {
        const cp = mapCheckpoints[i];
        const sx = worldToScreenX(cp.x);
        const sy = worldToScreenY(cp.y);

        // Outer Glow
        mapCtx.beginPath();
        mapCtx.arc(sx, sy, 10, 0, 2 * Math.PI);
        mapCtx.fillStyle = "rgba(6, 182, 212, 0.25)";
        mapCtx.fill();

        // Inner Circle
        mapCtx.beginPath();
        mapCtx.arc(sx, sy, 5, 0, 2 * Math.PI);
        mapCtx.fillStyle = "#06b6d4";
        mapCtx.shadowColor = "#06b6d4";
        mapCtx.shadowBlur = 8;
        mapCtx.fill();
        mapCtx.shadowBlur = 0;

        // Label Tag
        mapCtx.fillStyle = "#06b6d4";
        mapCtx.font = "bold 11px Inter, sans-serif";
        mapCtx.fillText(cp.name, sx + 12, sy + 4);
    }

    // 7. Draw Trajectory Trail History Line
    if (mapPathTrail.length > 1) {
        mapCtx.beginPath();
        mapCtx.strokeStyle = "#38bdf8";
        mapCtx.lineWidth = 3.0;
        mapCtx.shadowColor = "#0284c7";
        mapCtx.shadowBlur = 10;
        for (let i = 0; i < mapPathTrail.length; i++) {
            const sx = worldToScreenX(mapPathTrail[i].x);
            const sy = worldToScreenY(mapPathTrail[i].y);
            if (i === 0) mapCtx.moveTo(sx, sy);
            else mapCtx.lineTo(sx, sy);
        }
        mapCtx.stroke();
        mapCtx.shadowBlur = 0;
    }

    // 8. Draw Real-time AGV Robot Chassis & Sensors
    const agvSx = worldToScreenX(lastTelemetryState.x);
    const agvSy = worldToScreenY(lastTelemetryState.y);
    const headRad = (-lastTelemetryState.heading) * (Math.PI / 180.0); // Convert Cartesian angle to canvas rad

    mapCtx.save();
    mapCtx.translate(agvSx, agvSy);
    mapCtx.rotate(headRad);

    // Robot Dimensions in meters (0.35m x 0.26m scaled to zoom)
    const robotW = 0.35 * mapZoomScale;
    const robotH = 0.26 * mapZoomScale;

    // Draw ToF Sensor Range Arcs (Left: +45°, Centre: 0°, Right: -45°)
    const drawTofArc = (angleDeg, distMm) => {
        const rad = angleDeg * (Math.PI / 180.0);
        const maxDistM = Math.min(distMm / 1000.0, 2.0); // Cap visual distance to 2m
        const arcDistPx = maxDistM * mapZoomScale;

        mapCtx.save();
        mapCtx.rotate(-rad);
        mapCtx.beginPath();
        mapCtx.moveTo(robotW / 2, 0);
        mapCtx.arc(robotW / 2, 0, arcDistPx, -0.2, 0.2);
        mapCtx.closePath();

        let arcColor = "rgba(16, 185, 129, 0.2)";
        let strokeColor = "#10b981";
        if (distMm < 300) { arcColor = "rgba(239, 68, 68, 0.35)"; strokeColor = "#ef4444"; }
        else if (distMm < 600) { arcColor = "rgba(245, 158, 11, 0.3)"; strokeColor = "#f59e0b"; }

        mapCtx.fillStyle = arcColor;
        mapCtx.fill();
        mapCtx.strokeStyle = strokeColor;
        mapCtx.lineWidth = 1.5;
        mapCtx.stroke();
        mapCtx.restore();
    };

    drawTofArc(45, lastTelemetryState.tof_l);
    drawTofArc(0, lastTelemetryState.tof_c);
    drawTofArc(-45, lastTelemetryState.tof_r);

    // Robot Body Body Shadow & Fill
    mapCtx.shadowColor = "#eab308";
    mapCtx.shadowBlur = 12;
    mapCtx.fillStyle = "#1e293b";
    mapCtx.strokeStyle = "#eab308"; // Industrial Yellow Accent
    mapCtx.lineWidth = 2.5;

    mapCtx.beginPath();
    mapCtx.roundRect(-robotW / 2, -robotH / 2, robotW, robotH, 6);
    mapCtx.fill();
    mapCtx.stroke();
    mapCtx.shadowBlur = 0;

    // Wheel Markers (Left & Right differential drive wheels)
    mapCtx.fillStyle = "#64748b";
    mapCtx.fillRect(-robotW / 4, -robotH / 2 - 3, robotW / 2, 4);
    mapCtx.fillRect(-robotW / 4, robotH / 2 - 1, robotW / 2, 4);

    // Directional Front Arrow Marker
    mapCtx.fillStyle = "#eab308";
    mapCtx.beginPath();
    mapCtx.moveTo(robotW / 2 - 2, 0);
    mapCtx.lineTo(robotW / 2 - 12, -6);
    mapCtx.lineTo(robotW / 2 - 12, 6);
    mapCtx.closePath();
    mapCtx.fill();

    // Center Pivot Indicator
    mapCtx.beginPath();
    mapCtx.arc(0, 0, 4, 0, 2 * Math.PI);
    mapCtx.fillStyle = "#ef4444";
    mapCtx.fill();

    mapCtx.restore();
    mapCtx.restore();
}

/* ============================================================================
   DUAL-STICK ERGONOMIC MOBILE TOUCH CONTROLLER ENGINE
   ============================================================================ */
function initTouchJoystick() {
    const btnDpad = document.getElementById("btn-toggle-dpad");
    const btnJoystick = document.getElementById("btn-toggle-joystick");
    const dpadWrapper = document.getElementById("dpad-wrapper");
    const joystickWrapper = document.getElementById("joystick-wrapper");

    const leftBase = document.getElementById("joystick-left-base");
    const leftThumb = document.getElementById("joystick-left-thumb");
    const throttleBadge = document.getElementById("joystick-throttle-badge");

    const rightBase = document.getElementById("joystick-right-base");
    const rightThumb = document.getElementById("joystick-right-thumb");
    const steeringBadge = document.getElementById("joystick-steering-badge");

    if (!leftBase || !leftThumb || !rightBase || !rightThumb) return;

    // Mode Toggle (D-PAD vs DUAL-STICK)
    if (btnDpad && btnJoystick && dpadWrapper && joystickWrapper) {
        btnDpad.addEventListener("click", () => {
            btnDpad.classList.add("active");
            btnJoystick.classList.remove("active");
            dpadWrapper.style.display = "flex";
            joystickWrapper.style.display = "none";
        });

        btnJoystick.addEventListener("click", () => {
            btnJoystick.classList.add("active");
            btnDpad.classList.remove("active");
            dpadWrapper.style.display = "none";
            joystickWrapper.style.display = "flex";
        });
    }

    const maxRadius = 40; // Max thumb travel distance in pixels
    let lastSendTime = 0;

    // Stick States
    let leftTouchId = null;
    let rightTouchId = null;

    let throttleVal = 0.0; // -1.0 (Reverse) to +1.0 (Forward)
    let steeringVal = 0.0; // -1.0 (Left) to +1.0 (Right)

    let leftRect = null;
    let rightRect = null;

    const dispatchCombinedMotion = () => {
        const now = Date.now();
        if (now - lastSendTime < COMMAND_THROTTLE_MS) return;

        let cmd = "STOP";
        let speedPct = 0;

        if (Math.abs(throttleVal) > 0.15) {
            cmd = throttleVal > 0 ? "FORWARD" : "REVERSE";
            speedPct = Math.round(Math.abs(throttleVal) * manualDriveSpeed);
        } else if (Math.abs(steeringVal) > 0.15) {
            cmd = steeringVal < 0 ? "LEFT" : "RIGHT";
            speedPct = Math.round(Math.abs(steeringVal) * manualDriveSpeed);
        }

        lastSendTime = now;
        sendManualDriveCommand(cmd, speedPct);
    };

    const updateLeftStick = (clientY) => {
        if (!leftRect) return;
        const centerY = leftRect.top + leftRect.height / 2;
        let deltaY = clientY - centerY;

        if (deltaY < -maxRadius) deltaY = -maxRadius;
        if (deltaY > maxRadius) deltaY = maxRadius;

        leftThumb.style.transform = `translate(0px, ${deltaY}px)`;
        throttleVal = (-deltaY) / maxRadius; // Invert Y so up is forward

        const pct = Math.round(Math.abs(throttleVal) * 100);
        if (throttleBadge) {
            if (Math.abs(throttleVal) <= 0.15) {
                throttleBadge.innerText = "FWD / REV: 0%";
                throttleBadge.className = "joystick-badge";
            } else {
                const labelStr = throttleVal > 0 ? "FWD" : "REV";
                throttleBadge.innerText = `${labelStr}: ${pct}%`;
                throttleBadge.className = "joystick-badge active";
            }
        }
        dispatchCombinedMotion();
    };

    const updateRightStick = (clientX) => {
        if (!rightRect) return;
        const centerX = rightRect.left + rightRect.width / 2;
        let deltaX = clientX - centerX;

        if (deltaX < -maxRadius) deltaX = -maxRadius;
        if (deltaX > maxRadius) deltaX = maxRadius;

        rightThumb.style.transform = `translate(${deltaX}px, 0px)`;
        steeringVal = deltaX / maxRadius;

        const pct = Math.round(Math.abs(steeringVal) * 100);
        if (steeringBadge) {
            if (Math.abs(steeringVal) <= 0.15) {
                steeringBadge.innerText = "TURN: CENTER";
                steeringBadge.className = "joystick-badge";
            } else {
                const labelStr = steeringVal < 0 ? "LEFT" : "RIGHT";
                steeringBadge.innerText = `${labelStr}: ${pct}%`;
                steeringBadge.className = "joystick-badge active";
            }
        }
        dispatchCombinedMotion();
    };

    const resetLeftStick = () => {
        leftTouchId = null;
        throttleVal = 0.0;
        leftThumb.style.transform = "translate(0px, 0px)";
        leftBase.classList.remove("active");
        if (throttleBadge) {
            throttleBadge.innerText = "FWD / REV: 0%";
            throttleBadge.className = "joystick-badge";
        }
        if (Math.abs(steeringVal) <= 0.15) {
            sendManualDriveCommand("STOP", 0);
        }
    };

    const resetRightStick = () => {
        rightTouchId = null;
        steeringVal = 0.0;
        rightThumb.style.transform = "translate(0px, 0px)";
        rightBase.classList.remove("active");
        if (steeringBadge) {
            steeringBadge.innerText = "TURN: CENTER";
            steeringBadge.className = "joystick-badge";
        }
        if (Math.abs(throttleVal) <= 0.15) {
            sendManualDriveCommand("STOP", 0);
        }
    };

    // Multi-Touch Handlers
    leftBase.addEventListener("touchstart", (e) => {
        for (let i = 0; i < e.changedTouches.length; i++) {
            if (leftTouchId === null) {
                const t = e.changedTouches[i];
                leftTouchId = t.identifier;
                leftBase.classList.add("active");
                leftRect = leftBase.getBoundingClientRect();
                updateLeftStick(t.clientY);
                break;
            }
        }
    }, { passive: true });

    rightBase.addEventListener("touchstart", (e) => {
        for (let i = 0; i < e.changedTouches.length; i++) {
            if (rightTouchId === null) {
                const t = e.changedTouches[i];
                rightTouchId = t.identifier;
                rightBase.classList.add("active");
                rightRect = rightBase.getBoundingClientRect();
                updateRightStick(t.clientX);
                break;
            }
        }
    }, { passive: true });

    window.addEventListener("touchmove", (e) => {
        for (let i = 0; i < e.touches.length; i++) {
            const t = e.touches[i];
            if (t.identifier === leftTouchId) {
                updateLeftStick(t.clientY);
            } else if (t.identifier === rightTouchId) {
                updateRightStick(t.clientX);
            }
        }
    }, { passive: true });

    window.addEventListener("touchend", (e) => {
        for (let i = 0; i < e.changedTouches.length; i++) {
            const id = e.changedTouches[i].identifier;
            if (id === leftTouchId) resetLeftStick();
            if (id === rightTouchId) resetRightStick();
        }
    });

    window.addEventListener("touchcancel", (e) => {
        for (let i = 0; i < e.changedTouches.length; i++) {
            const id = e.changedTouches[i].identifier;
            if (id === leftTouchId) resetLeftStick();
            if (id === rightTouchId) resetRightStick();
        }
    });

    // Mouse Drag Support for PC Testing
    let isMouseLeft = false;
    let isMouseRight = false;

    leftBase.addEventListener("mousedown", (e) => {
        isMouseLeft = true;
        leftBase.classList.add("active");
        leftRect = leftBase.getBoundingClientRect();
        updateLeftStick(e.clientY);
    });

    rightBase.addEventListener("mousedown", (e) => {
        isMouseRight = true;
        rightBase.classList.add("active");
        rightRect = rightBase.getBoundingClientRect();
        updateRightStick(e.clientX);
    });

    window.addEventListener("mousemove", (e) => {
        if (isMouseLeft) updateLeftStick(e.clientY);
        if (isMouseRight) updateRightStick(e.clientX);
    });

    window.addEventListener("mouseup", () => {
        if (isMouseLeft) { isMouseLeft = false; resetLeftStick(); }
        if (isMouseRight) { isMouseRight = false; resetRightStick(); }
    });
}

/* ============================================================================
   HARDWARE PERFORMANCE CANVAS CHART RENDERER ENGINE
   ============================================================================ */
class AGVPerfChart {
    constructor(canvasId, strokeColor, fillColor) {
        this.canvas = document.getElementById(canvasId);
        if (!this.canvas) return;
        this.ctx = this.canvas.getContext("2d");
        this.strokeColor = strokeColor;
        this.fillColor = fillColor;
        this.data = new Array(50).fill(0); // 50 samples history ring buffer
    }

    pushValue(val) {
        if (!this.canvas || !this.ctx) return;
        val = Math.max(0, Math.min(100, val));
        this.data.shift();
        this.data.push(val);
        this.render();
    }

    render() {
        if (!this.canvas || !this.ctx) return;
        const w = this.canvas.width = this.canvas.clientWidth || 400;
        const h = this.canvas.height = this.canvas.clientHeight || 140;
        const ctx = this.ctx;

        ctx.clearRect(0, 0, w, h);

        // Draw horizontal grid lines (25%, 50%, 75%)
        ctx.strokeStyle = "rgba(255, 255, 255, 0.06)";
        ctx.lineWidth = 1;
        for (let pct of [0.25, 0.50, 0.75]) {
            const y = h * (1 - pct);
            ctx.beginPath();
            ctx.moveTo(0, y);
            ctx.lineTo(w, y);
            ctx.stroke();
        }

        if (this.data.length < 2) return;

        const stepX = w / (this.data.length - 1);

        // Draw filled gradient area under curve
        ctx.beginPath();
        ctx.moveTo(0, h);
        for (let i = 0; i < this.data.length; i++) {
            const x = i * stepX;
            const y = h - (this.data[i] / 100) * (h - 10);
            ctx.lineTo(x, y);
        }
        ctx.lineTo(w, h);
        ctx.closePath();

        const grad = ctx.createLinearGradient(0, 0, 0, h);
        grad.addColorStop(0, this.fillColor);
        grad.addColorStop(1, "rgba(0, 0, 0, 0)");
        ctx.fillStyle = grad;
        ctx.fill();

        // Draw glowing line curve
        ctx.beginPath();
        for (let i = 0; i < this.data.length; i++) {
            const x = i * stepX;
            const y = h - (this.data[i] / 100) * (h - 10);
            if (i === 0) ctx.moveTo(x, y);
            else ctx.lineTo(x, y);
        }
        ctx.strokeStyle = this.strokeColor;
        ctx.lineWidth = 2;
        ctx.shadowColor = this.strokeColor;
        ctx.shadowBlur = 8;
        ctx.stroke();
        ctx.shadowBlur = 0;
    }
}

let cpuPerfChart = null;
let ramPerfChart = null;

function initPerfCharts() {
    cpuPerfChart = new AGVPerfChart("chart-cpu-canvas", "#38bdf8", "rgba(56, 189, 248, 0.25)");
    ramPerfChart = new AGVPerfChart("chart-ram-canvas", "#c084fc", "rgba(192, 132, 252, 0.25)");
}

/* ============================================================================
   PID TUNE MANAGER
   ============================================================================ */
function initPIDTuneManager() {
    // Load encoder params on boot
    fetch(`${apiUrl}/api/settings`)
    .then(res => res.json())
    .then(settings => {
        if(settings.enc_ppr_l !== undefined) document.getElementById("set-enc-ppr-l").value = settings.enc_ppr_l;
        if(settings.enc_ppr_r !== undefined) document.getElementById("set-enc-ppr-r").value = settings.enc_ppr_r;
        if(settings.wheel_circ_mm !== undefined) document.getElementById("set-wheel-circ").value = settings.wheel_circ_mm;
    });

    // Motor Trim slider live labels
    const bindSliderLabel = (sliderId, labelId) => {
        const slider = document.getElementById(sliderId);
        if(slider) {
            slider.addEventListener("input", (e) => {
                document.getElementById(labelId).innerText = e.target.value;
            });
        }
    };
    bindSliderLabel("set-trim-l-fwd", "lbl-trim-l-fwd");
    bindSliderLabel("set-trim-r-fwd", "lbl-trim-r-fwd");
    bindSliderLabel("set-trim-l-turn", "lbl-trim-l-turn");
    bindSliderLabel("set-trim-r-turn", "lbl-trim-r-turn");

    // PID realtime WebSocket broadcast
    let pidState = {
        left: { kp: 1.0, ki: 0.0, kd: 0.0 },
        right: { kp: 1.0, ki: 0.0, kd: 0.0 }
    };
    
    // Attempt to load current values from settings on boot
    fetch(`${apiUrl}/api/settings`).then(res => res.json()).then(settings => {
        if (settings.pid_kp_l !== undefined) {
            pidState.left.kp = settings.pid_kp_l;
            pidState.left.ki = settings.pid_ki_l;
            pidState.left.kd = settings.pid_kd_l;
            pidState.right.kp = settings.pid_kp_r;
            pidState.right.ki = settings.pid_ki_r;
            pidState.right.kd = settings.pid_kd_r;
            updatePIDUIInputs();
        }
    });

    const inpKpL = document.getElementById("set-pid-kp-l");
    const inpKiL = document.getElementById("set-pid-ki-l");
    const inpKdL = document.getElementById("set-pid-kd-l");
    const inpKpR = document.getElementById("set-pid-kp-r");
    const inpKiR = document.getElementById("set-pid-ki-r");
    const inpKdR = document.getElementById("set-pid-kd-r");
    const btnApplyLive = document.getElementById("btn-pid-apply-live");
    
    const updatePIDUIInputs = () => {
        if(!inpKpL) return;
        inpKpL.value = pidState.left.kp;
        inpKiL.value = pidState.left.ki;
        inpKdL.value = pidState.left.kd;
        inpKpR.value = pidState.right.kp;
        inpKiR.value = pidState.right.ki;
        inpKdR.value = pidState.right.kd;
    };
    
    const readInputsIntoState = () => {
        if(!inpKpL) return;
        pidState.left.kp = parseFloat(inpKpL.value) || 0;
        pidState.left.ki = parseFloat(inpKiL.value) || 0;
        pidState.left.kd = parseFloat(inpKdL.value) || 0;
        pidState.right.kp = parseFloat(inpKpR.value) || 0;
        pidState.right.ki = parseFloat(inpKiR.value) || 0;
        pidState.right.kd = parseFloat(inpKdR.value) || 0;
    };

    if (inpKpL) inpKpL.addEventListener("input", readInputsIntoState);
    if (inpKiL) inpKiL.addEventListener("input", readInputsIntoState);
    if (inpKdL) inpKdL.addEventListener("input", readInputsIntoState);
    if (inpKpR) inpKpR.addEventListener("input", readInputsIntoState);
    if (inpKiR) inpKiR.addEventListener("input", readInputsIntoState);
    if (inpKdR) inpKdR.addEventListener("input", readInputsIntoState);

    const sendPidTuning = () => {
        if (!wsConn || wsConn.readyState !== WebSocket.OPEN) return;
        readInputsIntoState();
        const data = {
            kpL: pidState.left.kp,
            kiL: pidState.left.ki,
            kdL: pidState.left.kd,
            kpR: pidState.right.kp,
            kiR: pidState.right.ki,
            kdR: pidState.right.kd
        };
        wsConn.send(JSON.stringify({ type: "update_pid", data: data }));
        showToast("Live PID Applied!", "info");
    };
    
    if (btnApplyLive) {
        btnApplyLive.addEventListener("click", sendPidTuning);
    }

    // Chart.js init
    if (typeof Chart !== 'undefined') {
        const createPidChart = (ctxId) => {
            const ctx = document.getElementById(ctxId);
            if (!ctx) return null;
            return new Chart(ctx, {
                type: 'line',
                data: {
                    labels: Array(50).fill(''),
                    datasets: [
                        { label: 'Target RPM', borderColor: 'rgba(239, 68, 68, 1)', data: Array(50).fill(0), fill: false, pointRadius: 0, tension: 0.1, borderWidth: 2 },
                        { label: 'Actual RPM', borderColor: 'rgba(59, 130, 246, 1)', data: Array(50).fill(0), fill: false, pointRadius: 0, tension: 0.1, borderWidth: 2 }
                    ]
                },
                options: {
                    responsive: true,
                    maintainAspectRatio: false,
                    animation: false,
                    scales: {
                        y: { min: -400, max: 400 },
                        x: { display: false }
                    },
                    plugins: { legend: { display: true, position: 'top' } }
                }
            });
        };
        
        window.pidChartLeft = createPidChart('chart-motor-pid-l');
        window.pidChartRight = createPidChart('chart-motor-pid-r');
        window.pidDataLeft = { target: Array(50).fill(0), actual: Array(50).fill(0) };
        window.pidDataRight = { target: Array(50).fill(0), actual: Array(50).fill(0) };
        
        window.updatePidCharts = function(target, actualL, actualR) {
            window.pidDataLeft.target.shift(); window.pidDataLeft.target.push(target);
            window.pidDataLeft.actual.shift(); window.pidDataLeft.actual.push(actualL);
            
            window.pidDataRight.target.shift(); window.pidDataRight.target.push(target);
            window.pidDataRight.actual.shift(); window.pidDataRight.actual.push(actualR);
            
            if (window.pidChartLeft) {
                window.pidChartLeft.data.datasets[0].data = window.pidDataLeft.target;
                window.pidChartLeft.data.datasets[1].data = window.pidDataLeft.actual;
                window.pidChartLeft.update();
            }
            if (window.pidChartRight) {
                window.pidChartRight.data.datasets[0].data = window.pidDataRight.target;
                window.pidChartRight.data.datasets[1].data = window.pidDataRight.actual;
                window.pidChartRight.update();
            }
        };
    }

    // Save button logic
    const saveBtn = document.getElementById("btn-pid-save");
    if(saveBtn) {
        saveBtn.addEventListener("click", () => {
            const body = {
                pid_kp_l: pidState.left.kp,
                pid_ki_l: pidState.left.ki,
                pid_kd_l: pidState.left.kd,
                pid_kp_r: pidState.right.kp,
                pid_ki_r: pidState.right.ki,
                pid_kd_r: pidState.right.kd,
                enc_ppr_l: parseInt(document.getElementById("set-enc-ppr-l").value),
                enc_ppr_r: parseInt(document.getElementById("set-enc-ppr-r").value),
                wheel_circ_mm: parseFloat(document.getElementById("set-wheel-circ").value),
                motor_l_fwd_scale: parseInt(document.getElementById("set-trim-l-fwd").value),
                motor_r_fwd_scale: parseInt(document.getElementById("set-trim-r-fwd").value),
                motor_l_turn_scale: parseInt(document.getElementById("set-trim-l-turn").value),
                motor_r_turn_scale: parseInt(document.getElementById("set-trim-r-turn").value)
            };

            fetch(`${apiUrl}/api/settings`, {
                method: "POST",
                headers: { "Content-Type": "application/json" },
                body: JSON.stringify(body)
            })
            .then(res => res.json())
            .then(data => {
                if (data.status === "success") {
                    showToast("PID & Encoder settings saved!", "success");
                } else {
                    showToast("Failed to save settings.", "error");
                }
            })
            .catch(err => {
                showToast("Error communicating with AGV.", "error");
            });
        });
    }

    // System Config: ToF Settings
    fetch(`${apiUrl}/api/settings`)
    .then(res => res.json())
    .then(settings => {
        if(settings.tof_stop_distance_mm !== undefined) {
            document.getElementById("set-tof-stop-dist").value = settings.tof_stop_distance_mm;
        }
        if(settings.drive_method !== undefined) {
            document.getElementById("set-drive-method").value = settings.drive_method;
        }
        if(settings.lookahead_distance !== undefined) {
            document.getElementById("set-lookahead").value = settings.lookahead_distance;
        }
    });

    const tofSaveBtn = document.getElementById("btn-tof-save");
    if (tofSaveBtn) {
        tofSaveBtn.addEventListener("click", () => {
            const dist = parseFloat(document.getElementById("set-tof-stop-dist").value);
            
            // Send to HTTP API to persist to LittleFS
            fetch(`${apiUrl}/api/settings`, {
                method: "POST",
                headers: { "Content-Type": "application/json" },
                body: JSON.stringify({ tof_stop_distance_mm: dist })
            })
            .then(res => res.json())
            .then(data => {
                if (data.status === "success") {
                    showToast("ToF config saved via API!", "success");
                }
            });

            // Send via WebSockets for immediate live update to STM32
            if (wsConn && wsConn.readyState === WebSocket.OPEN) {
                wsConn.send(JSON.stringify({
                    type: "update_tof_config",
                    data: { stop_distance_mm: dist }
                }));
            }
        });
    }

    // System Config: IMU Calibration
    const imuCalBtn = document.getElementById("btn-imu-calibrate");
    if (imuCalBtn) {
        imuCalBtn.addEventListener("click", () => {
            if (confirm("Ensure the AGV is perfectly stationary before calibrating the IMU. Proceed?")) {
                if (wsConn && wsConn.readyState === WebSocket.OPEN) {
                    wsConn.send(JSON.stringify({ type: "calibrate_imu" }));
                    showToast("Calibration command sent to STM32", "info");
                } else {
                    showToast("WebSocket disconnected", "error");
                }
            }
        });
    }

    // System Config: Nav Settings
    const navSaveBtn = document.getElementById("btn-nav-save");
    if (navSaveBtn) {
        navSaveBtn.addEventListener("click", () => {
            const method = parseInt(document.getElementById("set-drive-method").value);
            const lookahead = parseFloat(document.getElementById("set-lookahead").value);
            
            fetch(`${apiUrl}/api/settings`, {
                method: "POST",
                headers: { "Content-Type": "application/json" },
                body: JSON.stringify({ 
                    drive_method: method,
                    lookahead_distance: lookahead
                })
            })
            .then(res => res.json())
            .then(data => {
                if (data.status === "success") {
                    showToast("Navigation settings saved!", "success");
                }
            });
        });
    }

    // System Config: Tab Switching Logic
    const configTabBtns = document.querySelectorAll(".config-tab-btn");
    const configTabContents = document.querySelectorAll(".config-tab-content");
    
    configTabBtns.forEach(btn => {
        btn.addEventListener("click", () => {
            configTabBtns.forEach(b => {
                b.classList.remove("active");
                b.style.color = "var(--text-muted)";
                b.style.borderBottom = "2px solid transparent";
            });
            btn.classList.add("active");
            btn.style.color = "var(--text-main)";
            btn.style.borderBottom = "2px solid var(--color-blue)";
            configTabContents.forEach(content => content.style.display = "none");
            const targetId = btn.getAttribute("data-target");
            const targetEl = document.getElementById(targetId);
            if (targetEl) targetEl.style.display = "block";
        });
    });

    // System Config: PPR Calibration Tool
    let isPPRMeasuring = false;
    let startRaw = 0;
    let calibPPR = 0;
    let calibSide = "left";

    const btnPprStart = document.getElementById("btn-ppr-start");
    const btnPprStop = document.getElementById("btn-ppr-stop");
    const btnPprApply = document.getElementById("btn-ppr-apply");

    if (btnPprStart) {
        btnPprStart.addEventListener("click", () => {
            isPPRMeasuring = true;
            calibSide = document.getElementById("calib-ppr-side").value;
            startRaw = calibSide === "left" ? (window.latestRawEncL || 0) : (window.latestRawEncR || 0);
            
            document.getElementById("sys-calib-ppr-result").innerText = "Measuring...";
            
            btnPprStart.style.display = "none";
            btnPprStop.style.display = "flex";
            btnPprApply.style.display = "none";
            showToast("Measurement started. Manually rotate the " + calibSide + " wheel now.", "info");
        });
    }

    if (btnPprStop) {
        btnPprStop.addEventListener("click", () => {
            isPPRMeasuring = false;
            const endRaw = calibSide === "left" ? (window.latestRawEncL || 0) : (window.latestRawEncR || 0);
            const cycles = parseFloat(document.getElementById("calib-ppr-cycles").value) || 1;
            
            calibPPR = Math.round(Math.abs(endRaw - startRaw) / cycles);
            
            document.getElementById("sys-calib-ppr-result").innerText = calibPPR;
            
            btnPprStart.style.display = "flex";
            btnPprStart.innerText = "RESTART MEASUREMENT";
            btnPprStop.style.display = "none";
            btnPprApply.style.display = "flex";
            showToast("Measurement complete!", "success");
        });
    }

    if (btnPprApply) {
        btnPprApply.addEventListener("click", () => {
            let payload = {};
            if (calibSide === "left") {
                document.getElementById("set-enc-ppr-l").value = calibPPR;
                payload = { enc_ppr_l: calibPPR };
            } else {
                document.getElementById("set-enc-ppr-r").value = calibPPR;
                payload = { enc_ppr_r: calibPPR };
            }
            
            fetch(`${apiUrl}/api/settings`, {
                method: "POST",
                headers: { "Content-Type": "application/json" },
                body: JSON.stringify(payload)
            })
            .then(res => res.json())
            .then(data => {
                if (data.status === "success") {
                    showToast(`New ${calibSide} PPR applied and saved!`, "success");
                    btnPprApply.style.display = "none";
                    btnPprStart.innerText = "START MEASUREMENT";
                }
            });
        });
    }
}
