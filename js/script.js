/* =====================================================
   WATER TRACKER
   ===================================================== */


/* =====================================================
   CONFIGURATION

   Change these values if the requirements change.
   ===================================================== */

const CONFIG = {

    // Daily target for EACH person
    DAILY_GLASS_GOAL: 8,

    // Current assumed glass size
    GLASS_SIZE_ML: 250,

    // Number of people in the household
    NUMBER_OF_PEOPLE: 4,

    // Storage keys
    DATA_KEY: "waterTrackerData",

    DATE_KEY: "waterTrackerDate"

};


/* =====================================================
   PEOPLE

   Each person has an independent counter.
   ===================================================== */

let people = [

    {
        id: 1,
        name: "Person 1",
        glasses: 0
    },

    {
        id: 2,
        name: "Person 2",
        glasses: 0
    },

    {
        id: 3,
        name: "Person 3",
        glasses: 0
    },

    {
        id: 4,
        name: "Person 4",
        glasses: 0
    }

];


/* =====================================================
   GET TODAY'S DATE
   ===================================================== */

function getToday() {

    return new Date().toDateString();

}


/* =====================================================
   FORMAT DATE
   ===================================================== */

function displayDate() {

    const date = new Date();

    const formattedDate =
        date.toLocaleDateString(
            "en-IN",
            {
                weekday: "long",
                day: "numeric",
                month: "short",
                year: "numeric"
            }
        );

    document.getElementById("currentDate")
        .textContent = formattedDate;

}


/* =====================================================
   SAVE DATA
   ===================================================== */

function saveData() {

    localStorage.setItem(
        CONFIG.DATA_KEY,
        JSON.stringify(people)
    );

    localStorage.setItem(
        CONFIG.DATE_KEY,
        getToday()
    );

}


/* =====================================================
   RESET PEOPLE
   ===================================================== */

function resetPeople() {

    people = people.map(person => {

        return {

            ...person,

            glasses: 0

        };

    });

    saveData();

}


/* =====================================================
   LOAD DATA
   ===================================================== */

function loadData() {

    const savedData =
        localStorage.getItem(CONFIG.DATA_KEY);

    const savedDate =
        localStorage.getItem(CONFIG.DATE_KEY);


    /*
       If there is no saved date,
       this is the first time the app is opened.
    */

    if (!savedDate) {

        saveData();

        return;

    }


    /*
       If the saved date is different from today,
       automatically start a new day.
    */

    if (savedDate !== getToday()) {

        resetPeople();

        return;

    }


    /*
       Load today's saved counters.
    */

    if (savedData) {

        try {

            const parsedData =
                JSON.parse(savedData);


            /*
               Make sure the saved data
               contains the expected people.
            */

            if (
                Array.isArray(parsedData) &&
                parsedData.length === CONFIG.NUMBER_OF_PEOPLE
            ) {

                people = parsedData;

            }

        }

        catch (error) {

            console.error(
                "Could not load saved data.",
                error
            );

        }

    }

}


/* =====================================================
   CALCULATE WATER
   ===================================================== */

function calculateWater(glasses) {

    return glasses * CONFIG.GLASS_SIZE_ML;

}


/* =====================================================
   FORMAT WATER
   ===================================================== */

function formatWater(amountML) {

    if (amountML >= 1000) {

        const litres =
            amountML / 1000;

        return litres + " L";

    }

    return amountML + " ml";

}


/* =====================================================
   CALCULATE PROGRESS
   ===================================================== */

function calculateProgress(glasses) {

    return (
        glasses /
        CONFIG.DAILY_GLASS_GOAL
    ) * 100;

}


/* =====================================================
   CREATE PERSON CARD
   ===================================================== */

function createPersonCard(person) {

    const card =
        document.createElement("article");


    card.className =
        "person-card";


    card.innerHTML = `

        <div class="person-header">

            <div class="avatar">
                👤
            </div>

            <div>

                <h3 class="person-name">
                    ${person.name}
                </h3>

                <p class="person-subtitle">
                    Daily Tracker
                </p>

            </div>

        </div>


        <div class="progress-container">

            <div
                class="progress-circle"
                id="circle-${person.id}"
            >

                <div class="circle-content">

                    <span
                        class="glass-count"
                        id="glasses-${person.id}"
                    >
                        0
                    </span>

                    <span class="glass-total">
                        / ${CONFIG.DAILY_GLASS_GOAL}
                    </span>

                </div>

            </div>

        </div>


        <div class="water-info">

            <div
                class="water-amount"
                id="water-${person.id}"
            >
                0 ml
            </div>

            <p
                class="progress-text"
                id="percent-${person.id}"
            >
                0% completed
            </p>

        </div>


        <div class="progress-bar">

            <div
                class="progress-fill"
                id="bar-${person.id}"
            >
            </div>

        </div>


        <div class="buttons">

            <button
                class="drink-btn"
                id="drink-${person.id}"
                onclick="drinkWater(${person.id})"
            >
                + 1 Glass
            </button>


            <button
                class="undo-btn"
                id="undo-${person.id}"
                onclick="undoWater(${person.id})"
            >
                ↩ Undo
            </button>

        </div>


        <p
            class="status"
            id="status-${person.id}"
        >
            Keep drinking water 💧
        </p>

    `;


    return card;

}


/* =====================================================
   RENDER PEOPLE
   ===================================================== */

function renderPeople() {

    const grid =
        document.getElementById("peopleGrid");


    grid.innerHTML = "";


    people.forEach(person => {

        const card =
            createPersonCard(person);


        grid.appendChild(card);

    });


    updateAllUI();

}


/* =====================================================
   UPDATE ONE PERSON
   ===================================================== */

function updatePersonUI(person) {

    const glasses =
        person.glasses;


    const water =
        calculateWater(glasses);


    const percentage =
        calculateProgress(glasses);


    /*
       Prevent percentage from exceeding 100.
    */

    const safePercentage =
        Math.min(percentage, 100);


    /*
       Update counter.
    */

    document.getElementById(
        `glasses-${person.id}`
    ).textContent = glasses;


    /*
       Update water amount.
    */

    document.getElementById(
        `water-${person.id}`
    ).textContent =
        formatWater(water);


    /*
       Update percentage.
    */

    document.getElementById(
        `percent-${person.id}`
    ).textContent =
        safePercentage + "% completed";


    /*
       Update progress bar.
    */

    document.getElementById(
        `bar-${person.id}`
    ).style.width =
        safePercentage + "%";


    /*
       Update circular progress.
    */

    const degrees =
        (safePercentage / 100) * 360;


    document.getElementById(
        `circle-${person.id}`
    ).style.background =
        `conic-gradient(
            #2196f3 ${degrees}deg,
            #e5f2f9 ${degrees}deg
        )`;


    /*
       Get buttons and status.
    */

    const drinkButton =
        document.getElementById(
            `drink-${person.id}`
        );


    const undoButton =
        document.getElementById(
            `undo-${person.id}`
        );


    const status =
        document.getElementById(
            `status-${person.id}`
        );


    /*
       Daily goal completed.
    */

    if (
        glasses >=
        CONFIG.DAILY_GLASS_GOAL
    ) {

        status.textContent =
            "🎉 Daily goal completed,Congratulations!-!!";


        status.classList.add(
            "completed"
        );


        /*
           Prevent more than 8 glasses.
        */

        drinkButton.disabled = true;

    }

    else {

        status.textContent =
            "Keep drinking water 💧";


        status.classList.remove(
            "completed"
        );


        drinkButton.disabled = false;

    }


    /*
       Disable Undo when counter is zero.
    */

    undoButton.disabled =
        glasses <= 0;

}


/* =====================================================
   UPDATE EVERY PERSON
   ===================================================== */

function updateAllUI() {

    people.forEach(person => {

        updatePersonUI(person);

    });

}


/* =====================================================
   DRINK WATER
   ===================================================== */

function drinkWater(personId) {

    const person =
        people.find(
            person =>
                person.id === personId
        );


    if (!person) {

        return;

    }


    /*
       Do not allow more than the goal.
    */

    if (
        person.glasses >=
        CONFIG.DAILY_GLASS_GOAL
    ) {

        return;

    }


    /*
       Add one glass.
    */

    person.glasses++;


    /*
       Save immediately.
    */

    saveData();


    /*
       Update only this person's card.
    */

    updatePersonUI(person);

}


/* =====================================================
   UNDO WATER
   ===================================================== */

function undoWater(personId) {

    const person =
        people.find(
            person =>
                person.id === personId
        );


    if (!person) {

        return;

    }


    /*
       Cannot go below zero.
    */

    if (person.glasses <= 0) {

        return;

    }


    /*
       Remove one glass.
    */

    person.glasses--;


    /*
       Save immediately.
    */

    saveData();


    /*
       Update UI.
    */

    updatePersonUI(person);

}


/* =====================================================
   UPDATE GLASS SIZE DISPLAY
   ===================================================== */

function updateGlassSizeDisplay() {

    document.getElementById(
        "glassSizeText"
    ).textContent =
        CONFIG.GLASS_SIZE_ML + " ml";

}


/* =====================================================
   INITIALIZE APPLICATION
   ===================================================== */

function initializeApp() {

    displayDate();

    updateGlassSizeDisplay();

    loadData();

    renderPeople();

}


/* =====================================================
   START
   ===================================================== */

initializeApp();