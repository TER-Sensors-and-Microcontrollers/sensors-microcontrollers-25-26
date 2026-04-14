/*
    graph-dropdowns.js
    
    real-time logic for dropdown graphs
*/

const pagePath = window.location.pathname;
const pageName = pagePath.split("/").pop() || "index"

socket.on('new_datapoint', (reading) => {
    
    id_name_mappings[reading.sensor_id] = reading.name;

    const now = new Date();
    // console.log(now + now.getMilliseconds() + " - message recieved: ID:" + reading.sensor_id + " Data:" + reading.data);
    if (reading.sensor_id == selectedValue1) buffers.g1.push(reading);
    if (reading.sensor_id == selectedValue2) buffers.g2.push(reading);
    if (reading.sensor_id == selectedValue3) buffers.g3.push(reading);
    
    // console.log("currentx: " + currentX + " currenty: " + currentY)
    // Update scatterplot
    if (reading.name == currentX) scatter_pt.x = reading.data;
    if (reading.name == currentY) scatter_pt.y = reading.data;
    if (scatter_pt.x != null && scatter_pt.y != null) {
        scatter.data.datasets[0].data.push({x: scatter_pt.x, y: scatter_pt.y});
        scatter_pt.x = null;
        scatter_pt.y = null;
        scatter.update({ duration: 0, lazy: true });

        if (scatter.data.datasets[0].data.length > MAX_POINTS) {
        const excess = scatter.data.datasets[0].data.length - MAX_POINTS;
        scatter.data.datasets[0].data.splice(0, excess);
    }
    }
});


async function updateDropdown(d) {
    try {
            const dropdown = document.getElementById(d);
            const ids = await getIdsByMode("f");
            // console.log(ids);
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
    updateDropdown("g2");
}, 1000);    
}
