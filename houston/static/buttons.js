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

  if (openWeb) {
    openWeb.onclick = () => {
      window.location.href = "/SD";
    };
  }

  if (uploadDB && fileInput) {
    uploadDB.addEventListener("click", () => {
      fileInput.click();
    });

    fileInput.addEventListener("change", (e) => {
      fileName = e.target.files[0];
      length = fileName.name.length;
      if (fileName.name[length - 3] == '.' && fileName.name[length - 2] == 'd' && fileName.name[length - 1] == 'b') {
        console.log("Success!");
      } else {
        console.log("Not a DB File");
      }
    });
  }

  if (scatterOn && scatterOff) {
    scatterOn.onclick = () => {
      scatterOff.classList.remove("hidden");
      scatterOn.classList.add("hidden");
      document.getElementById("sc")?.classList.remove("hidden");
    };

    scatterOff.onclick = () => {
      scatterOff.classList.add("hidden");
      scatterOn.classList.remove("hidden");
      document.getElementById("sc")?.classList.add("hidden");
    };
  }
});
