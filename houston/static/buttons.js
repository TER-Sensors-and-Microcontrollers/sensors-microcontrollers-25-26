/*
    buttons.js
    
    logic for popups + buttons
*/

document.addEventListener('DOMContentLoaded', function() {
  const openWeb = document.getElementById('openWeb');
  const uploadDB = document.getElementById('upload');
  const fileInput = document.getElementById('file-input');
  const scatterOn = document.getElementById('scatterOn');
  const scatterOff = document.getElementById('scatterOff');
  const message = document.getElementById('message');
  
  if (openWeb) {
    openWeb.onclick = () => {
      window.location.href = "/SD";
    };
  }

  if (uploadDB && fileInput) {
    uploadDB.addEventListener("click", () => {
      fileInput.click();
    });

    fileInput.addEventListener("change", async (e) => {
      fileName = e.target.files[0];
      length = fileName.name.length;
      if (fileName.name[length - 3] == '.' && fileName.name[length - 2] == 'd' && fileName.name[length - 1] == 'b') {
        console.log("Success!");
        message.textContent = "SUCCESS";
        message.style.color = "green";

        const formData = new FormData();
        formData.append('file', fileName);

        await fetch('/upload', {
          method: 'POST',
          body: formData
        });

        const arrayBuffer = await fileName.arrayBuffer();
        const uint8Array = new Uint8Array(arrayBuffer);
        
        const SQL = await initSqlJs ({
          locateFile: file => `https://cdnjs.cloudflare.com/ajax/libs/sql.js/1.10.2/${file}`
        })

        const db = new SQL.Database(uint8Array);

        const result = db.exec("SELECT * FROM sensor_readings WHERE sensor_id = 1 ORDER BY timestamp DESC LIMIT 1");
        console.log(result[0].values[0]);
      } else {
        message.textContent = "NOT A DB FILE";
        message.style.color = "red";
        console.log("Not a DB File");
      }
    });
  }

  if (scatterOn && scatterOff) {
    const scatterX = document.getElementById('scatterX');
    scatterOn.onclick = () => {
      scatterOff.classList.remove("hidden");
      scatterOn.classList.add("hidden");
      scatterX.classList.remove("hidden")
      document.getElementById("sc")?.classList.remove("hidden");
      document.getElementById("scatterChoice")?.classList.remove("hidden");
    };

    scatterOff.onclick = () => {
      scatterOff.classList.add("hidden");
      scatterOn.classList.remove("hidden");
      document.getElementById("sc")?.classList.add("hidden");
      document.getElementById("scatterChoice")?.classList.add("hidden");
    };
  }
});
