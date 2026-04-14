/*
    graph-dropdowns.js
    
    real-time logic for dropdown graphs
*/

const pagePath = window.location.pathname;
const pageName = pagePath.split("/").pop() || "index"


/*
    updateDropdown

    Updates given dropdown id with given list of sensors to track
*/
async function updateDropdown(d, ids) {
    try {
            const dropdown = document.getElementById(d);

            if (dropdown !== null) {
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
 
            
        }
        catch (error) {
            console.error("Error fetching or parsing data:", error);
        }
}

socket.on('unique_sens', (unique) => {
// check file running this code
if (pageName === "index") {
    updateDropdown("g1", unique);
    updateDropdown("g1-1", unique);
    updateDropdown("g2", unique);
    updateDropdown("g3", unique);
    updateDropdown("scatterX", unique);
    updateDropdown("scatterY", unique);
}
else if (pageName === "max-graph") {
    updateDropdown("g1", unique);
    updateDropdown("g2", unique);
    updateDropdown("g3", unique);
}   
});


