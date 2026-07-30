// script2.js replaces formerly 4 .js files and is used in various places outside of the main player.html

// ===== IR Recorder functionality for ir.html =====

var irloaded=false;
function checkSelect(){
  var elements = document.getElementsByClassName("irradio");
  var chkid = 0;
  for (var i = 0; i < elements.length; i++) {
      elements[i].classList.remove("active");
      elements[i].parentElement.getElementsByClassName("irrecordvalue")[0].classList.remove("active");
      if(elements[i]===this) chkid=i;
  }
  var ts = this!==window?this:elements[0];
  ts.classList.add("active");
  ts.parentElement.getElementsByClassName("irrecordvalue")[0].classList.add("active");
  if(this!==window) websocket.send('chkid='+chkid);
  document.getElementById('protocol').innerText="";
}

function irbuttonClick(){
  var elements = document.getElementsByClassName("irbutton");
  var hasactive = this.classList.contains("active");
  var btnid = -1;
  for (var i = 0; i < elements.length; i++) {
    elements[i].classList.remove("active");
    if(!hasactive && elements[i]==this) btnid=i;
  }
  if(!hasactive) {
    document.getElementById("irrecordtitle").innerHTML = t('msg_codes_for_button', 'Codes for button') + ' <span>'+this.innerHTML+'</span>';
    document.getElementById("irrecord").classList.remove("hidden");
    document.getElementById("irstartrecord").classList.add("hidden");
    this.classList.add("active");
    checkSelect();
  }else{
    document.getElementById("irrecord").classList.add("hidden");
    document.getElementById("irstartrecord").classList.remove("hidden");
  }
  document.getElementById('protocol').innerText="";
  websocket.send('irbtn='+btnid);
}
function backRecord(){
  var elements = document.getElementsByClassName("irbutton");
  for (var i = 0; i < elements.length; i++) {
    elements[i].classList.remove("active");
  }
  document.getElementById("irrecord").classList.add("hidden");
  document.getElementById("irstartrecord").classList.remove("hidden");
  websocket.send('irbtn=-1');
}
function irClear(el){
  el.parentElement.getElementsByClassName("irrecordvalue")[0].innerText="";
  document.getElementById('protocol').innerText="";
  websocket.send('irclr='+el.parentElement.getElementsByClassName("irradio")[0].getAttribute('data-id'));
}
function initControls(){
  if(irloaded) return;
  irloaded=true;
  var elements = document.getElementsByClassName("irbutton");
  for (var i = 0; i < elements.length; i++) {
      elements[i].addEventListener('click', irbuttonClick, false);
  }
  elements = document.getElementsByClassName("irradio");
  for (var i = 0; i < elements.length; i++) {
      elements[i].addEventListener('click', checkSelect, false);
  }
}

// ===== Online update checker functionality for update.html =====

function initOnlineUpdateChecker() {
  if (onlineUpdCapable) {
    getId('check_online_update').classList.remove('hidden');
    getId('check_online_update').value = t('btn_check_online', 'Check for Online Update');
    getId('check_online_update').disabled = false;
    console.log("Online Update is available");
  } else {
    getId('check_online_update').classList.add('hidden');
    console.log("Online Update not available");
  }
}

function checkOnlineUpdate(button) {
  if (button.value === t('btn_check_online', 'Check for Online Update')) {
    console.log("Checking for online update");
    button.value = t('lbl_checking', 'Checking...');
    button.disabled = true;
    fetch('/onlineupdatecheck')
      .then(response => response.text())
      .then(data => {
        console.log("Check update response:", data);
        // The server will send WebSocket messages with the actual results so just wait
      })
      .catch(error => {
        console.error("Error checking for updates:", error);
        button.value = t('btn_check_online', 'Check for Online Update');
        button.disabled = false;
      });
  } else if (button.value.startsWith("Update to")) {
    console.log("Starting online update via HTTP");
    // Show and reset progress bar
    const bar = getId('updateprogress');
    if (bar) { bar.hidden = false; bar.value = 0; }

    button.disabled = true;
    fetch('/onlineupdatestart')
      .then(response => response.text())
      .then(data => {
        console.log("Start update response:", data);
        // Prepare UI: hide form, show status and progress bar
        const status = getId('uploadstatus');
        
        if(status) status.innerHTML = getId('check_online_update').value;

        getId("uploadstatus").innerHTML = t('lbl_starting_online_update', 'Starting online update...');
        getId('updateform').classList.add('hidden');
        getId("updateprogress").value = 0;
        getId('updateprogress').hidden=false;
        getId('update_cancel_button').hidden=true;
        getId('check_online_update').classList.add('hidden');
        // WebSocket messages will drive progress
      })
      .catch(error => {
        console.error("Error starting update:", error);
        button.value = t('btn_check_online', 'Check for Online Update');
        button.disabled = false;
        getId("uploadstatus").innerHTML = t('lbl_error_starting_online_update', 'Error starting online update');
        getId('updateform').classList.remove('hidden');
        getId('updateprogress').hidden=true;
        getId("updateprogress").value = 0;
        getId('update_cancel_button').hidden=false;
        getId('check_online_update').classList.remove('hidden');
      });
  }
}

// ===== Shared station preview/play functionality for search and playlist editor =====

function sendStationAction(name, url, addtoplaylist) {
  if (!name || !url) {
    console.error('Invalid station data:', { name, url });
    return;
  }
  
  const label = addtoplaylist ? "Added to playlist: " : "Preview: ";
  const formData = new URLSearchParams();
  formData.append('name', name);
  formData.append('url', url);
  formData.append('addtoplaylist', addtoplaylist);
  
  fetch('/search', {
    method: 'POST',
    headers: { 'Content-Type': 'application/x-www-form-urlencoded' },
    body: formData
  })
  .then(response => {
    if (!response.ok) throw new Error('Action failed');
    return response.text();
  })
  .then(responseText => {
    console.log(label + name, 'Response:', responseText);
  })
  .catch(error => console.error('Error sending station action:', error));
}
