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
            
        }
        catch (error) {
            console.error("Error fetching or parsing data:", error);
        }
}

setInterval(() => {
    updateDropdown("g1");
    updateDropdown("g2");
    updateDropdown("g3");
    updateDropdown("scatterX");
    updateDropdown("scatterY");
}, 1000);