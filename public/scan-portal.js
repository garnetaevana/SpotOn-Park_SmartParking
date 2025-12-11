// ==========================================
// 1. CONFIGURATION & IMPORTS
// ==========================================
import { initializeApp } from "https://www.gstatic.com/firebasejs/10.12.0/firebase-app.js";
import { getFirestore, doc, getDoc, updateDoc, setDoc } from "https://www.gstatic.com/firebasejs/10.12.0/firebase-firestore.js";

// --- KONFIGURASI FIREBASE (LANGSUNG) ---
const firebaseConfig = {
  apiKey: "AIzaSyAqFcSWRj7qyEFVyfhCAaIyTdU7xaoNyk0",
  authDomain: "spoton-park.firebaseapp.com",
  projectId: "spoton-park",
  storageBucket: "spoton-park.firebasestorage.app",
  messagingSenderId: "87537754672",
  appId: "1:87537754672:web:8a4dfdd6166c47996c43f5",
  measurementId: "G-LVVRWV0TPV"
};

// Initialize Firebase
const app = initializeApp(firebaseConfig);
const db = getFirestore(app);

// Global variables
let html5QrCodeScanner;
let isProcessing = false;

// DOM Elements
const statusContainer = document.getElementById('statusContainer');
const statusIcon = document.getElementById('statusIcon');
const statusTitle = document.getElementById('statusTitle');
const statusMessage = document.getElementById('statusMessage');
const transactionDetails = document.getElementById('transactionDetails');
const resetScanner = document.getElementById('resetScanner');
const permissionModal = document.getElementById('camera-permission-modal');

// ==========================================
// 2. SCANNER LOGIC
// ==========================================
async function initializeScanner() {
    if (typeof Html5Qrcode === "undefined") {
        setTimeout(initializeScanner, 500);
        return;
    }

    try {
        html5QrCodeScanner = new Html5Qrcode("reader");
        const cameras = await Html5Qrcode.getCameras().catch(err => {
            if (err.name === 'NotAllowedError') {
                if(permissionModal) permissionModal.classList.remove('hidden');
                throw new Error("Izin kamera ditolak");
            }
            throw err;
        });
        
        if (cameras && cameras.length > 0) {
            let cameraId = cameras[0].id;
            const backCam = cameras.find(c => c.label.toLowerCase().includes('back'));
            if(backCam) cameraId = backCam.id;
            await startScanner(cameraId);
            showStatus('ready', 'Scanner Siap', 'Arahkan kamera ke QR Code tiket');
        } else {
            await startScanner({ facingMode: "environment" });
        }
    } catch (error) {
        console.error('Scanner Init Error:', error);
        if (error.message !== "Izin kamera ditolak") {
            showStatus('error', 'Kamera Error', 'Pastikan menggunakan HTTPS');
        }
    }
}

async function startScanner(cameraConfig) {
    try {
        await html5QrCodeScanner.start(
            cameraConfig, 
            { fps: 10, qrbox: { width: 250, height: 250 }, aspectRatio: 1.0 },
            onScanSuccess
        );
    } catch (err) {
        console.error("Start Error:", err);
    }
}

async function onScanSuccess(decodedText) {
    if (isProcessing) return;
    isProcessing = true;
    try { await html5QrCodeScanner.pause(); } catch(e){}
    console.log('📱 QR Scanned:', decodedText);
    await processExit(decodedText.trim());
}

// ==========================================
// 3. TRANSACTION LOGIC
// ==========================================
async function processExit(tokenOrId) {
    showStatus('checking', 'Memeriksa Data...', 'Mohon tunggu sebentar');
    
    try {
        const { collection, query, where, getDocs } = await import("https://www.gstatic.com/firebasejs/10.12.0/firebase-firestore.js");
        const logsRef = collection(db, "parking_logs");
        
        const q1 = query(logsRef, where("exitToken", "==", tokenOrId));
        const snap1 = await getDocs(q1);

        let docData = null, docRef = null;

        if (!snap1.empty) {
            docData = snap1.docs[0].data();
            docRef = snap1.docs[0].ref;
        }

        if (docData && docRef) {
            await validateAndOpen(docData, docRef);
        } else {
            showStatus('error', 'Token Tidak Valid', 'QR Code tidak dikenali');
            setTimeout(resetProcess, 3000);
        }

    } catch (error) {
        console.error('Process Error:', error);
        showStatus('error', 'Error Sistem', 'Gagal terhubung ke database');
        setTimeout(resetProcess, 3000);
    }
}

async function validateAndOpen(data, docRef) {
    document.getElementById('detailId').textContent = data.transactionId || '-';
    document.getElementById('detailPlate').textContent = data.plateNumber || '-';
    document.getElementById('detailStatus').textContent = data.paymentStatus || '-';
    
    if (data.paymentStatus !== 'paid') {
        showStatus('locked', 'Belum Lunas', 'Silakan bayar dulu');
        setTimeout(resetProcess, 4000);
        return;
    }

    if (data.status === 'completed') {
        showStatus('locked', 'Sudah Keluar', 'Tiket sudah dipakai');
        setTimeout(resetProcess, 4000);
        return;
    }

    // === SUKSES ===
    showStatus('open', 'Portal Dibuka', 'Selamat Jalan!');
    
    try {
        // 1. Update Status Transaksi
        await updateDoc(docRef, {
            status: 'completed',
            exitProcessed: true,
            exitTime: Date.now()
        });
    } catch(e) { console.error(e); }

    // 2. Buka Palang (VIA FIRESTORE TRIGGER)
    await controlGateSequence();
}

// ==========================================
// 4. GATE CONTROL (FIRESTORE TRIGGER)
// ==========================================
async function controlGateSequence() {
    try {
        console.log("➡️ Mengirim sinyal BUKA gate...");
        const gateRef = doc(db, "gate_control", "exit_gate");
        
        // Pastikan kita hanya mengirim OPEN
        // ESP32 yang akan mengembalikan ke CLOSED setelah menerima sinyal
        await setDoc(gateRef, {
            command: "OPEN",
            timestamp: Date.now() // Timestamp memaksa Firestore melihat ini sebagai perubahan baru
        }, { merge: true });

        console.log("✅ Sinyal Terkirim.");

        // Hitung Mundur UI (Visual Saja)
        let timeLeft = 60;
        const timer = setInterval(() => {
            timeLeft--;
            statusMessage.textContent = `Gate terbuka. Menutup dalam ${timeLeft} detik...`;
            
            if(timeLeft <= 0) {
                clearInterval(timer);
                
                // BACKUP: Kirim sinyal CLOSED dari Web jika ESP32 belum meresetnya
                // Ini menjaga agar command tidak stuck di OPEN selamanya
                setDoc(gateRef, {
                    command: "CLOSED",
                    timestamp: Date.now()
                }, { merge: true }).catch(e => console.log("Backup reset failed"));
                
                resetProcess();
            }
        }, 1000);

    } catch (error) {
        console.error("Gate Control Error:", error);
        showStatus('error', 'Gagal Buka Gate', 'Cek koneksi internet');
        setTimeout(resetProcess, 5000);
    }
}

// ==========================================
// 5. UTILITIES
// ==========================================
function showStatus(type, title, message) {
    statusContainer.classList.remove('hidden');
    statusContainer.className = 'animate-slide-in'; 
    
    const box = statusContainer.firstElementChild;
    box.className = "glass-effect rounded-2xl p-6 mb-6 border-2";

    let icon = '';
    if (type === 'ready') { 
        icon = '📷'; 
        box.classList.add('border-blue-500/30', 'bg-blue-500/10');
    } else if (type === 'checking') { 
        icon = '⏳'; 
        box.classList.add('border-yellow-500/30', 'bg-yellow-500/10');
    } else if (type === 'open') { 
        transactionDetails.classList.remove('hidden'); 
        icon = '✅'; 
        box.classList.add('border-green-500/50', 'bg-green-500/20');
    } else if (type === 'locked' || type === 'error') { 
        icon = '⛔'; 
        box.classList.add('border-red-500/50', 'bg-red-500/20');
    }

    statusIcon.textContent = icon;
    statusTitle.textContent = title;
    statusMessage.textContent = message;
}

function resetProcess() {
    isProcessing = false;
    statusContainer.classList.add('hidden');
    transactionDetails.classList.add('hidden');
    if (html5QrCodeScanner) {
        try { html5QrCodeScanner.resume(); } catch(e){}
    }
    showStatus('ready', 'Scanner Siap', 'Arahkan kamera ke QR Code tiket');
}

// Init
document.addEventListener('DOMContentLoaded', () => {
    if (location.protocol !== 'https:' && location.hostname !== 'localhost') {
        alert("Wajib HTTPS untuk akses kamera!");
    }
    setTimeout(initializeScanner, 1000);
    if(resetScanner) resetScanner.addEventListener('click', resetProcess);
});