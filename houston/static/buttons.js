/*
    buttons.js
    
    logic for popups + buttons
*/
        
        
document.addEventListener('DOMContentLoaded', function() {
    const modal = document.getElementById('sensorExchange');
    const openPopupBtn = document.getElementById('openPopup');
    const closeButton = document.querySelector('.close-button');
    const openWeb = document.getElementById('openWeb');
    
    openWeb.onclick = function() {
        window.location.href = "/SD";
        
    }
    scatterOn.onclick = function() {
        var element = document.getElementById("scatterOff");
        element.classList.remove("hidden");
        element = document.getElementById("scatterOn");
        element.classList.add("hidden");   

        var element2 = document.getElementById("sc");
        element2.classList.remove("hidden");
            

    }
    scatterOff.onclick = function() {
        var element = document.getElementById("scatterOff");
        element.classList.add("hidden");
        element = document.getElementById("scatterOn");
        element.classList.remove("hidden");    
        
        var element2 = document.getElementById("sc");
        element2.classList.add("hidden");
    }
});