const modes = {
    resolver: { color: 'var(--neon-resolver)', text: 'RESOLVER' },
    mezclar:  { color: 'var(--neon-mezclar)', text: 'MEZCLAR' },
    ajustes:  { color: 'var(--neon-ajustes)', text: 'AJUSTES' }
};

const COLOR_MAP = {
    'Blanco': '#FFFFFF', 'w': '#FFFFFF', 'U': '#FFFFFF',
    'Naranja': '#FF7700', 'o': '#FF7700', 'L': '#FF7700',
    'Verde': '#00FF66',  'g': '#00FF66', 'F': '#00FF66',
    'Rojo': '#FF2244',   'r': '#FF2244', 'R': '#FF2244',
    'Azul': '#0088FF',   'b': '#0088FF', 'B': '#0088FF',
    'Amarillo': '#FFE600', 'y': '#FFE600', 'D': '#FFE600'
};

// Agrega esto justo debajo de tu COLOR_MAP
const ESTADO_RESUELTO = {
    'U': Array(9).fill('w'),
    'L': Array(9).fill('o'),
    'F': Array(9).fill('g'),
    'R': Array(9).fill('r'),
    'B': Array(9).fill('b'),
    'D': Array(9).fill('y')
};

// Mapeo matemático de los 54 stickers a las 27 piezas 3D reales del motor
const faceMapping = {
    'U': [ [-1,-1,-1], [0,-1,-1], [1,-1,-1], [-1,-1,0], [0,-1,0], [1,-1,0], [-1,-1,1], [0,-1,1], [1,-1,1] ],
    'F': [ [-1,-1,1], [0,-1,1], [1,-1,1], [-1,0,1], [0,0,1], [1,0,1], [-1,1,1], [0,1,1], [1,1,1] ],
    'R': [ [1,-1,1], [1,-1,0], [1,-1,-1], [1,0,1], [1,0,0], [1,0,-1], [1,1,1], [1,1,0], [1,1,-1] ],
    'B': [ [1,-1,-1], [0,-1,-1], [-1,-1,-1], [1,0,-1], [0,0,-1], [-1,0,-1], [1,1,-1], [0,1,-1], [-1,1,-1] ],
    'L': [ [-1,-1,-1], [-1,-1,0], [-1,-1,1], [-1,0,-1], [-1,0,0], [-1,0,1], [-1,1,-1], [-1,1,0], [-1,1,1] ],
    'D': [ [-1,1,1], [0,1,1], [1,1,1], [-1,1,0], [0,1,0], [1,1,0], [-1,1,-1], [0,1,-1], [1,1,-1] ]
};

let isDragging = false;
let isConnected = false;
let isEnded = false;
let isMoving = false;
let isUnfolded = false;
let isCubeScan = false;
let previousMousePosition = { x: 0, y: 0 };
let cubeRotations = { x: -20, y: -45 };
let ws;
let currentMezcla = "";
let movimientoAjustesSeleccionado = "";

// ---------------- GENERADOR MOTOR 3D ----------------
function initCube(containerId) {
    const cube = document.getElementById(containerId);
    if(!cube) return;
    cube.innerHTML = '';

    // Generamos los 27 cubitos ("cubies")
    for (let x = -1; x <= 1; x++) {
        for (let y = -1; y <= 1; y++) {
            for (let z = -1; z <= 1; z++) {
                const cubie = document.createElement('div');
                cubie.className = `cubie cubie-${x}-${y}-${z}`;
                cubie.dataset.x = x; cubie.dataset.y = y; cubie.dataset.z = z;
                cubie.style.transform = `translate3d(${x*56}px, ${y*56}px, ${z*56}px)`;

                // Generamos las 6 caras para este mini-cubito
                const faces = [
                    { dir: 'U', rx: 90, ry: 0, rz: 0, tz: 28 },
                    { dir: 'D', rx: -90, ry: 0, rz: 0, tz: 28 },
                    { dir: 'F', rx: 0, ry: 0, rz: 0, tz: 28 },
                    { dir: 'B', rx: 180, ry: 0, rz: 0, tz: 28 },
                    { dir: 'R', rx: 0, ry: 90, rz: 0, tz: 28 },
                    { dir: 'L', rx: 0, ry: -90, rz: 0, tz: 28 }
                ];
                faces.forEach(f => {
                    const face = document.createElement('div');
                    face.className = `cubie-face face-${f.dir}`;
                    face.style.transform = `rotateX(${f.rx}deg) rotateY(${f.ry}deg) rotateZ(${f.rz}deg) translateZ(${f.tz}px)`;
                    cubie.appendChild(face);
                });
                cube.appendChild(cubie);
            }
        }
    }
}
// Inicializar los tres cubos 3D
initCube('cube-3d-resolver');
initCube('cube-3d-mezclar');
initCube('cube-3d-ajustes');

// ---------------- COMUNICACIÓN WEBSOCKET ----------------
function conectarServidor() {
    ws = new WebSocket('ws://localhost:8000/ws');
    ws.onopen = () => {
        isConnected = true;
        logTerminal("Conexión establecida con ScramblerBot.");
        document.getElementById('esp-status-dot').className = 'dot green-dot';
        document.getElementById('esp-status-text').innerText = 'CONECTADO';
    };
    ws.onmessage = (event) => {
        const respuesta = JSON.parse(event.data);
        if (respuesta.tipo === "terminal") {
            logTerminal(respuesta.mensaje);
        }
        else if (respuesta.tipo === "movimientos_resolver") {
            const solucion = respuesta.datos.trim();
            document.getElementById('moves-list-resolver').innerText = solucion;
            const totalMovimientos = solucion ? solucion.split(/\s+/).length : 0;
            document.getElementById('moves-text-resolver').innerText = ` / ${totalMovimientos}`;
            document.getElementById('progress-text-resolver').innerText = "0";
            document.getElementById('progress-fill-resolver').style.width = "0%";
        }
        else if (respuesta.tipo === "secuencia_leida" || respuesta.tipo === "preparar_mezcla") {
            if(respuesta.tipo === "secuencia_leida"){
                currentMezcla = respuesta.datos.trim();
                document.getElementById('moves-list-mezclar').innerText = currentMezcla;
                const totalMovimientos = currentMezcla ? currentMezcla.split(/\s+/).length : 0;
                document.getElementById('moves-text-mezclar').innerText = ` / ${totalMovimientos}`;
                document.getElementById('progress-text-mezclar').innerText = "0";
                document.getElementById('progress-fill-mezclar').style.width = "0%";
            }
            // ESTO OCULTA LOS BOTONES Y MUESTRA EL CUBO EN MODO MEZCLAR
            document.getElementById('scramble-controls').classList.add('hidden');
            document.getElementById('cube-viewer-mezclar').classList.remove('hidden');

            if(respuesta.estado) renderizarEstadoCubo(respuesta.estado);
        }
        else if (respuesta.tipo === "actualizar_estado_cubo") {
            if (respuesta.animar && respuesta.movimiento) {
                animarMovimientoYCubo(respuesta.movimiento, respuesta.estado);
            } else {
                renderizarEstadoCubo(respuesta.estado);
            }
            if (respuesta.progreso !== undefined && respuesta.total !== undefined) {
                const modo = respuesta.modo || 'resolver';
                document.getElementById(`progress-text-${modo}`).innerText = respuesta.progreso;
                document.getElementById(`progress-fill-${modo}`).style.width = `${(respuesta.progreso / respuesta.total) * 100}%`;
            }
        }
        else if (respuesta.tipo === "cargar_servos") {
            const config = respuesta.config;
            for (const [id, angulo] of Object.entries(config)) {
                actualizarDOMServo(id, angulo);
            }
        }
        else if (respuesta.tipo === "proceso_completado") {
            isEnded = true;
            isMoving = false;
        }
    };
    ws.onclose = () => {
        isConnected = false;
        logTerminal("Servidor desconectado. Reintentando...");
        document.getElementById('esp-status-dot').className = 'dot red-dot';
        document.getElementById('esp-status-text').innerText = 'DESCONECTADO';
        setTimeout(conectarServidor, 3000);
    };
}
conectarServidor();

// ---------------- RENDERIZADO Y ANIMACIONES FLUIDAS 3D ----------------
function renderizarEstadoCubo(estado) {
    if (!estado) return;

    // Primero, pintamos todos los stickers del 3D en negro
    document.querySelectorAll('.cubie-face').forEach(f => f.style.backgroundColor = '#000');

    for (const [cara, stickers] of Object.entries(estado)) {
        stickers.forEach((colorNombre, idx) => {
            const colorHex = COLOR_MAP[colorNombre] || '#FFFFFF';

            // Render 3D Engine
            const coords = faceMapping[cara][idx];
            // Pintar en todos los visores 3D activos (resolver, mezclar, etc)
            document.querySelectorAll(`.cubie-${coords[0]}-${coords[1]}-${coords[2]} .face-${cara}`).forEach(faceEl => {
                faceEl.style.backgroundColor = colorHex;
            });

            // Render 2D Modal
            const el2D = document.querySelectorAll(`.unfolded-cube [data-face="${cara}"] [data-idx="${idx}"]`);
            el2D.forEach(el => el.style.backgroundColor = colorHex);
        });
    }
}

function animarMovimientoYCubo(movimiento, nuevoEstado) {
    const cara = movimiento[0].toUpperCase();
    const mod = movimiento.slice(1) || "";

    // Identificar el cubo que el usuario está viendo actualmente
    const viewActiva = document.querySelector('.view.active');
    const cubeActivo = viewActiva.querySelector('.cube-3d');
    if(!cubeActivo) return renderizarEstadoCubo(nuevoEstado);

    // Determinar qué bloque de 9 piezas orbitará según la cara
    let selector = ''; let axis = ''; let deg = 90;
    if (cara === 'U') { selector = '[data-y="-1"]'; axis = 'Y'; deg = -90; }
    if (cara === 'D') { selector = '[data-y="1"]'; axis = 'Y'; deg = 90; }
    if (cara === 'F') { selector = '[data-z="1"]'; axis = 'Z'; deg = 90; }
    if (cara === 'B') { selector = '[data-z="-1"]'; axis = 'Z'; deg = -90; }
    if (cara === 'R') { selector = '[data-x="1"]'; axis = 'X'; deg = 90; }
    if (cara === 'L') { selector = '[data-x="-1"]'; axis = 'X'; deg = -90; }

    if (mod === "'") deg = -deg;
    if (mod === "2") deg = deg * 2;

    const piezasGirando = cubeActivo.querySelectorAll(`.cubie${selector}`);

    // Crear un PIVOTE temporal para lograr la fluidez 3D real de todo el bloque
    const pivot = document.createElement('div');
    pivot.className = 'pivot';
    cubeActivo.appendChild(pivot);

    piezasGirando.forEach(c => pivot.appendChild(c));

    // Forzar reflow para aplicar CSS
    void pivot.offsetWidth;

    // Ejecutar el giro
    pivot.style.transition = 'transform 0.35s ease-in-out';
    pivot.style.transform = `rotate${axis}(${deg}deg)`;

    // Al terminar el giro fluido visual, desarmamos el pivote y repintamos instantaneamente
    setTimeout(() => {
        piezasGirando.forEach(c => cubeActivo.appendChild(c));
        pivot.remove();
        renderizarEstadoCubo(nuevoEstado);
    }, 360);
}

// ---------------- NAVEGACIÓN Y CONTROLES DOM ----------------
function setMode(modeId) {
    if (isUnfolded || isMoving) return;
    isEnded = false;
    isCubeScan = false;
    currentMezcla = "";
    document.querySelectorAll('.nav-btn').forEach(btn => btn.classList.remove('active'));
    document.getElementById(`btn-${modeId}`).classList.add('active');

    document.documentElement.style.setProperty('--active-neon', modes[modeId].color);

    const titleText = document.getElementById('current-mode-text');
    titleText.innerText = modes[modeId].text;
    titleText.style.color = modes[modeId].color;
    titleText.style.textShadow = `0 0 8px ${modes[modeId].color}`;

    document.querySelectorAll('.view').forEach(view => view.classList.remove('active'));
    document.getElementById(`view-${modeId}`).classList.add('active');

    document.getElementById('btn-historial').style.visibility = (modeId === 'ajustes') ? 'hidden' : 'visible';

    if (isConnected && ws && ws.readyState === WebSocket.OPEN) {
        ws.send(JSON.stringify({ comando: "resetear_orientacion" }));
    }

    // NUEVO: Resetear al estado resuelto si entramos a resolver o ajustes
    if (modeId === 'resolver' || modeId === 'ajustes') {
        renderizarEstadoCubo(ESTADO_RESUELTO);
    }
    // Resetear vistas al entrar a mezclar
    if (modeId === 'mezclar') {
        document.getElementById('scramble-controls').classList.remove('hidden');
        document.getElementById('cube-viewer-mezclar').classList.add('hidden');
    }
}

function logTerminal(message) {
    const now = new Date();
    const dia = String(now.getDate()).padStart(2, '0');
    const mes = String(now.getMonth()+1).padStart(2, '0');
    const anho = now.getFullYear();
    const hora = String(now.getHours()).padStart(2, '0');
    const minutos = String(now.getMinutes()).padStart(2, '0');
    document.getElementById('terminal-output').innerText = `${dia}/${mes}/${anho} ${hora}:${minutos} - [STATUS]: ${message}`;
}

// Interacción 3D Global con Mouse
document.addEventListener('mousedown', (e) => {
    if(e.target.closest('.scene')) {
        isDragging = true;
        previousMousePosition = { x: e.clientX, y: e.clientY };
    }
});
document.addEventListener('mousemove', (e) => {
    if(isDragging) {
        const deltaMove = { x: e.clientX - previousMousePosition.x, y: e.clientY - previousMousePosition.y };
        cubeRotations.y += deltaMove.x * 0.6;
        cubeRotations.x -= deltaMove.y * 0.6;
        document.querySelectorAll('.cube-3d').forEach(cube => {
            cube.style.transform = `rotateX(${cubeRotations.x}deg) rotateY(${cubeRotations.y}deg)`;
        });
        previousMousePosition = { x: e.clientX, y: e.clientY };
    }
});
document.addEventListener('mouseup', () => { isDragging = false; });
document.addEventListener('mouseleave', () => { isDragging = false; });

// Acciones Botones
function startProcess(mode) {
    if (!isConnected) return logTerminal("Error: Sin conexión al servidor.");
    isMoving = true;
    if (mode === 'resolver') {
        ws.send(JSON.stringify({ comando: "iniciar_resolver" }));
    } else if (mode === 'mezclar') {
        if (!currentMezcla) return logTerminal("Error: Genera o sube una secuencia primero.");
        ws.send(JSON.stringify({ comando: "iniciar_mezclar", secuencia: currentMezcla }));
    } else if (mode === 'ajustes') {
        isCubeScan = true;
        ws.send(JSON.stringify({ comando: "capturar_cubo" }));
    }
}
function stopProcess() {
    if (isConnected && isMoving) {
        ws.send(JSON.stringify({ comando: "detener_proceso" }));
        isMoving = false;
    }
}
function viewProcess(mode) {
    if (!isConnected || !isEnded) return;
    ws.send(JSON.stringify({ comando: mode === 'resolver' ? "recibir_resumen_resolver" : "recibir_resumen_mezclar" }));
}
function downloadWCA() { if (isConnected) ws.send(JSON.stringify({ comando: "abrir_explorador" })); }
function generateWCA() { if (isConnected) ws.send(JSON.stringify({ comando: "generar_movimientos" })); }

// Actualiza únicamente la pantalla sin enviar evento WebSocket (para evitar bucles al cargar)
function actualizarDOMServo(id, value) {
    let cleanValue = Math.max(0, Math.min(180, Number(value)));
    const slider = document.getElementById(`slider-${id}`);
    const input = document.getElementById(`input-${id}`);
    if (slider) slider.value = cleanValue;
    if (input) input.value = cleanValue;
}

// Se ejecuta al mover los sliders/inputs y guarda en Python
function syncAngle(id, value) {
    let cleanValue = Math.max(0, Math.min(180, Number(value)));
    actualizarDOMServo(id, cleanValue);

    if (isConnected) {
        ws.send(JSON.stringify({
            comando: "guardar_servo",
            clave: id,
            angulo: cleanValue
        }));
    }
}

function testServo(servoName, positionName) {
    if (!isConnected) return;
    const idMap = {
        'Inferior-Derecha': 'inf-der', 'Inferior-Centro': 'inf-cen', 'Inferior-Izquierda': 'inf-izq',
        'Superior-Abajo': 'sup-aba', 'Superior-Arriba': 'sup-arr', 'Superior-Volteo': 'sup-vol'
    };
    const key = idMap[`${servoName}-${positionName}`];
    const angle = document.getElementById(`input-${key}`).value;

    ws.send(JSON.stringify({
        comando: "mover_servo",
        clave: key,
        servo: servoName,
        angulo: parseInt(angle)
    }));
}

// Abrir / Cerrar Dropdown
function toggleDropdownAjustes(event) {
    event.stopPropagation();
    const menu = document.getElementById('dropdown-menu-ajustes');
    if (menu) menu.classList.toggle('hidden');
}

// Cerrar si hace clic fuera
document.addEventListener('click', (e) => {
    const dropdown = document.getElementById('dropdown-ajustes');
    const menu = document.getElementById('dropdown-menu-ajustes');
    if (menu && !menu.classList.contains('hidden') && dropdown && !dropdown.contains(e.target)) {
        menu.classList.add('hidden');
    }
});

// Seleccionar opción
function selectMove(mov) {
    movimientoAjustesSeleccionado = mov;
    document.getElementById('selected-move-text').innerText = `Movimiento: ${mov}`;
    const menu = document.getElementById('dropdown-menu-ajustes');
    if (menu) menu.classList.add('hidden');
}

// Ejecutar movimiento
function ejecutarMovimientoAjustes() {
    if (!isConnected) return logTerminal("Error: Sin conexión con el servidor.");
    if (!movimientoAjustesSeleccionado) {
        return logTerminal("Error: Seleccione un movimiento primero.");
    }

    ws.send(JSON.stringify({
        comando: "ejecutar_movimiento_ajustes",
        movimiento: movimientoAjustesSeleccionado,
        scan: isCubeScan
    }));
}

// Desplegar cubo
function toggleUnfold() {
    document.getElementById('unfold-modal').classList.toggle('hidden');
    isUnfolded = !isUnfolded;
}

setMode('resolver');