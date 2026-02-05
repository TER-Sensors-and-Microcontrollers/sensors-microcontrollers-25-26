/*
    table.js
    
    real-time data logic
*/
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

            const response_data = await fetch('/get_test/' + cells[0].textContent);
            if (!response_data.ok) {
                throw new Error(`HTTP error! status: ${response_data.status}`);
            }
            const reading = await response_data.json();

            cells[2].textContent = reading.data;
        }
    }
    catch (error) {
        console.error("Error fetching or parsing data:", error);
    }
}

setInterval(() => {
    updateTable("f");
}, 1000);