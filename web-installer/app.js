// UI Elements
const btnConnect = document.getElementById('btn-connect');
const statusText = document.getElementById('status-text');
const connectionPanel = document.getElementById('connection-panel');
const uploadPanel = document.getElementById('upload-panel');
const dropZone = document.getElementById('drop-zone');
const fileInput = document.getElementById('file-input');
const logConsole = document.getElementById('log-console');

const progressContainer = document.getElementById('progress-container');
const progressTitle = document.getElementById('progress-title');
const progressFill = document.getElementById('progress-fill');
const progressStatus = document.getElementById('progress-status');

// Serial Port variables
let port;
let reader;
let writer;
let keepReading = true;

// Utility: Logging
function log(msg, type = 'info') {
    const p = document.createElement('p');
    p.className = `log-${type}`;
    p.textContent = `> ${msg}`;
    logConsole.appendChild(p);
    logConsole.scrollTop = logConsole.scrollHeight;
}

// Utility: Read line from serial
async function readLineFromSerial() {
    let line = '';
    while (true) {
        const { value, done } = await reader.read();
        if (done) break;
        if (value) {
            line += value;
            if (line.includes('\n')) {
                const parts = line.split('\n');
                line = parts.pop();
                return parts[0].trim();
            }
        }
    }
    return null;
}

// Check Web Serial API support
if (!('serial' in navigator)) {
    log('Web Serial API não suportada neste navegador. Use Chrome ou Edge.', 'error');
    btnConnect.disabled = true;
    btnConnect.textContent = 'NAVEGADOR INCOMPATÍVEL';
}

// Conectar ao KIT via Serial
btnConnect.addEventListener('click', async () => {
    try {
        port = await navigator.serial.requestPort();
        await port.open({ baudRate: 921600 }); // High baud rate for fast transfer

        const textEncoder = new TextEncoderStream();
        textEncoder.readable.pipeTo(port.writable);
        writer = textEncoder.writable.getWriter();

        const textDecoder = new TextDecoderStream();
        port.readable.pipeTo(textDecoder.writable);
        reader = textDecoder.readable.getReader();

        statusText.textContent = 'CONECTADO';
        connectionPanel.classList.add('connected');
        btnConnect.textContent = 'DESCONECTAR';
        uploadPanel.classList.remove('disabled');
        
        log('Conectado à porta serial com sucesso.', 'success');
        
        // Listener loop em background para esvaziar buffers mortos
        // Em um app de produção, esse listener escutaria mensagens do firmware.
        // Aqui usaremos readLineFromSerial() sob demanda.
        
    } catch (err) {
        log(`Erro de conexão: ${err.message}`, 'error');
    }
});

// Drag and Drop handlers
dropZone.addEventListener('click', () => fileInput.click());

dropZone.addEventListener('dragover', (e) => {
    e.preventDefault();
    dropZone.classList.add('dragover');
});

dropZone.addEventListener('dragleave', () => {
    dropZone.classList.remove('dragover');
});

dropZone.addEventListener('drop', (e) => {
    e.preventDefault();
    dropZone.classList.remove('dragover');
    if (e.dataTransfer.files.length > 0) {
        handleFile(e.dataTransfer.files[0]);
    }
});

fileInput.addEventListener('change', (e) => {
    if (e.target.files.length > 0) {
        handleFile(e.target.files[0]);
    }
});

async function handleFile(file) {
    if (!file.name.endsWith('.kit') && !file.name.endsWith('.zip')) {
        log('Apenas arquivos .kit são suportados.', 'error');
        return;
    }

    dropZone.classList.add('hidden');
    progressContainer.classList.remove('hidden');
    progressTitle.textContent = file.name;
    
    try {
        log(`Extraindo pacote: ${file.name}...`);
        
        const zip = new JSZip();
        const contents = await zip.loadAsync(file);
        
        // Lê manifest
        if (!contents.file('manifest.json')) {
            throw new Error("manifest.json não encontrado no pacote.");
        }
        
        const manifestText = await contents.file('manifest.json').async("string");
        const manifest = JSON.parse(manifestText);
        const toolId = manifest.id;
        
        log(`Tool detectada: ${manifest.name} (${toolId})`, 'success');
        
        await sendToDevice(contents, toolId);
        
    } catch (err) {
        log(`Erro ao processar arquivo: ${err.message}`, 'error');
        resetUploadUI();
    }
}

async function sendCommandAndWait(cmd) {
    await writer.write(cmd + '\n');
    log(`TX: ${cmd}`);
    
    // Espera a resposta OK
    let retries = 0;
    while (retries < 50) {
        const { value, done } = await reader.read();
        if (value) {
            const resp = value.trim();
            log(`RX: ${resp}`);
            if (resp === 'OK') return true;
            if (resp.startsWith('ERR')) throw new Error(`Firmware recusou: ${resp}`);
        }
        retries++;
    }
    throw new Error("Timeout aguardando resposta do firmware.");
}

async function sendBinaryData(data) {
    // Para enviar binário, precisamos de um writer cru (sem TextEncoder)
    // Liberamos os locks de texto temporariamente
    writer.releaseLock();
    reader.releaseLock();
    
    const binWriter = port.writable.getWriter();
    await binWriter.write(data);
    await binWriter.releaseLock();
    
    // Reconecta os encoders de texto
    const textEncoder = new TextEncoderStream();
    textEncoder.readable.pipeTo(port.writable);
    writer = textEncoder.writable.getWriter();

    const textDecoder = new TextDecoderStream();
    port.readable.pipeTo(textDecoder.writable);
    reader = textDecoder.readable.getReader();
    
    // Espera OK após envio binário
    let retries = 0;
    while (retries < 50) {
        const { value, done } = await reader.read();
        if (value) {
            const resp = value.trim();
            log(`RX (bin_ack): ${resp}`);
            if (resp === 'OK') return true;
            if (resp.startsWith('ERR')) throw new Error(`Erro na gravação: ${resp}`);
        }
        retries++;
    }
}

async function sendToDevice(zipContents, toolId) {
    try {
        progressStatus.textContent = 'Iniciando transação...';
        progressFill.style.width = '5%';
        
        await sendCommandAndWait(`KIT_TOOL_BEGIN ${toolId}`);
        
        // Pega todos os arquivos do zip
        const files = Object.values(zipContents.files).filter(f => !f.dir);
        let completed = 0;
        
        for (const file of files) {
            progressStatus.textContent = `Enviando ${file.name}...`;
            
            const data = await file.async("uint8array");
            const size = data.length;
            
            await sendCommandAndWait(`KIT_FILE_WRITE ${file.name} ${size}`);
            await sendBinaryData(data);
            
            completed++;
            const pct = 5 + ((completed / files.length) * 90);
            progressFill.style.width = `${pct}%`;
        }
        
        progressStatus.textContent = 'Finalizando...';
        await sendCommandAndWait('KIT_TOOL_COMMIT');
        
        progressFill.style.width = '100%';
        progressStatus.textContent = 'INSTALAÇÃO CONCLUÍDA!';
        log('Tool instalada com sucesso. O menu do KIT foi atualizado.', 'success');
        
        setTimeout(() => resetUploadUI(), 3000);
        
    } catch (err) {
        log(`Falha no envio serial: ${err.message}`, 'error');
        resetUploadUI();
    }
}

function resetUploadUI() {
    dropZone.classList.remove('hidden');
    progressContainer.classList.add('hidden');
    progressFill.style.width = '0%';
    fileInput.value = '';
}
