 document.addEventListener('DOMContentLoaded', function() {
            const modal = document.getElementById('sensorExchange');
            const openPopupBtn = document.getElementById('openPopup');
            const closeButton = document.querySelector('.close-button');

            openPopupBtn.onclick = function() {
                modal.style.display = 'block';
                openPopupBtn.style.display = 'none';
            }

            closeButton.onclick = function() {
                modal.style.display = 'none';
                openPopupBtn.style.display = 'block';
            }

            window.onclick = function(event) {
                if (event.target == modal) {
                    modal.style.display = 'none';
                }
            }
        });