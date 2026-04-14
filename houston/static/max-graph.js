/*
    max-graph.js
    
    re-used graph code for max graph page

*/
var selectedValue1 = undefined;
var selectedValue2 = undefined;
var selectedValue3 = undefined;

var start = sessionStorage.getItem("startTime");
if (!start) {
    // Deserialize the JSON string back into a JavaScript object
    start = Date.now();
    sessionStorage.setItem("startTime", JSON.stringify(start));
}

// define chart(s) here
const g1 = new Chart("graph1", {
    type: "line",
    data: {
        labels: [],
        datasets: [{
            label: "Reading",
            backgroundColor:"rgba(255,255,255,1.0)",
            borderColor: "rgba(8, 8, 151, 0.83)",
            data: [],
            hidden: false
            },
            {
            label: "Reading2",
            backgroundColor:"rgba(255,255,255,1.0)",
            borderColor: "rgba(124, 11, 11, 0.44)",
            data: [],
            hidden: false
            },
            {
            label: "Reading3",
            backgroundColor:"rgba(255,255,255,1.0)",
            borderColor: "rgba(13, 141, 62, 0.65)",
            data: [],
            hidden: false
            },        ]
        
    },
    options: {
        title: {
            display: true,
            text: 'Sensor 1 Over Time'
        },
        animation: false
    },
});

document.addEventListener('DOMContentLoaded', function() {
    var dropdown = document.getElementById('g1');
                dropdown.addEventListener('change', function() 
                {
                    clearGraph(g1);
                    selectedValue1 = dropdown.value;
                    get_new_data(selectedValue1, g1, selectedValue2, selectedValue3);
                });
    var dropdown2 = document.getElementById('g2');
                dropdown2.addEventListener('change', function() 
                {
                    clearGraph(g1);
                    selectedValue2 = dropdown2.value;
                    if (selectedValue2 === "none") selectedValue2 = undefined;
                    get_new_data(selectedValue1, g1, selectedValue2, selectedValue3);
                });
    var dropdown3 = document.getElementById('g3');
                dropdown3.addEventListener('change', function() 
                {
                    clearGraph(g1);
                    selectedValue3 = dropdown3.value;
                    if (selectedValue3 === "none") selectedValue3 = undefined;
                    get_new_data(selectedValue1, g1, selectedValue2, selectedValue3);
                });

});

// update value of first line, and optionally the second and third lines
async function get_new_data(selectedValue, g, selectedValue2, selectedValue3) {
    const new_data = await fetch('/get_all_data/' + selectedValue);
        if (!new_data.ok) {
        throw new Error(`HTTP error! status: ${new_data.status}`);
        }
    
    const all_data = await new_data.json();
    
    // update first line independently
    g.options.title.text = all_data[0].name + " Over Time";
    for (let r = 0; r < all_data.length; r++) {
        g.data.labels.push(all_data[r].timestamp);
        g.data.datasets[0].data.push(all_data[r].data);
    }
    g.data.datasets[0].label = all_data[0].name;

    if (selectedValue2 !== undefined) {
        const new_data = await fetch('/get_all_data/' + selectedValue2);
            if (!new_data.ok) {
            throw new Error(`HTTP error! status: ${new_data.status}`);
            }
        
        const all_data_g2 = await new_data.json();
        for (let r = 0; r < all_data_g2.length; r++) {
            g.data.labels.push(all_data_g2[r].timestamp);
            g.data.datasets[1].data.push(all_data_g2[r].data);
        }  
        g.options.title.text = all_data[0].name + " and " + all_data_g2[0].name + " Over Time";
        g.data.datasets[1].label = all_data_g2[0].name;
    }

    if (selectedValue3 !== undefined) {
        const new_data = await fetch('/get_all_data/' + selectedValue3);
            if (!new_data.ok) {
            throw new Error(`HTTP error! status: ${new_data.status}`);
            }
        
        const all_data_g3 = await new_data.json();
        for (let r = 0; r < all_data_g3.length; r++) {
            g.data.labels.push(all_data_g3[r].timestamp);
            g.data.datasets[2].data.push(all_data_g3[r].data);
        }   

        if (selectedValue2 === undefined) g.options.title.text = all_data[0].name + ", " + all_data_g3[0].name + ", and " + all_data_g3[0].name + " Over Time";
        else g.options.title.text = all_data[0].name + " and " + all_data_g3[0].name + " Over Time";

        g.data.datasets[1].label = all_data_g3[0].name;
    }

    g.update({ duration: 0, lazy: true });
}

async function clearGraph(g) {
    g.data.labels = [];
    g.data.datasets[0].data = [];
    g.data.datasets[1].data = [];
    g.data.datasets[2].data = [];
    g.update({ duration: 0, lazy: true });
}