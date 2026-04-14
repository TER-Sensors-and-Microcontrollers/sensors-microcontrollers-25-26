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
    getIdsByMode

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
    updateTable

    Updates real-time table with all incoming real-time data
    input(s): 
        - mode (str): determines what specific sensors to include in table view
*/  
async function updateTable(mode) {
    try {
        const table = document.getElementById("tableContainer").querySelector("table");
        const tbody_old = table.querySelector("tbody");
        
        const ids = await getIdsByMode(mode);

        // console.log(ids);
        if (!tbody_old || tbody_old.rows.length != ids.length) {
               
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

            if (tbody_old) {
                table.replaceChild(tbody_new, tbody_old);
            } else {
                table.appendChild(tbody_new);
            }
           }    
        

        for (const row of tbody_old.rows) {
            const cells = row.cells;

            const response_data = await fetch('/get_dp/' + cells[0].textContent);
            if (!response_data.ok) {
                throw new Error(`HTTP error! status: ${response_data.status}`);
            }
            const reading = await response_data.json();

            if (reading.error) {
                continue;
            }

            cells[2].textContent = reading.data;
        }
    }
    catch (error) {
        console.error("Error fetching or parsing data:", error);
    }
}

const WHEEL_RADIUS_M = 20.5 / 2; 
const GEAR_RATIO = 3.8;

async function updateSpeed(data) {

    const motorRPM = data;
    const wheelRPM = motorRPM / GEAR_RATIO;
    const speedMS = wheelRPM * 2 * Math.PI * WHEEL_RADIUS_M / 60;
    const speedMPH = speedMS * 2.237;

    document.getElementById('speed-display').textContent = speedMPH.toFixed(1) + " mph";
}



// setInterval(() => {
//     updateTable("f");
//     updateSpeed();
// }, 1000);


socket.on('unique_sens', (unique) => {
    try {
        const table_qs = document.getElementById("tableContainer").querySelector("table");
        const tbody_old = table_qs.querySelector("tbody");
        
        const ids = unique

        // console.log(ids);
        if (!tbody_old || tbody_old.rows.length != ids.length) {
               
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

            if (tbody_old) {
                table_qs.replaceChild(tbody_new, tbody_old);
            } else {
                table_qs.appendChild(tbody_new);
            }
           }   

    }
    catch (error) {
        console.error("Error fetching or parsing data:", error);
    }
});

socket.on('new_datapoint', (reading) => {
    const table_qs = document.getElementById("tableContainer").querySelector("table");
    const tbody_old = table_qs.querySelector("tbody");
    for (const row of tbody_old.rows) {
        const cells = row.cells;
        if (reading.sensor_id == cells[0].textContent) cells[2].textContent = reading.data;
    }

    // update speed if dp is motor speed (ID 1651)
    if (reading.sensor_id == 1651) updateSpeed(reading.data);
});


// /*
//     table.js — real-time table (optimized)
// */

// const container = document.getElementById('tableContainer');
// let table = null;
// let tbody = null;
// let sensorRows = {}; // sensor_id → <tr>

// async function getIdsByMode(mode) {
//     if (mode === "f") {
//         const res = await fetch('/unique_sensors');
//         if (!res.ok) throw new Error("Failed to get unique sensors");
//         return await res.json();
//     }
//     throw new Error(`Unsupported mode '${mode}'`);
// }

// // Build the table one time
// async function buildTable(mode) {
//     const ids = await getIdsByMode(mode);

//     table = document.createElement('table');

//     const thead = document.createElement('thead');
//     const headerRow = document.createElement('tr');
//     ["ID", "Name", "Value", "Unit"].forEach(text => {
//         const th = document.createElement('th');
//         th.textContent = text;
//         headerRow.appendChild(th);
//     });
//     thead.appendChild(headerRow);

//     tbody = document.createElement('tbody');

//     table.appendChild(thead);
//     table.appendChild(tbody);
//     container.appendChild(table);

//     // create empty rows
//     ids.forEach(sensor => {
//         const tr = document.createElement('tr');

//         const tdId   = document.createElement('td');
//         const tdName = document.createElement('td');
//         const tdVal  = document.createElement('td');
//         const tdUnit = document.createElement('td');

//         tdId.textContent   = sensor.sensor_id;
//         tdName.textContent = sensor.name;
//         tdUnit.textContent = sensor.unit;

//         tr.appendChild(tdId);
//         tr.appendChild(tdName);
//         tr.appendChild(tdVal);
//         tr.appendChild(tdUnit);

//         tbody.appendChild(tr);
//         sensorRows[sensor.sensor_id] = tr;
//     });
// }

// async function updateTableValues() {
//     // fetch all dp in parallel
//     const promises = Object.keys(sensorRows).map(async sensorId => {
//         const res = await fetch(`/get_dp/${sensorId}`);
//         if (!res.ok) return;

//         const reading = await res.json();
//         if (reading.error) return;

//         // Update only the value cell
//         const tr = sensorRows[sensorId];
//         tr.cells[2].textContent = reading.data;
//     });

//     await Promise.all(promises);
// }

// // Initialize once
// (async function() {
//     await buildTable("f");
//     setInterval(updateTableValues, 1000);
// })();