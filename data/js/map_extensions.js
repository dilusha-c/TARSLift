// map_extensions.js
// Extensions for Train Mode Live Map & Route Manager Modal

let trainMapCanvas = null;
let trainMapCtx = null;
let previewMapCanvas = null;
let previewMapCtx = null;

let trainPathTrail = [];
let routePreviewData = null;

document.addEventListener("DOMContentLoaded", () => {
    trainMapCanvas = document.getElementById("trainMapCanvas");
    if (trainMapCanvas) {
        trainMapCtx = trainMapCanvas.getContext("2d");
        resizeCanvas(trainMapCanvas);
    }
    
    previewMapCanvas = document.getElementById("previewMapCanvas");
    if (previewMapCanvas) {
        previewMapCtx = previewMapCanvas.getContext("2d");
        resizeCanvas(previewMapCanvas);
    }

    // Modal Close logic
    const btnViewRouteClose = document.getElementById("btn-view-route-close");
    if (btnViewRouteClose) {
        btnViewRouteClose.addEventListener("click", () => {
            document.getElementById("view-route-modal").classList.remove("active");
        });
    }

    // Hook into the original updateMapTelemetry to update Train map
    if (typeof updateMapTelemetry === 'function') {
        const originalUpdate = updateMapTelemetry;
        updateMapTelemetry = function(tele) {
            originalUpdate(tele);
            
            if (currentActiveView === "teach") {
                const curX = parseFloat(tele.x) || 0;
                const curY = parseFloat(tele.y) || 0;
                const curHead = parseFloat(tele.heading) || 0;
                
                // Append to train trail
                if (trainPathTrail.length === 0) {
                    trainPathTrail.push({x: curX, y: curY});
                } else {
                    const lastPt = trainPathTrail[trainPathTrail.length - 1];
                    if (Math.hypot(curX - lastPt.x, curY - lastPt.y) > 0.02) {
                        trainPathTrail.push({x: curX, y: curY});
                    }
                }
                
                const coordBadge = document.getElementById("train-map-coord-badge");
                if (coordBadge) {
                    coordBadge.innerText = `X: ${curX.toFixed(2)}m | Y: ${curY.toFixed(2)}m | ${curHead.toFixed(1)}°`;
                }

                if (trainMapCanvas && (trainMapCanvas.width === 0 || trainMapCanvas.width !== trainMapCanvas.parentElement.getBoundingClientRect().width * (window.devicePixelRatio || 1))) {
                    resizeCanvas(trainMapCanvas);
                }

                drawGenericMap(trainMapCanvas, trainMapCtx, trainPathTrail, curX, curY, curHead);
            }
        };
    }
    
    // Add event delegation to the routes table
    const routesTableBody = document.querySelector("#routes-table tbody");
    if (routesTableBody) {
        routesTableBody.addEventListener("click", (e) => {
            const tr = e.target.closest("tr");
            if (!tr) return;
            
            // Ignore if they clicked an action button
            if (e.target.tagName === "BUTTON" || e.target.closest("button")) {
                return;
            }
            
            const runBtn = tr.querySelector("button[onclick^='runRouteFromManager']");
            if (runBtn) {
                const onclick = runBtn.getAttribute("onclick");
                const match = onclick.match(/'([^']+)'/);
                if (match && match[1]) {
                    openRoutePreviewModal(match[1]);
                }
            }
        });
        
        // Add CSS to make rows look clickable via hovering
        const style = document.createElement("style");
        style.innerHTML = `
            #routes-table tbody tr:hover { cursor: pointer; background: rgba(255, 255, 255, 0.05); }
        `;
        document.head.appendChild(style);
    }
    
    // Clear trail when stopping teach mode
    const btnTeachStop = document.getElementById("btn-teach-stop");
    if (btnTeachStop) {
        btnTeachStop.addEventListener("click", () => {
            // keep it on screen until next start
        });
    }
    
    const btnTeachStart = document.getElementById("btn-teach-start");
    if (btnTeachStart) {
        btnTeachStart.addEventListener("click", () => {
            trainPathTrail = [];
        });
    }
});

window.addEventListener("resize", () => {
    resizeCanvas(trainMapCanvas);
    resizeCanvas(previewMapCanvas);
});

function resizeCanvas(canvas) {
    if (!canvas || !canvas.parentElement) return;
    const rect = canvas.parentElement.getBoundingClientRect();
    const dpr = window.devicePixelRatio || 1;
    canvas.width = rect.width * dpr;
    canvas.height = Math.max(300, rect.height) * dpr; // Minimum 300px height
}

function openRoutePreviewModal(routeId) {
    const modal = document.getElementById("view-route-modal");
    if (!modal) return;
    
    // Fetch full route details
    fetch(`${apiUrl}/api/route?id=${routeId}`)
    .then(res => res.json())
    .then(route => {
        document.getElementById("view-route-title").innerText = route.name;
        document.getElementById("view-route-checkpoints").innerText = route.checkpoints ? route.checkpoints.length : "-";
        document.getElementById("view-route-distance").innerText = `${route.distance} m`;
        document.getElementById("view-route-duration").innerText = `${route.duration}s`;
        
        const btnStart = document.getElementById("btn-view-route-start");
        btnStart.onclick = () => {
            modal.classList.remove("active");
            runRouteFromManager(routeId);
        };
        
        modal.classList.add("active");
        
        // Draw the route on preview map
        setTimeout(() => {
            resizeCanvas(previewMapCanvas);
            drawGenericMap(previewMapCanvas, previewMapCtx, route.trajectory || [], null, null, null);
        }, 300); // Give modal time to animate in
    })
    .catch(err => {
        showToast("Error loading route details", "error");
    });
}

function drawGenericMap(canvas, ctx, trajectory, currentX, currentY, currentHeading) {
    if (!canvas || !ctx) return;
    
    const width = canvas.width;
    const height = canvas.height;
    const dpr = window.devicePixelRatio || 1;
    
    ctx.save();
    ctx.scale(dpr, dpr);
    const cssWidth = width / dpr;
    const cssHeight = height / dpr;
    
    ctx.fillStyle = "#090d16";
    ctx.fillRect(0, 0, cssWidth, cssHeight);
    
    // Determine bounds
    let minX = -1, maxX = 1, minY = -1, maxY = 1;
    
    if (trajectory && trajectory.length > 0) {
        const xs = trajectory.map(p => (p.x !== undefined ? p.x : p[0]));
        const ys = trajectory.map(p => (p.y !== undefined ? p.y : p[1]));
        minX = Math.min(...xs);
        maxX = Math.max(...xs);
        minY = Math.min(...ys);
        maxY = Math.max(...ys);
    } else if (currentX !== null) {
        minX = currentX - 1; maxX = currentX + 1;
        minY = currentY - 1; maxY = currentY + 1;
    }
    
    // Add 1 meter padding
    minX -= 1; maxX += 1;
    minY -= 1; maxY += 1;
    
    const scale = Math.min(cssWidth / (maxX - minX), cssHeight / (maxY - minY)) * 0.9;
    
    const originX = cssWidth / 2 - ((maxX + minX) / 2) * scale;
    const originY = cssHeight / 2 + ((maxY + minY) / 2) * scale; 
    
    const transformX = (x) => originX + x * scale;
    const transformY = (y) => originY - (y * scale); // Invert Y so up is positive
    
    // Grid
    ctx.strokeStyle = "rgba(255, 255, 255, 0.1)";
    ctx.lineWidth = 1;
    ctx.beginPath();
    for (let x = Math.floor(minX); x <= Math.ceil(maxX); x++) {
        ctx.moveTo(transformX(x), 0);
        ctx.lineTo(transformX(x), cssHeight);
    }
    for (let y = Math.floor(minY); y <= Math.ceil(maxY); y++) {
        ctx.moveTo(0, transformY(y));
        ctx.lineTo(cssWidth, transformY(y));
    }
    ctx.stroke();
    
    // Draw Trajectory
    if (trajectory && trajectory.length > 1) {
        ctx.strokeStyle = "#3b82f6";
        ctx.lineWidth = 3;
        ctx.lineJoin = "round";
        ctx.beginPath();
        let started = false;
        trajectory.forEach(pt => {
            const tx = transformX(pt.x !== undefined ? pt.x : pt[0]);
            const ty = transformY(pt.y !== undefined ? pt.y : pt[1]);
            if (!started) {
                ctx.moveTo(tx, ty);
                started = true;
            } else {
                ctx.lineTo(tx, ty);
            }
        });
        ctx.stroke();
        
        // Mark Start and End Points
        const startPt = trajectory[0];
        const endPt = trajectory[trajectory.length-1];
        
        ctx.fillStyle = "#10b981"; // Green for start
        ctx.beginPath();
        ctx.arc(transformX(startPt.x !== undefined ? startPt.x : startPt[0]), transformY(startPt.y !== undefined ? startPt.y : startPt[1]), 6, 0, Math.PI*2);
        ctx.fill();
        
        ctx.fillStyle = "#ef4444"; // Red for end
        ctx.beginPath();
        ctx.arc(transformX(endPt.x !== undefined ? endPt.x : endPt[0]), transformY(endPt.y !== undefined ? endPt.y : endPt[1]), 6, 0, Math.PI*2);
        ctx.fill();
    }
    
    // Draw Current Position AGV Icon
    if (currentX !== null && currentY !== null && currentHeading !== null) {
        ctx.translate(transformX(currentX), transformY(currentY));
        // HTML Canvas rotation is clockwise. If our heading is Counter-Clockwise (standard math), invert it.
        // Assuming 0 is facing right (X axis), and 90 is facing up (Y axis).
        // Since Y is inverted visually, standard math is already correct, just invert angle.
        ctx.rotate((-currentHeading) * Math.PI / 180); 
        
        // AGV Triangle pointing Right (0 deg)
        ctx.fillStyle = "#f59e0b";
        ctx.beginPath();
        ctx.moveTo(12, 0);
        ctx.lineTo(-10, 10);
        ctx.lineTo(-6, 0);
        ctx.lineTo(-10, -10);
        ctx.fill();
    }
    
    ctx.restore();
}
