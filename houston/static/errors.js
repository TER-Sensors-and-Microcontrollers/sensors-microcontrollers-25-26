/*

Socket code for updating webapp with fault codes

*/


socket.on('mc_faults', (errors) => {
    const e = document.getElementById('errors');
    e.textContent = '';
    // console.log(e.textContent + " - " + errors);
    for (let i = 0; i < errors.length; i++) {
        if (errors[i].type == "BMS") {
            e.textContent += ("BMS: " + errors[i].error + '\n');
        }
        else {
            e.textContent += ("MC: (" + errors[i].type + "): "  + errors[i].error + '\n');
        }
        // console.log(errors[i].error)
    }
});