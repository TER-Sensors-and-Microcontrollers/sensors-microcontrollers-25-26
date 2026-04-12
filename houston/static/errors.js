async function getErrors() {
    const response = await fetch('/faults');
    if (!response.ok) return;
    const errors = await response.json();
    if (errors.error) return;

    document.getElementById('errors').textContent = '';
    for (let i = 0; i < errors.length; i++) {
        document.getElementById('errors').textContent += (errors[i].error + '\n');
    }
}

setInterval(() => {
    getErrors();
}, 1000);