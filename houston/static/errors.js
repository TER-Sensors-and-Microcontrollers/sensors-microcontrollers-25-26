/*

Socket code for updating webapp with fault codes

*/


socket.on('mc_faults', (errors) => {
    const e = document.getElementById('errors');
    e.textContent = '';
    // console.log(e.textContent + " - " + errors);
    for (let i = 0; i < errors.length; i++) {
        // console.log(errors[i].error)
        e.textContent += (errors[i].error + '\n');
    }
});