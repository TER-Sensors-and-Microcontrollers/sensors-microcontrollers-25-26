/*
    buttons.js
    
    logic for popups + buttons
*/
        
        
document.addEventListener('DOMContentLoaded', function() {
  const back = document.getElementById('back');
  const openSD = document.getElementById('openSD');
  const openGraph = document.getElementById('openGraph');
  const uploadDB = document.getElementById('upload');
  const fileInput = document.getElementById('file-input');
  const scatterOn = document.getElementById('scatterOn');
  const scatterOff = document.getElementById('scatterOff');

  if (back) {
    back.onclick = () => {
      window.location.href = "/";
    };
  }
  if (openSD) {
    openSD.onclick = () => {
      window.location.href = "/SD";
    };
  }
  if (openGraph) {
    openGraph.onclick = () => {
      window.location.href = "/max-graph";
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
