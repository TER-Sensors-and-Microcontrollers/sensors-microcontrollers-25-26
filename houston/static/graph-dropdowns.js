/*
    graph-dropdowns.js
    
    real-time logic for dropdown graphs
*/

const pagePath = window.location.pathname;
const pageName = pagePath.split("/").pop() || "index"


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

// check file running this code
if (pageName === "index") {
    setInterval(() => {
    updateDropdown("g1");
    updateDropdown("g1-1");
    updateDropdown("g2");
    updateDropdown("g3");
    updateDropdown("scatterX");
    updateDropdown("scatterY");
}, 1000);
}
else if (pageName === "max-graph") {
    setInterval(() => {
    updateDropdown("g1");
}, 1000);    
}
