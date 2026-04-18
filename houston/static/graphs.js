/*
    graph.js
    
    real-time graph logic
*/

// initial starting values for the 3 graphs, subject to change
var selectedValue1 = 1651;
var selectedValue2 = 1670;
var selectedValue3 = 1720;

var id_name_mappings = {} // store locally so we don't need call from backend
var currentX = "";
var currentY = "";
var scatter_pt = {x: null, y: null};

var MAX_POINTS = 200;

// get absolute start time from browser cache if it exists
var start = sessionStorage.getItem("startTime");
if (!start) {
    // Deserialize the JSON string back into a JavaScript object
    start = Date.now();
    sessionStorage.setItem("startTime", JSON.stringify(start));
}


/*
    updateGraph

    Adds the most recently grabbed datapoint from a preexisting sensor,
    and adds it to its corresponding chart.
    input(s): sensor id, variable name of graph
    notes: 
        - saves chart to cache based on CANVAS ID (see chartjs documentation),
        - is async
        - sensor(s) with given id must exist within database
*/

async function updateGraph(sid, g)
{
    
    try{
        const response = await fetch('/get_dp/' + sid);
        if (!response.ok) {
        throw new Error(`HTTP error! status: ${response.status}`);
        }

        const reading = await response.json();
        

        if (reading.error) {
            console.log("Error: No data found");
            return;
        }

        // update graphs
        // we only divide start by 1000 since timestamp is already given to us in seconds
        g.data.labels.push(reading.timestamp);
        g.data.datasets[0].data.push(reading.data);

        g.options.title.text = reading.name + " (" + reading.unit + ") Over Time"
        g.update({ duration: 0, lazy: true });

        // save chart to client's cache so that it can be reloaded on refresh
        maybeSaveToSessionStorage(g.canvas.id, {
            labels: g.data.labels,
            datasets: g.data.datasets.map(ds => ({
                backgroundColor: ds.backgroundColor,
                borderColor: ds.borderColor,
                data: ds.data
            })),
            options: {
                title: {
                    display: true,
                    // text: reading1.name + " Over Time"
                },
                animation: false
            }
        })
        maybeSaveToSessionStorage(scatter.canvas.id, {
            labels: scatter.data.labels,
            datasets: scatter.data.datasets.map(ds => ({
                label: reading1.name + " vs " + reading2.name,
                backgroundColor: ds.backgroundColor,
                borderColor: ds.borderColor,
                data: ds.data
            })),
            options: {
                title: {
                    display: true,
                    text: reading1.name + " vs " + reading2.name
            },
            animation: false
    }})
    }
    catch (error) {
        console.error("Error fetching or parsing data:", error);
    }
}

async function clearGraph(g) {
    g.data.labels = [];
    g.data.datasets[0].data = [];
    g.update({ duration: 0, lazy: true });
    maybeSaveToSessionStorage(g.canvas.id,
    {
        labels: [],
        datasets: [{
            label: "",
            backgroundColor:"rgba(255,255,255,1.0)",
            borderColor: "rgba(0,0,255,0.1)",
            data: []
            }]
    });
    console.log("graph " + g + " cleared...");
}
/*
    maybeSaveToSessionStorage

    Given chart name and cachable chart data, saves chart data in user's browser cache
    approximately every 5 seconds.
    input(s): name/key to save chart data field as, chart data field to be cached
*/

let lastSave = 0;
const SAVE_INTERVAL = 5000; // save at most every 5 seconds

function maybeSaveToSessionStorage(name, data) {
    const now = Date.now();
    if (now - lastSave < SAVE_INTERVAL) return;
    lastSave = now;
    console.log("Saving graph " + name + " to session storage.");
    sessionStorage.setItem(name, JSON.stringify(data));
}
/*
    loadFromSessionStorage

    Given name of chart to load, return chart data from user's browser cache, 
    if there exists any. Else, return a fresh instance of chart data
    input(s): name under which chart data was saved as (in saveToSessionStorage)
*/
function loadFromSessionStorage(name) {
    // filler values to put into fresh chart
    const xValues = [];
    const yValues = [];
    const storedData = sessionStorage.getItem(name);
    if (storedData) {
        // Deserialize the JSON string back into a JavaScript object
        return JSON.parse(storedData);
    }
    // Return a default data structure if nothing is saved yet
    return {
        labels: xValues,
        datasets: [{
            label: name,
            backgroundColor:"rgba(255,255,255,1.0)",
            borderColor: "rgba(0,0,255,0.1)",
            data: yValues
            }]
    };
}

// define chart(s) here
const g1 = new Chart("graph1", {
    type: "line",
    data: loadFromSessionStorage("graph1"),
    options: {
        title: {
            display: true,
            text: 'Sensor 1 Over Time'
        },
        animation: false
    },
    decimation: {
        enabled: true,
        algorithm: 'min-max',
        samples: 200  // max points rendered at once
    }
});

const g2 = new Chart("graph2", {
    type: "line",
    data: loadFromSessionStorage("graph2"),
    options: {
        title: {
            display: true,
            text: 'Sensor 2 Over Time',
            
        },
        animation: false,
        showLine: false // disable for all datasets
    },
    decimation: {
        enabled: true,
        algorithm: 'min-max',
        samples: 200  // max points rendered at once
    }
});
const g3 = new Chart("graph3", {                
    type: "line",
    data: loadFromSessionStorage("graph3"),
    options: {
        title: {
            display: true,
            text: 'Sensor 3 Over Time'
        },
        animation: false
    },
    decimation: {
        enabled: true,
        algorithm: 'min-max',
        samples: 200  // max points rendered at once
    }
});

const scatter = new Chart("scatter", {
    type: "scatter",
    data: {
        datasets: [{
            label: "Sensor 1 vs Sensor 2",
            data: [],
            backgroundColor: "rgba(17, 0, 255, 0.7)"
        }]
    },
    options: {
        title: {
            display: true,
            text: 'Sensor 1 vs Sensor 2'
        },
        scales: {
            xAxes: {
                title: {
                    display: true,
                    text: "Sensor 1"
                }
            },
            yAxes: {
                title: {
                    display: true,
                    text: "Sensor 2"
                }
            }
        },
        animation: false
    }
});

// ================================================
function trimDataPoints(g) {
    // Trim oldest points if over limit
    if (g.data.labels.length > MAX_POINTS) {
        const excess = g.data.labels.length - MAX_POINTS;
        g.data.labels.splice(0, excess);
        g.data.datasets[0].data.splice(0, excess);
    }
}

 document.addEventListener('DOMContentLoaded', function() {
    var dropdown = document.getElementById('g1');
                dropdown.addEventListener('change', function() 
                {
                    clearGraph(g1);
                    selectedValue1 = dropdown.value;
                    get_new_data(selectedValue1, g1);
                });
 });

 document.addEventListener('DOMContentLoaded', function() {
    var dropdown = document.getElementById('g2');
                dropdown.addEventListener('change', function() 
                {
                    clearGraph(g2);
                    selectedValue2 = dropdown.value;
                    get_new_data(selectedValue2, g2);
                });
 });

 document.addEventListener('DOMContentLoaded', function() {
    var dropdown = document.getElementById('g3');
                dropdown.addEventListener('change', function() 
                {
                    clearGraph(g3);
                    selectedValue3 = dropdown.value;
                    get_new_data(selectedValue3, g3);
                });
 });

document.addEventListener('DOMContentLoaded', function() {
    var dropdown = document.getElementById('scatterX');
                dropdown.addEventListener('change', function() 
                {
                    clearGraph(scatter);
                    selectedValue = dropdown.value;
                    currentX = id_name_mappings[selectedValue]
                    get_new_data(selectedValue, scatter);
                });
 });

 document.addEventListener('DOMContentLoaded', function() {
    var dropdown = document.getElementById('scatterY');
                dropdown.addEventListener('change', function() 
                {
                    clearGraph(scatter);
                    selectedValue = dropdown.value;
                    currentY = id_name_mappings[selectedValue]
                    get_new_data(selectedValue, scatter);
                });
 });

//  get all up-to-date data of selected id, updates graph g
 async function get_new_data(selectedValue, g)
 {
    const new_data = await fetch('/get_all_data/' + selectedValue);
        if (!new_data.ok) {
        throw new Error(`HTTP error! status: ${new_data.status}`);
        }
    
    const all_data = await new_data.json();

    for (let r = 0; r < all_data.length; r++) {
        g.data.labels.push(all_data[r].timestamp );
        g.data.datasets[0].data.push(all_data[r].data);
    }
    if (g === scatter) {
        g.options.title.text = currentY + " vs " + currentX;
    }
    else
        g.options.title.text = all_data[0].name + " Over Time";

    trimDataPoints(g);

    g.update({ duration: 0, lazy: true });
    
    maybeSaveToSessionStorage(g.canvas.id, {
            labels: g.data.labels,
            datasets: g.data.datasets.map(ds => ({
                backgroundColor: ds.backgroundColor,
                borderColor: ds.borderColor,
                data: ds.data
            })),
            options: {
                title: {
                    display: true,
                    text: all_data[0].name + " (" + all_data[0].unit + ") Over Time",
                }
            },
            animation: false
        })


 }

// buffers to push to graphs in bulk
const buffers = { g1: [], g2: [], g3: [], s: []};

// on new dp, push only selected graph values to buffers, and scatterplot if relevant
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

setInterval(() => {
    flushBuffer(buffers.g1, g1);
    flushBuffer(buffers.g2, g2);
    flushBuffer(buffers.g3, g3);
}, 33);

// Updates graph with all data in its buffer.
// Clears out excess data points
function flushBuffer(buffer, g) {
    if (buffer.length === 0) return;
    buffer.forEach(reading => {
        g.data.labels.push(reading.timestamp);
        g.data.datasets[0].data.push(reading.data);
    });
    buffer.length = 0; // clear in place (faster than reassigning)

    trimDataPoints(g);

    g.update({ duration: 0, lazy: true }); // skip animation


    // save chart to client's cache so that it can be reloaded on refresh
    maybeSaveToSessionStorage(g.canvas.id, {
        labels: g.data.labels,
        datasets: g.data.datasets.map(ds => ({
            backgroundColor: ds.backgroundColor,
            borderColor: ds.borderColor,
            data: ds.data
        })),
        options: {
            title: {
                display: true,
                // text: reading1.name + " Over Time"
            },
            animation: false
        }
    })
} 