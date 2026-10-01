// =====================================================
// WATER TRACKER - NODE.JS EXPRESS SERVER
// =====================================================
// Architecture:
//   Website <--> Node.js Server (Port 3000) <--> ESP32
//
// Endpoints:
//   GET  /api/state        - Returns live state of 4 people & hardware status
//   POST /api/state        - ESP32 updates its live state to the server
//   POST /api/command      - Website sends action (add / undo / reset)
//   GET  /api/command      - ESP32 polls for pending website commands
//   POST /api/command/ack  - ESP32 acknowledges processed command
//   POST /api/reset        - Resets all 4 people to 0 mL and clears history
// =====================================================

const express = require('express');
const path = require('path');

const app = express();
const PORT = process.env.PORT || 3000;

// Enable JSON parsing
app.use(express.json());

// Enable CORS for all routes (so local testing and remote calls both work smoothly)
app.use((req, res, next) => {
    res.header('Access-Control-Allow-Origin', '*');
    res.header('Access-Control-Allow-Methods', 'GET, POST, OPTIONS');
    res.header('Access-Control-Allow-Headers', 'Content-Type');
    if (req.method === 'OPTIONS') {
        return res.sendStatus(200);
    }
    next();
});

// Serve frontend static files from the repository root (index.html, css, js, assets)
app.use(express.static(path.join(__dirname, '..')));

// =====================================================
// STATE & CONFIGURATION - STARTS STRICTLY FROM ZERO
// =====================================================

const GOAL_ML = 3000;

// Initialized strictly to 0 mL for all 4 people
let waterState = {
    person1: 0,
    person2: 0,
    person3: 0,
    person4: 0,
    pair: 1 // Currently active pair on hardware: 1 = P1/P2, 2 = P3/P4
};

// Undo history stacks for each person
let historyState = {
    1: [],
    2: [],
    3: [],
    4: []
};

// Hardware communication tracking
let lastEspHeartbeat = 0;
const ESP_TIMEOUT_MS = 8000; // If no ping in 8s, mark ESP32 offline

// Command Queue for ESP32
let commandQueue = [];
let nextCommandId = 1;

// =====================================================
// MIDNIGHT RESET (ASIA/KOLKATA TIMEZONE)
// =====================================================

function getKolkataDateString() {
    try {
        return new Intl.DateTimeFormat('en-CA', {
            timeZone: 'Asia/Kolkata',
            year: 'numeric',
            month: '2-digit',
            day: '2-digit'
        }).format(new Date()); // Formats as YYYY-MM-DD
    } catch (e) {
        // Fallback offset UTC + 5:30
        const now = new Date();
        const istTime = new Date(now.getTime() + (5.5 * 60 * 60 * 1000));
        return istTime.toISOString().split('T')[0];
    }
}

let currentDate = getKolkataDateString();

function performReset() {
    waterState.person1 = 0;
    waterState.person2 = 0;
    waterState.person3 = 0;
    waterState.person4 = 0;
    historyState[1] = [];
    historyState[2] = [];
    historyState[3] = [];
    historyState[4] = [];
    commandQueue = [];
}

function checkDailyReset() {
    const today = getKolkataDateString();
    if (today !== currentDate) {
        console.log(`[DAILY RESET] Midnight reached in Asia/Kolkata. Resetting data for new day: ${today}`);
        performReset();
        currentDate = today;
    }
}

// Check every 10 seconds for midnight reset
setInterval(checkDailyReset, 10000);

// =====================================================
// API ROUTES
// =====================================================

// 1. GET /api/state - Current system state for Website
app.get('/api/state', (req, res) => {
    checkDailyReset();

    const isEspOnline = (Date.now() - lastEspHeartbeat) < ESP_TIMEOUT_MS;

    res.json({
        success: true,
        state: {
            person1: waterState.person1,
            person2: waterState.person2,
            person3: waterState.person3,
            person4: waterState.person4,
            pair: waterState.pair
        },
        people: [
            { id: 1, name: "Person 1", water: waterState.person1 },
            { id: 2, name: "Person 2", water: waterState.person2 },
            { id: 3, name: "Person 3", water: waterState.person3 },
            { id: 4, name: "Person 4", water: waterState.person4 }
        ],
        hardware: {
            online: isEspOnline,
            esp32: isEspOnline,
            oled: isEspOnline,
            buzzer: isEspOnline,
            lastHeartbeat: lastEspHeartbeat
        },
        date: currentDate,
        goalML: GOAL_ML
    });
});

// 2. POST /api/state - ESP32 periodically sends state
app.post('/api/state', (req, res) => {
    checkDailyReset();

    const body = req.body || {};
    if (typeof body.person1 === 'number') waterState.person1 = Math.max(0, body.person1);
    if (typeof body.person2 === 'number') waterState.person2 = Math.max(0, body.person2);
    if (typeof body.person3 === 'number') waterState.person3 = Math.max(0, body.person3);
    if (typeof body.person4 === 'number') waterState.person4 = Math.max(0, body.person4);
    if (typeof body.pair === 'number') waterState.pair = body.pair;

    lastEspHeartbeat = Date.now();

    res.json({
        success: true,
        message: "State updated successfully"
    });
});

// 3. POST /api/command - Website sends action (+250 mL or Undo)
app.post('/api/command', (req, res) => {
    checkDailyReset();

    const { action, person, amount } = req.body;
    const personNum = parseInt(person, 10);

    if (!personNum || personNum < 1 || personNum > 4) {
        return res.status(400).json({ success: false, message: "Invalid person number (1-4)" });
    }

    if (action !== 'add' && action !== 'undo') {
        return res.status(400).json({ success: false, message: "Invalid action. Use 'add' or 'undo'" });
    }

    const addAmount = amount ? parseInt(amount, 10) : 250;

    const command = {
        id: nextCommandId++,
        action: action,
        person: personNum,
        amount: addAmount,
        timestamp: Date.now()
    };

    // Queue command for ESP32 to execute on hardware
    commandQueue.push(command);

    // If ESP32 is offline, update server state directly so website remains fully functional
    const isEspOnline = (Date.now() - lastEspHeartbeat) < ESP_TIMEOUT_MS;
    if (!isEspOnline) {
        const key = `person${personNum}`;
        if (action === 'add') {
            waterState[key] = (waterState[key] || 0) + addAmount;
            historyState[personNum].push(addAmount);
        } else if (action === 'undo') {
            const lastAddition = historyState[personNum].pop();
            if (lastAddition) {
                waterState[key] = Math.max(0, (waterState[key] || 0) - lastAddition);
            }
        }
    }

    console.log(`[COMMAND] #${command.id}: ${action} for Person ${personNum} (amount: ${addAmount} mL) [ESP32 online: ${isEspOnline}]`);

    res.json({
        success: true,
        command: command
    });
});

// 4. POST /api/reset - Resets all 4 people to 0 mL
app.post('/api/reset', (req, res) => {
    performReset();
    // Enqueue reset for ESP32
    commandQueue.push({
        id: nextCommandId++,
        action: 'reset',
        person: 0,
        amount: 0,
        timestamp: Date.now()
    });
    console.log("[RESET] All 4 people reset to 0 mL");
    res.json({
        success: true,
        message: "All 4 people reset to 0 mL"
    });
});

// 5. GET /api/command - ESP32 polls for pending command
app.get('/api/command', (req, res) => {
    lastEspHeartbeat = Date.now();

    if (commandQueue.length > 0) {
        const cmd = commandQueue[0]; // Peek at oldest command
        return res.json({
            id: cmd.id,
            action: cmd.action,
            person: cmd.person,
            amount: cmd.amount
        });
    }

    // No pending command
    res.json({ id: 0 });
});

// 6. POST /api/command/ack - ESP32 confirms execution of command
app.post('/api/command/ack', (req, res) => {
    lastEspHeartbeat = Date.now();
    const ackId = req.body && parseInt(req.body.id, 10);

    if (ackId) {
        commandQueue = commandQueue.filter(cmd => cmd.id !== ackId);
        console.log(`[COMMAND ACK] Command #${ackId} acknowledged by ESP32`);
    }

    res.json({ success: true });
});

// =====================================================
// SERVER START
// =====================================================

app.listen(PORT, '0.0.0.0', () => {
    console.log("====================================================");
    console.log(`💧 Water Tracker Server running on port ${PORT}`);
    console.log(`📡 Local Website URL:   http://localhost:${PORT}`);
    console.log(`🌐 Network Website URL: http://10.107.183.37:${PORT}`);
    console.log(`🕒 Timezone: Asia/Kolkata (Current: ${currentDate})`);
    console.log("====================================================");
});