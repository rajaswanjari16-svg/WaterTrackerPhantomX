// =====================================================
// AQUATRACK 4P - CLIENT SCRIPT
// Synchronized with Node.js Express Server & ESP32
// Starts strictly from 0 mL for all 4 people
// =====================================================

const DAILY_GOAL_ML = 2000;
const GLASS_SIZE_ML = 250;
const TOTAL_GLASSES = 8;

// Local mirror of 4 people's hydration data - STARTS AT ZERO
let peopleData = [
    { id: 1, name: "Person 1", water: 0 },
    { id: 2, name: "Person 2", water: 0 },
    { id: 3, name: "Person 3", water: 0 },
    { id: 4, name: "Person 4", water: 0 }
];

let currentPair = 1; // 1 = P1/P2, 2 = P3/P4

// =====================================================
// DATE DISPLAY (ASIA/KOLKATA TIMEZONE)
// =====================================================

function updateDateDisplay(dateStr) {
    const dateElement = document.getElementById("currentDate");
    if (!dateElement) return;

    try {
        const now = new Date();
        const formatted = now.toLocaleDateString("en-IN", {
            timeZone: "Asia/Kolkata",
            weekday: "short",
            day: "numeric",
            month: "short",
            year: "numeric"
        });
        dateElement.textContent = formatted;
    } catch (e) {
        dateElement.textContent = dateStr || new Date().toDateString();
    }
}

// =====================================================
// SEND ACTION COMMAND TO SERVER
// =====================================================

async function sendCommand(action, personNumber) {
    const syncIndicator = document.getElementById("syncStatusIndicator");
    const syncText = document.getElementById("syncText");

    try {
        if (syncText) syncText.textContent = "Sending...";

        const response = await fetch("/api/command", {
            method: "POST",
            headers: {
                "Content-Type": "application/json"
            },
            body: JSON.stringify({
                action: action,
                person: personNumber,
                amount: action === "add" ? 250 : undefined
            })
        });

        const data = await response.json();

        if (!response.ok) {
            console.warn("[COMMAND ERROR]", data.message);
            return;
        }

        console.log(`[COMMAND SENT] ${action} for Person ${personNumber}`);

        // Fetch state quickly after command so UI updates immediately
        setTimeout(fetchServerState, 150);

    } catch (error) {
        console.error("Failed to send command to server:", error);
        if (syncIndicator) syncIndicator.classList.add("sync-error");
        if (syncText) syncText.textContent = "Offline";
    }
}

// Global functions attached to buttons in index.html
function drinkWater(personNumber) {
    sendCommand("add", personNumber);
}

function undoWater(personNumber) {
    sendCommand("undo", personNumber);
}

// Reset all 4 people back to 0 mL
async function resetAllData() {
    try {
        const response = await fetch("/api/reset", { method: "POST" });
        if (response.ok) {
            peopleData.forEach(p => p.water = 0);
            peopleData.forEach(p => updatePersonUI(p));
            console.log("[RESET] Reset all data to 0 mL");
            setTimeout(fetchServerState, 100);
        }
    } catch (e) {
        console.error("Failed to reset:", e);
    }
}

// =====================================================
// UPDATE INDIVIDUAL PERSON CARD UI
// =====================================================

function updatePersonUI(person) {
    const id = person.id;
    const water = Math.max(0, person.water || 0);

    // 1. Percentage (exact, capped at 100%)
    const rawPercent = (water / DAILY_GOAL_ML) * 100;
    const safePercent = Math.min(100, Math.round(rawPercent));

    // 2. Glass-equivalent progress (e.g., 450 mL = 1.8 glasses; 500 mL = 2 glasses)
    const glassEquiv = water / GLASS_SIZE_ML;
    const glassDisplay = Number.isInteger(glassEquiv) 
        ? `${glassEquiv} / ${TOTAL_GLASSES} glasses` 
        : `${glassEquiv.toFixed(1)} / ${TOTAL_GLASSES} glasses`;

    // 3. Litre value (e.g., 0.45 L or 2.00 L)
    const literDisplay = `${(water / 1000).toFixed(2)} L`;

    // DOM Elements
    const cardEl       = document.getElementById(`card${id}`);
    const waterEl      = document.getElementById(`water${id}`);
    const litersEl     = document.getElementById(`liters${id}`);
    const glassesEl    = document.getElementById(`glasses${id}`);
    const percentEl    = document.getElementById(`percent${id}`);
    const circleEl     = document.getElementById(`circle${id}`);
    const barEl        = document.getElementById(`bar${id}`);
    const statusEl     = document.getElementById(`status${id}`);
    const badgeEl      = document.getElementById(`badge${id}`);
    const undoBtn      = document.getElementById(`undoBtn${id}`);

    // Update Text Content
    if (waterEl)   waterEl.textContent   = `${water} mL`;
    if (litersEl)  litersEl.textContent  = literDisplay;
    if (glassesEl) glassesEl.textContent = glassDisplay;
    if (percentEl) percentEl.textContent = `${safePercent}%`;

    // Update Progress Bar
    if (barEl) {
        barEl.style.width = `${Math.min(100, rawPercent)}%`;
    }

    // Update Circular Conic Progress
    if (circleEl) {
        const degrees = (safePercent / 100) * 360;
        circleEl.style.background = `conic-gradient(#38bdf8 ${degrees}deg, rgba(255, 255, 255, 0.08) ${degrees}deg)`;
    }

    // Status Message & Goal Completion
    const isCompleted = (water >= DAILY_GOAL_ML);

    if (statusEl) {
        if (isCompleted) {
            statusEl.textContent = "Daily Goal Completed! 🎉";
        } else {
            const remaining = DAILY_GOAL_ML - water;
            const remainingGlasses = (remaining / GLASS_SIZE_ML).toFixed(1);
            statusEl.textContent = `${remaining} mL remaining (${remainingGlasses} glasses)`;
        }
    }

    if (badgeEl) {
        badgeEl.style.display = isCompleted ? "block" : "none";
    }

    if (cardEl) {
        if (isCompleted) {
            cardEl.classList.add("goal-completed");
        } else {
            cardEl.classList.remove("goal-completed");
        }
    }

    // Disable Undo button when water is 0 mL
    if (undoBtn) {
        undoBtn.disabled = (water <= 0);
    }
}

// =====================================================
// UPDATE ACTIVE HARDWARE PAIR DISPLAY
// =====================================================

function updatePairUI(pair) {
    currentPair = pair || 1;

    const activePairDisplay = document.getElementById("activePairDisplay");
    const activePairHint    = document.getElementById("activePairHint");

    if (activePairDisplay) {
        if (currentPair === 2) {
            activePairDisplay.innerHTML = 'Pair 2 <span class="unit">(P3 & P4)</span>';
        } else {
            activePairDisplay.innerHTML = 'Pair 1 <span class="unit">(P1 & P2)</span>';
        }
    }

    if (activePairHint) {
        if (currentPair === 2) {
            activePairHint.textContent = "Hold both ESP32 buttons 3s to switch to Pair 1 (P1/P2)";
        } else {
            activePairHint.textContent = "Hold both ESP32 buttons 3s to switch to Pair 2 (P3/P4)";
        }
    }

    // Update hardware binding hints on cards
    const bind1 = document.getElementById("bind1");
    const bind2 = document.getElementById("bind2");
    const bind3 = document.getElementById("bind3");
    const bind4 = document.getElementById("bind4");

    if (bind1) bind1.style.color = (currentPair === 1) ? "#38bdf8" : "#94a3b8";
    if (bind2) bind2.style.color = (currentPair === 1) ? "#38bdf8" : "#94a3b8";
    if (bind3) bind3.style.color = (currentPair === 2) ? "#38bdf8" : "#94a3b8";
    if (bind4) bind4.style.color = (currentPair === 2) ? "#38bdf8" : "#94a3b8";
}

// =====================================================
// UPDATE HARDWARE TELEMETRY STATUS
// =====================================================

function updateHardwareTelemetry(hardware) {
    const espStatus    = document.getElementById("espStatus");
    const oledStatus   = document.getElementById("oledStatus");
    const buzzerStatus = document.getElementById("buzzerStatus");
    const syncIndicator= document.getElementById("syncStatusIndicator");
    const syncText     = document.getElementById("syncText");

    const isConnected = !!(hardware && hardware.esp32);

    if (espStatus) {
        espStatus.textContent = isConnected ? "Connected (Live)" : "Disconnected";
        espStatus.className = `hw-status ${isConnected ? 'online' : ''}`;
    }

    if (oledStatus) {
        oledStatus.textContent = isConnected ? "Ready (0x3C I2C)" : "Offline";
        oledStatus.className = `hw-status ${isConnected ? 'online' : ''}`;
    }

    if (buzzerStatus) {
        buzzerStatus.textContent = isConnected ? "Ready (Pin 13)" : "Offline";
        buzzerStatus.className = `hw-status ${isConnected ? 'online' : ''}`;
    }

    if (syncIndicator && syncText) {
        syncIndicator.classList.remove("sync-error");
        syncText.textContent = isConnected ? "ESP32 Live" : "Server Ready";
    }
}

// =====================================================
// FETCH LIVE STATE FROM NODE.JS SERVER
// =====================================================

async function fetchServerState() {
    try {
        const response = await fetch("/api/state");
        if (!response.ok) {
            throw new Error(`HTTP ${response.status}`);
        }

        const data = await response.json();
        if (!data || !data.success) return;

        // Update 4 people's water values
        if (data.state) {
            peopleData[0].water = data.state.person1 || 0;
            peopleData[1].water = data.state.person2 || 0;
            peopleData[2].water = data.state.person3 || 0;
            peopleData[3].water = data.state.person4 || 0;

            updatePairUI(data.state.pair);
        }

        // Render each card
        peopleData.forEach(person => updatePersonUI(person));

        // Update Hardware Status
        updateHardwareTelemetry(data.hardware);

        // Update Date Display
        updateDateDisplay(data.date);

    } catch (error) {
        const syncIndicator = document.getElementById("syncStatusIndicator");
        const syncText      = document.getElementById("syncText");

        if (syncIndicator) syncIndicator.classList.add("sync-error");
        if (syncText) syncText.textContent = "Server Offline";

        updateHardwareTelemetry({ esp32: false, oled: false, buzzer: false });
    }
}

// =====================================================
// INITIALIZATION
// =====================================================

function init() {
    updateDateDisplay();
    peopleData.forEach(person => updatePersonUI(person));
    updatePairUI(1);

    // Initial server fetch
    fetchServerState();

    // Auto-refresh every 1000ms (1 second) to maintain tight live synchronization
    setInterval(fetchServerState, 1000);
}

// Run when DOM is ready
if (document.readyState === "loading") {
    document.addEventListener("DOMContentLoaded", init);
} else {
    init();
}