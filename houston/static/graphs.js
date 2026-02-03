/*
    graph.js
    
    real-time graph logic
*/

// initial starting values for the 3 graphs, subject to change
var selectedValue1 = 1;
var selectedValue2 = 2;
var selectedValue3 = 3;

// get absolute start time from browser cache if it exists
var start = sessionStorage.getItem("startTime");
if (!start) {
    // Deserialize the JSON string back into a JavaScript object
    start = Date.now();
    sessionStorage.setItem("startTime", JSON.stringify(start));
}

/*
    updateGraphs

    Adds the most recently grabbed datapoints from 3 preexisting sensors,
    and adds it to its corresponding chart.
    input(s): size-3 array of sensor ids
    notes: 
        - saves chart to cache based on CANVAS ID (see chartjs documentation),
        - is async
        - sensor(s) with given id must exist within database
*/
async function updateGraphs([sid1,sid2,sid3])
{
    // also TODO:
    // add button to exchange sensors in a specific graph
    // add button to view a specific sensor in table mode (and associated page)
    
    try{
        const response1 = await fetch('/get_test/' + sid1);
        if (!response1.ok) {
        throw new Error(`HTTP error! status: ${response1.status}`);
        }
        const response2 = await fetch('/get_test/' + sid2);
        if (!response2.ok) {
        throw new Error(`HTTP error! status: ${response2.status}`);
        }
        const response3 = await fetch('/get_test/' + sid3);
        if (!response3.ok) {
        throw new Error(`HTTP error! status: ${response3.status}`);
        }
        const reading1 = await response1.json();
        const reading2 = await response2.json();
        const reading3 = await response3.json();
        
        // update graphs
        // we only divide start by 1000 since timestamp is already given to us in seconds
        g1.data.labels.push(reading1.timestamp - (start / 1000));
        g1.data.datasets[0].data.push(reading1.data);
        g2.data.labels.push(reading2.timestamp - (start / 1000));
        g2.data.datasets[0].data.push(reading2.data);
        g3.data.labels.push(reading3.timestamp - (start / 1000));
        g3.data.datasets[0].data.push(reading3.data);
        g1.options.title.text = reading1.name + " Over Time";
        g2.options.title.text = reading2.name + " Over Time";
        g3.options.title.text = reading3.name + " Over Time";
        g1.update();
        g2.update();
        g3.update();
        // save chart to client's cache so that it can be reloaded on refresh
        saveToSessionStorage(g1.canvas.id, {
            labels: g1.data.labels,
            datasets: g1.data.datasets.map(ds => ({
                label: reading1.name + " Over Time",
                backgroundColor: ds.backgroundColor,
                borderColor: ds.borderColor,
                data: ds.data
            })),
            options: {
                title: {
                    display: true,
                    text: reading1.name + " Over Time"
            }
    }})
        saveToSessionStorage(g2.canvas.id, {
            labels: g2.data.labels,
            datasets: g2.data.datasets.map(ds => ({
                label: reading2.name + " Over Time",
                backgroundColor: ds.backgroundColor,
                borderColor: ds.borderColor,
                data: ds.data
            })),
                options: {
        title: {
            display: true,
            text: reading1.name + " Over Time"
        }
    }
        })
        saveToSessionStorage(g3.canvas.id, {
            labels: g3.data.labels,
            datasets: g3.data.datasets.map(ds => ({
                label: reading3.name + " Over Time",
                backgroundColor: ds.backgroundColor,
                borderColor: ds.borderColor,
                data: ds.data
            })),
                options: {
        title: {
            display: true,
            text: reading1.name + " Over Time"
        }
    }
        })
    }
    catch (error) {
        console.error("Error fetching or parsing data:", error);
    }
}
async function clearGraph(g) {
    g.data.labels = [];
    g.data.datasets[0].data = [];
    g.update();
    saveToSessionStorage(g.canvas.id,
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
    saveToSessionStorage

    Given chart name and cachable chart data, saves chart data in user's browser cache
    input(s): name/key to save chart data field as, chart data field to be cached
*/
function saveToSessionStorage(name, data) {
    // sessionStorage can only store strings, so we must serialize the data to JSON
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
        }
    }
});

const g2 = new Chart("graph2", {
    type: "line",
    data: loadFromSessionStorage("graph2"),
    options: {
        title: {
            display: true,
            text: 'Sensor 2 Over Time'
        }
    }
});
const g3 = new Chart("graph3", {                
    type: "line",
    data: loadFromSessionStorage("graph3"),
    options: {
        title: {
            display: true,
            text: 'Sensor 3 Over Time'
        }
    }
});

 document.addEventListener('DOMContentLoaded', function() {
    var dropdown = document.getElementById('g1');
                dropdown.addEventListener('change', function() 
                {
                    clearGraph(g1);
                    selectedValue = dropdown.value;
                    get_new_data(selectedValue, g1);
                });
 });

 document.addEventListener('DOMContentLoaded', function() {
    var dropdown = document.getElementById('g2');
                dropdown.addEventListener('change', function() 
                {
                    clearGraph(g2);
                    selectedValue = dropdown.value;
                    get_new_data(selectedValue, g2);
                });
 });

 document.addEventListener('DOMContentLoaded', function() {
    var dropdown = document.getElementById('g3');
                dropdown.addEventListener('change', function() 
                {
                    clearGraph(g3);
                    selectedValue = dropdown.value;
                    get_new_data(selectedValue, g3);
                });
 });

//  get all up-to-date data of selected id
 async function get_new_data(selectedValue, g)
 {
    const new_data = await fetch('/get_all_data/' + selectedValue);
        if (!new_data.ok) {
        throw new Error(`HTTP error! status: ${new_data.status}`);
        }
    
    const all_data = await new_data.json();

    for (let r = 0; r < all_data.length; r++) {
        g.data.labels.push((all_data[r].timestamp - (start / 1000)));
        g.data.datasets[0].data.push(all_data[r].data);
    }
    g.options.title.text = all_data[0].name + " Over Time";
    g.update();
    
    saveToSessionStorage(g1.canvas.id, {
            labels: g.data.labels,
            datasets: g.data.datasets.map(ds => ({
                label: all_data[0].name + " Over Time",
                backgroundColor: ds.backgroundColor,
                borderColor: ds.borderColor,
                data: ds.data
            })),
            options: {
                title: {
                    display: true,
                    text: all_data[0].name + " Over Time"
                }
            }
        })


 }
setInterval(() => {
    updateGraphs([selectedValue1, selectedValue2, selectedValue3]);
}, 3000);
