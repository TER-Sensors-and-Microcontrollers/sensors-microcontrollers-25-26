/*
    table.js
    
    real-time data logic
*/
// const socket = io();
const container = document.getElementById('tableContainer');
const table = document.createElement('table');
const thead = document.createElement('thead');
const headerRow = document.createElement('tr');
const headers = ['ID', 'Name', 'Value', "Unit"];




headers.forEach(headerText => {
    const th = document.createElement('th');
    th.textContent = headerText;
    headerRow.appendChild(th);
});

thead.appendChild(headerRow);
table.appendChild(thead);

const tbody = document.createElement('tbody');

table.appendChild(tbody);
container.appendChild(table);

/*
    getIdsByMode [deprecated]

    Calls the appropriate endpoint to grab the appropriate collection of unique sensors
    
    input(s): 
        - mode (str): determines what specific sensors to include in table view
            - f: "full mode"; all unique sensors in the database
    returns: list of unique sensor name,id pairs

*/
async function getIdsByMode(mode) {
    if (mode == "f") {
        const response_ids = await fetch('/unique_sensors'); 
        if (!response_ids.ok) {
            throw new Error(`HTTP error! status: ${response_ids.status}`);
        }

        const ids = await response_ids.json();
        return ids;
    }
    // TODO: custom modes for different sensor views :)
    else {
        throw new Error(`Error: unsupported table mode.`);
    }
}


/*
    updateTableState

    Updates length of real-time table given sensors to track,
    table body to update, table object.
    
    Formats table as |ID|NAME|DATA[currently blank]|UNIT|
*/

function updateTableState(sens, tbody, table_qs) {
    try {
        const ids = sens;

        // console.log(ids);
        if (!tbody || tbody.rows.length != ids.length) {
               
            // reset table
            const tbody_new = document.createElement('tbody');
            
            ids.forEach(async id => {
                const row      = document.createElement('tr');
                const cellID   = document.createElement('td');
                const cellName = document.createElement('td');
                const cellData = document.createElement('td');
                const cellUnit = document.createElement('td');

                cellID.textContent   = id.sensor_id;
                cellName.textContent = id.name;
                cellUnit.textContent = id.unit;

                row.appendChild(cellID);
                row.appendChild(cellName);
                row.appendChild(cellData);
                row.appendChild(cellUnit);
                   
                tbody_new.appendChild(row);
               });

            if (tbody) {
                table_qs.replaceChild(tbody_new, tbody);
            } else {
                table_qs.appendChild(tbody_new);
            }
           }   

    }
    catch (error) {
        console.error("Error fetching or parsing data:", error);
    }
}

/*
    populateTable

    Populates tbody data section with reading if that
    sensor reading is tracked by the table.
    
    Assumes first col is ID and third col is data
*/
function populateTable(reading, tbody) {
    for (const row of tbody.rows) {
        const cells = row.cells;
        if (reading.sensor_id == cells[0].textContent) cells[2].textContent = reading.data;
    }
}



const WHEEL_RADIUS_M = 20.5 / 2; 
const GEAR_RATIO = 3.8;
/*
    updateSpeed

    given motor speed data, updates real-time speed display with
    calculated speed
*/
async function updateSpeed(data) {

    const motorRPM = data;
    const wheelRPM = motorRPM / GEAR_RATIO;
    const speedMS = wheelRPM * 2 * Math.PI * WHEEL_RADIUS_M / 60;
    const speedMPH = speedMS * 2.237;

    document.getElementById('speed-display').textContent = speedMPH.toFixed(1) + " mph";
}


socket.on('unique_sens', (unique) => {
    const table_qs = document.getElementById("tableContainer").querySelector("table");
    const tbody_old = table_qs.querySelector("tbody");

    updateTableState(unique, tbody_old, table_qs);
});

socket.on('new_datapoint', (reading) => {
    const table_qs = document.getElementById("tableContainer").querySelector("table");
    const tbody_old = table_qs.querySelector("tbody");

    populateTable(reading, tbody_old);
    // update speed if dp is motor speed (ID 1651)
    if (reading.sensor_id == 1651) updateSpeed(reading.data);
});
