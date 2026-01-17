/*
    graph-dropdowns.js
    
    real-time logic for dropdown graphs
*/

async function updateDropdown(d) {
    try {
            const dropdown = document.getElementById(d);
        
            const ids = await getIdsByMode("f");

            // console.log(ids);
            if (dropdown.length < ids.length) {
                
                // reset table
                
                
                ids.forEach(async id => {
                    const option = document.createElement("option");

                    option.text       = id.name;
                    option.value      = id.sensor_id;
                    
                    dropdown.appendChild(option);
                });

            }    
            

            // for (const row of tbody_old.rows) {
            //     const cells = row.cells;

            //     const response_data = await fetch('/get_test/' + cells[0].textContent);
            //     if (!response_data.ok) {
            //         throw new Error(`HTTP error! status: ${response_data.status}`);
            //     }
            //     const reading = await response_data.json();

            //     cells[2].textContent = reading.data;
            // }
        }
        catch (error) {
            console.error("Error fetching or parsing data:", error);
        }
}

setInterval(() => {
    updateDropdown("g1");
    updateDropdown("g2");
    updateDropdown("g3");
}, 1000);