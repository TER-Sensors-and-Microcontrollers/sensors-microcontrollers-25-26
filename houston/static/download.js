/* 
    download.js
    
    Support downloading of .db file
*/

function downloadDb() {
  const link = document.createElement('a');
  link.href = "/database.db";
  link.download = "database.db"; // Optional: provide a default name for the file
  document.body.appendChild(link);
  link.click();
  document.body.removeChild(link); // Clean up the DOM
}