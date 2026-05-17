// ======================================================
// SENSOR DATA
// ======================================================
let sensorData = {
    temperature: 25.0,
    humidity: 60.0,
    currentL: 0.0,
    currentF: 0.0,
    presence: false,
    light: false,
    fan: false,
    alert: false
};

// ======================================================
// API
// ======================================================
const API_URL = 'http://26.196.241.44:5000/api/data';
const CONTROL_URL = 'http://26.196.241.44:5000/api/control';

// ======================================================
// CHART
// ======================================================
let ctx = document.getElementById('envChart').getContext('2d');

Chart.defaults.color = '#94a3b8';
Chart.defaults.font.family = "'Outfit', sans-serif";

let envChart = new Chart(ctx, {

    type: 'line',

    data: {

        labels: [],

        datasets: [

            {
                label: 'Nhiệt độ (°C)',
                data: [],
                borderColor: '#ef4444',
                backgroundColor: 'rgba(239,68,68,0.1)',
                borderWidth: 2,
                tension: 0.4,
                yAxisID: 'y'
            },

            {
                label: 'Dòng điện Đèn (A)',
                data: [],
                borderColor: '#f59e0b',
                backgroundColor: 'rgba(245,158,11,0.1)',
                borderWidth: 2,
                tension: 0.4,
                yAxisID: 'y1'
            },

            {
                label: 'Dòng điện Quạt (A)',
                data: [],
                borderColor: '#10b981',
                backgroundColor: 'rgba(16,185,129,0.1)',
                borderWidth: 2,
                tension: 0.4,
                yAxisID: 'y1'
            }
        ]
    },

    options: {

        responsive: true,
        maintainAspectRatio: false,

        plugins: {
            legend: {
                position: 'top',
                align: 'end'
            }
        },

        scales: {

            x: {
                grid: {
                    color: 'rgba(255,255,255,0.05)'
                }
            },

            y: {
                type: 'linear',
                display: true,
                position: 'left',
                suggestedMin: 20,
                suggestedMax: 40,
                grid: {
                    color: 'rgba(255,255,255,0.05)'
                }
            },

            y1: {
                type: 'linear',
                display: true,
                position: 'right',
                suggestedMin: 0,
                suggestedMax: 5,
                grid: {
                    drawOnChartArea: false
                }
            }
        }
    }
});

// ======================================================
// INIT CHART
// ======================================================
function initChartData() {

    let now = new Date();

    for(let i=15;i>=0;i--){

        let time = new Date(now.getTime() - i * 2000);

        envChart.data.labels.push(
            time.toLocaleTimeString([],{
                hour12:false,
                minute:'2-digit',
                second:'2-digit'
            })
        );

        envChart.data.datasets[0].data.push(sensorData.temperature);
        envChart.data.datasets[1].data.push(sensorData.currentL);
        envChart.data.datasets[2].data.push(sensorData.currentF);
    }

    envChart.update();
}

// ======================================================
// UPDATE UI
// ======================================================
function updateUI(){

    // ================= SENSOR =================
    document.getElementById('val-temp').innerText =
    sensorData.temperature.toFixed(1);

    document.getElementById('val-humid').innerText =
    sensorData.humidity.toFixed(1);

    document.getElementById('val-current-l').innerText =
    sensorData.currentL.toFixed(2);

    document.getElementById('val-current-f').innerText =
    sensorData.currentF.toFixed(2);

    // ================= PROGRESS =================
    document.getElementById('bar-temp').style.width =
    `${Math.min(100,(sensorData.temperature/50)*100)}%`;

    document.getElementById('bar-humid').style.width =
    `${sensorData.humidity}%`;

    document.getElementById('bar-current-l').style.width =
    `${Math.min(100,(sensorData.currentL/5)*100)}%`;

    document.getElementById('bar-current-f').style.width =
    `${Math.min(100,(sensorData.currentF/5)*100)}%`;

    // ================= RADAR =================
    const radarCard =
    document.getElementById('radar-card');

    const presenceStatus =
    document.getElementById('presence-status');

    if(sensorData.presence){

        radarCard.className =
        'status-card presence-card active';

        presenceStatus.innerText =
        'Phát hiện có người';

    }else{

        radarCard.className =
        'status-card presence-card inactive';

        presenceStatus.innerText =
        'Không có người';
    }

    // ================= BUTTON =================
    document.getElementById('light-toggle').checked =
    sensorData.light;

    document.getElementById('fan-toggle').checked =
    sensorData.fan;
}

// ======================================================
// FETCH SENSOR
// ======================================================
async function fetchData(){

    try{

        const response = await fetch(API_URL);

        if(!response.ok){

            throw new Error("Network ERR");
        }

        const data = await response.json();

        // ================= SENSOR =================
        sensorData.temperature =
        data.temperature || 0;

        sensorData.humidity =
        data.humidity || 0;

        sensorData.currentL =
        data.currentL || 0;

        sensorData.currentF =
        data.currentF || 0;

        sensorData.presence =
        data.presence || false;

        // ================= BUTTON =================
        sensorData.light =
        data.light || false;

        sensorData.fan =
        data.fan || false;

        updateUI();

        // ================= CHART =================
        let now = new Date();

        envChart.data.labels.push(
            now.toLocaleTimeString([],{
                hour12:false,
                minute:'2-digit',
                second:'2-digit'
            })
        );

        envChart.data.datasets[0].data.push(sensorData.temperature);
        envChart.data.datasets[1].data.push(sensorData.currentL);
        envChart.data.datasets[2].data.push(sensorData.currentF);

        if(envChart.data.labels.length > 20){

            envChart.data.labels.shift();

            envChart.data.datasets[0].data.shift();
            envChart.data.datasets[1].data.shift();
            envChart.data.datasets[2].data.shift();
        }

        envChart.update('none');

    }catch(err){

        console.error(err);
    }
}

// ======================================================
// LIGHT BUTTON
// ======================================================
document.getElementById('light-toggle')
.addEventListener('change', async function(){

    try{

        await fetch(CONTROL_URL,{

            method:'POST',

            headers:{
                'Content-Type':'application/json'
            },

            body:JSON.stringify({

                light:this.checked
            })
        });

    }catch(err){

        console.error(err);
    }
});

// ======================================================
// FAN BUTTON
// ======================================================
document.getElementById('fan-toggle')
.addEventListener('change', async function(){

    try{

        await fetch(CONTROL_URL,{

            method:'POST',

            headers:{
                'Content-Type':'application/json'
            },

            body:JSON.stringify({

                fan:this.checked
            })
        });

    }catch(err){

        console.error(err);
    }
});

// ======================================================
// START
// ======================================================
document.addEventListener('DOMContentLoaded',()=>{

    initChartData();

    updateUI();

    fetchData();

    setInterval(fetchData,500);
});