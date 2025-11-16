const container = document.getElementById('tableContainer');
                const table = document.createElement('table');
                const thead = document.createElement('thead');
                const headerRow = document.createElement('tr');
                const headers = ['Name', 'Value'];
                headers.forEach(headerText => {
                    const th = document.createElement('th');
                    th.textContent = headerText;
                    headerRow.appendChild(th);
                });
                thead.appendChild(headerRow);
                table.appendChild(thead);

                const tbody = document.createElement('tbody');
                const data = [];
            
                data.forEach(rowData => {
                    const row = document.createElement('tr');
                    for (const key in rowData) {
                        const cell = document.createElement('td');
                        cell.textContent = rowData[key];
                        row.appendChild(cell);
                    }
                    tbody.appendChild(row);
                    });
                table.appendChild(tbody);
                container.appendChild(table);
                


                /* 
                    renderTable
                    
                    renders real-time data table with upated data via data array
                
                
                */
                function renderTable() {
                    // Get all rows in tbody
                    const rows = tbody.querySelectorAll('tr');

                    rows.forEach((row, rowIndex) => {
                        const cells = row.querySelectorAll('td');
                        cells[0].textContent = data[rowIndex].name;
                        cells[1].textContent = data[rowIndex].value;
                    });
                }
                
                
                /*
                    updateTable
                
                    Updates real-time table with all incoming real-time data

                    input(s): data array to be updated in entirety with incoming data
                */  
                async function updateTable(data)
                {

                    // make it flexible to however many unique incoming readings need to be read
                    try {


                        const response1 = await fetch('/get_test/1'); // Replace with your actual API endpoint
                        if (!response1.ok) {
                        throw new Error(`HTTP error! status: ${response1.status}`);
                        }
                        const response2 = await fetch('/get_test/36');
                        if (!response2.ok) {
                        throw new Error(`HTTP error! status: ${response2.status}`);
                        }
                        const response3 = await fetch('/get_test/37');
                        if (!response3.ok) {
                        throw new Error(`HTTP error! status: ${response3.status}`);
                        }
                        const reading1 = await response1.json();
                        const reading2 = await response2.json();
                        const reading3 = await response3.json();

                        data[0].name = reading1.name;
                        data[1].name = reading2.name;
                        data[2].name = reading3.name;
                        
                        data[0].value = reading1.data;
                        data[1].value = reading2.data;
                       
                        data[2].value = reading3.data;

                        renderTable();
                    }
                    catch (error) {
                        console.error("Error fetching or parsing data:", error);
                    }
                }

            setInterval(() => {
                updateTable(data);
            }, 3000);