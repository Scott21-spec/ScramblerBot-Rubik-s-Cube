const modes = {
    resolver: { color: 'var(--neon-resolver)', text: 'RESOLVER' },
    mezclar:  { color: 'var(--neon-mezclar)', text: 'MEZCLAR' },
    ajustes:  { color: 'var(--neon-ajustes)', text: 'AJUSTES' },
    about:    { color: 'var(--neon-about)', text: 'ABOUT US' }
};

function setMode(modeId) {
    document.querySelectorAll('.nav-btn').forEach(btn => btn.classList.remove('active'));
    document.getElementById(`btn-${modeId}`).classList.add('active');

    document.documentElement.style.setProperty('--active-neon', modes[modeId].color);
    
    const titleText = document.getElementById('current-mode-text');
    titleText.innerText = modes[modeId].text;
    titleText.style.color = modes[modeId].color;
    titleText.style.textShadow = `0 0 8px ${modes[modeId].color}`;

    document.querySelectorAll('.view').forEach(view => view.classList.remove('active'));
    document.getElementById(`view-${modeId}`).classList.add('active');

    const historialBtn = document.getElementById('btn-historial');
    if (modeId === 'ajustes' || modeId === 'about') {
        historialBtn.style.visibility = 'hidden';
    } else {
        historialBtn.style.visibility = 'visible';
    }

    logTerminal(`Cambiando a modo ${modes[modeId].text}...`);
}

function logTerminal(message) {
    const now = new Date();
    const timeString = `${now.getDate()}/${now.getMonth()+1}/${now.getFullYear()} ${now.getHours()}:${now.getMinutes()}`;
    document.getElementById('terminal-output').innerText = `${timeString} - [STATUS]: ${message}`;
}

// --- LOGICA DE ROTACIÓN DEL CUBO (DRAG CON MOUSE) ---
let isDragging = false;
let previousMousePosition = { x: 0, y: 0 };
// Estado de rotación inicial
let cubeRotations = { x: -20, y: -45 };

document.addEventListener('mousedown', (e) => {
    // Solo inicia el drag si hacemos click dentro de la escena del cubo
    if(e.target.closest('.scene')) {
        isDragging = true;
        previousMousePosition = { x: e.clientX, y: e.clientY };
    }
});

document.addEventListener('mousemove', (e) => {
    if(isDragging) {
        const deltaMove = {
            x: e.clientX - previousMousePosition.x,
            y: e.clientY - previousMousePosition.y
        };

        // Modifica la rotación actual (ajusta los multiplicadores si gira muy rápido/lento)
        cubeRotations.y += deltaMove.x * 0.6;
        cubeRotations.x -= deltaMove.y * 0.6; 

        // Aplica la rotación al cubo visible
        document.querySelectorAll('.cube').forEach(cube => {
            cube.style.transform = `rotateX(${cubeRotations.x}deg) rotateY(${cubeRotations.y}deg)`;
        });

        previousMousePosition = { x: e.clientX, y: e.clientY };
    }
});

document.addEventListener('mouseup', () => { isDragging = false; });
// Evitar bugs si el mouse sale de la ventana
document.addEventListener('mouseleave', () => { isDragging = false; });

// --- FUNCIONES BOTONES ---
function startProcess(mode) {
    logTerminal(`Iniciando proceso en modo ${mode}. Enviando comando a ESP32...`);
    if (mode === 'resolver') {
        document.getElementById('moves-list-resolver').innerText = "Calculando solución...";
    } 
    else if (mode === 'mezclar') {
        document.getElementById('scramble-setup-box').classList.add('shrunk');
        document.getElementById('cube-viewer-mezclar').classList.remove('hidden');
    }
}
function stopProcess() { logTerminal(`Proceso detenido de emergencia por el usuario.`); }
function downloadWCA() { logTerminal("Abriendo explorador para seleccionar PDF WCA..."); }
function generateWCA() {
    logTerminal("Generando secuencia WCA aleatoria...");
    document.getElementById('moves-list-mezclar').innerText = "R U R' U' D F2 L B"; 
}
function sendManualMove(move) { logTerminal(`Comando manual enviado: ${move}`); }

// --- LÓGICA DEL CUBO DESPLEGADO 2D ---
function toggleUnfold() {
    const modal = document.getElementById('unfold-modal');
    // Alterna la clase 'hidden' para mostrar/ocultar
    modal.classList.toggle('hidden');
}

function viewPDF() {
    logTerminal("Abriendo vista del documento PDF...");
}

setMode('resolver');