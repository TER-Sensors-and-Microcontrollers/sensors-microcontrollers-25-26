# Documentation for Houston

https://www.w3schools.com/sql/sql_syntax.asp    

**Tech Stack (Official)**
- Frontend: raw HTML/CSS (not final)
- Backend: Flask (Python)
- DB: SQLite
- Hardware: Raspberry Pi 4
- Communication with car: RFD900, STM32 (STM32 script not in this repo)

**File & Folder Overview**
- *app.py*: flask app script that hosts the webapp, initializes the SQLite
  database, and (tentatively) defines several relevant routes.
- templates: directory that stores the HTML files for our webpages
    - *index.html*: the "main" webpage for houston. Displays critical sensor
      information.
- static: directory that stores style sheets (CSS) to decorate HTML pages
    - *style.css*: see above
- test: directory that stores test files to use throughout development of
  houston
    - *database-feeder.py*: feeds sample data into the database
- *requirements.txt*: Python/pip dependency file that names libraries for Docker
  to install
- *Dockerfile*: Docker compilation file for houston

> NOTE: the app will attempt to run on port 5000 by default (mapped from
> 5000:5000) on docker as well. If your device is actively using port 5000, you
> will need to remap the port on your computer to the port on Docker (the first
> number in xxxx:xxxx)

**Links for Knowledge**
- Routes in Flask: https://www.geeksforgeeks.org/python/flask-app-routing/    
    - ie. "/", "/readings"
- SQL basic syntax: https://www.w3schools.com/sql/sql_syntax.asp


**Notable Projects**
- RFD900 serial reader / timestamper
- SQLite db caching + retrieval
- Flask App
- sqlite file reader (Flask App)

