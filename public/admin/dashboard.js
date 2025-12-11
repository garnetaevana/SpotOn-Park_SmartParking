// ==========================================
// 1. IMPORTS & CONFIG
// ==========================================
import { db, auth } from "../firebase-config.js"; 
import { signOut, onAuthStateChanged } from "https://www.gstatic.com/firebasejs/10.12.0/firebase-auth.js";
import { collection, onSnapshot } from "https://www.gstatic.com/firebasejs/10.12.0/firebase-firestore.js";

// ===== Data Collection =====
const slotRef = collection(db, "parking_slots");
const logRef = collection(db, "parking_logs");

// ===== Constants (TARIF BARU) =====
const SLOT_LIST_COUNT = 4; 
const RATE_FIRST_HOUR = 3000;
const RATE_NEXT_HOUR = 2000;

// ===== Elements Selection =====
const els = {
    totalSlot: document.getElementById("totalSlot"),
    availableSlot: document.getElementById("availableSlot"),
    activeVehicles: document.getElementById("activeVehicles"),
    occupancyRate: document.getElementById("occupancyRate"),
    totalIncome: document.getElementById("totalIncome"),
    parkingTable: document.getElementById("parkingTableBody"),
    statusIndicator: document.getElementById("statusIndicator"),
    statusText: document.getElementById("statusText"),
    periodSelect: document.getElementById("periodSelect"), // Dropdown filter
    btnLogout: document.getElementById("btnLogout"),
    btnHome: document.getElementById("btnHome"),
    
    // Stream Elements
    camUrlInput: document.getElementById("camUrlInput"), 
    streamVideo: document.getElementById("streamVideo"),
    startStreamBtn: document.getElementById("startStream"),
    stopStreamBtn: document.getElementById("stopStream"),
    streamPlaceholder: document.getElementById("streamPlaceholder"),
    streamStatus: document.getElementById("streamStatus")
};

// Variable Global untuk menyimpan data logs mentah
let allParkingLogs = [];

// ===== Auth State =====
onAuthStateChanged(auth, (user) => {
  if (!user) {
    console.log("User belum login");
    // window.location.href = "login.html"; 
  } else {
    console.log("✅ User login:", user.email);
  }
});

// ===== Navigation =====
if (els.btnLogout) {
  els.btnLogout.addEventListener("click", async () => {
    await signOut(auth);
    window.location.href = "../index.html";
  });
}
if (els.btnHome) {
  els.btnHome.addEventListener("click", () => {
    window.location.href = "../index.html";
  });
}

// ==========================================
// 2. LIVE STREAMING LOGIC
// ==========================================
let isStreaming = false;

function initializeStreaming() {
  if (els.streamVideo && els.streamVideo.tagName === 'VIDEO') {
      const img = document.createElement('img');
      img.id = 'streamVideo';
      img.className = els.streamVideo.className;
      img.style.objectFit = 'contain';
      img.referrerPolicy = "no-referrer"; 
      img.crossOrigin = "anonymous"; 
      img.src = ""; 
      els.streamVideo.parentNode.replaceChild(img, els.streamVideo);
      els.streamVideo = img; 
  }

  if (els.startStreamBtn) els.startStreamBtn.addEventListener('click', startStreaming);
  if (els.stopStreamBtn) els.stopStreamBtn.addEventListener('click', stopStreaming);
}

function startStreaming() {
  if (!els.camUrlInput) return;
  let url = els.camUrlInput.value.trim();

  if (!url) {
      alert("Silakan masukkan URL Ngrok terlebih dahulu!");
      els.camUrlInput.focus();
      return;
  }

  if (url.endsWith('/')) url = url.slice(0, -1);
  if (!url.endsWith('/stream')) url += '/stream'; 

  const streamUrl = `${url}?t=${new Date().getTime()}`;
  
  if(els.streamStatus) {
      els.streamStatus.className = 'px-3 py-1 rounded-full bg-yellow-500/20 text-yellow-300 text-xs font-bold';
      els.streamStatus.innerHTML = 'Connecting...';
  }
  
  els.streamVideo.src = ""; 
  
  setTimeout(() => {
      els.streamVideo.src = streamUrl;
      els.streamVideo.classList.remove('hidden');
      if (els.streamPlaceholder) els.streamPlaceholder.classList.add('hidden');
      if (els.stopStreamBtn) els.stopStreamBtn.classList.remove('hidden');
  }, 100);
  
  els.streamVideo.onload = () => {
    isStreaming = true;
    if(els.streamStatus) {
        els.streamStatus.className = 'px-3 py-1 rounded-full bg-green-500/20 text-green-300 text-xs font-bold';
        els.streamStatus.innerHTML = 'Live';
    }
  };
  
  els.streamVideo.onerror = () => {
    if(els.streamStatus) {
        els.streamStatus.className = 'px-3 py-1 rounded-full bg-red-500/20 text-red-300 text-xs font-bold';
        els.streamStatus.innerHTML = 'Error / Blocked';
    }
  };
}

function stopStreaming() {
  els.streamVideo.src = '';
  els.streamVideo.removeAttribute("src");
  if (els.streamPlaceholder) els.streamPlaceholder.classList.remove('hidden');
  if (els.stopStreamBtn) els.stopStreamBtn.classList.add('hidden');
  if(els.streamStatus) {
      els.streamStatus.className = 'px-3 py-1 rounded-full bg-gray-500/20 text-gray-300 text-xs font-bold';
      els.streamStatus.innerHTML = 'Offline';
  }
  isStreaming = false;
}

// ==========================================
// 3. SLOT MONITORING
// ==========================================
function initializeSlotMonitoring() {
  onSnapshot(slotRef, (snapshot) => {
    const slots = snapshot.docs.map(doc => doc.data());
    // Hitung slot yang terisi
    const occupied = slots.filter(s => 
        (s.status && s.status.toLowerCase() === "occupied") || 
        (s.status && s.status.toLowerCase() === "terisi")
    ).length;
    
    const available = SLOT_LIST_COUNT - occupied;
    const occupancyRate = SLOT_LIST_COUNT > 0 ? ((occupied / SLOT_LIST_COUNT) * 100).toFixed(0) : 0;

    // Update UI Stats
    if(els.totalSlot) animateValue(els.totalSlot, parseInt(els.totalSlot.textContent) || 0, SLOT_LIST_COUNT, 500);
    if(els.availableSlot) animateValue(els.availableSlot, parseInt(els.availableSlot.textContent) || 0, available, 500);
    if(els.activeVehicles) animateValue(els.activeVehicles, parseInt(els.activeVehicles.textContent) || 0, occupied, 500);
    if(els.occupancyRate) animateValue(els.occupancyRate, parseInt(els.occupancyRate.textContent) || 0, occupancyRate, 500);

    // Indikator Status
    if(els.statusIndicator) {
        els.statusIndicator.className = occupied > 0 ? 'w-2 h-2 bg-green-400 rounded-full animate-pulse' : 'w-2 h-2 bg-green-400 rounded-full';
    }
    if(els.statusText) els.statusText.textContent = "Data Live Terhubung";

  }, (error) => {
    console.error("Error fetching slots:", error);
    if(els.statusText) els.statusText.textContent = "Koneksi Putus";
    if(els.statusIndicator) els.statusIndicator.className = 'w-2 h-2 bg-red-500 rounded-full';
  });
}

// ==========================================
// 4. PARKING HISTORY & INCOME CALCULATION
// ==========================================
function initializeParkingHistory() {
  onSnapshot(logRef, (snapshot) => {
    // 1. Simpan semua logs ke variabel global
    allParkingLogs = snapshot.docs.map(doc => ({ id: doc.id, ...doc.data() }))
      .sort((a, b) => b.entryTime - a.entryTime);
    
    // 2. Render Tabel
    renderTable(allParkingLogs);

    // 3. Hitung Pendapatan Awal (Default: Hari Ini)
    calculateTotalIncome();
    
  });
}

function renderTable(logs) {
    if(!els.parkingTable) return;
    els.parkingTable.innerHTML = "";
    
    if (logs.length === 0) {
      els.parkingTable.innerHTML = `<tr><td colspan="5" class="px-4 py-8 text-center text-white/50">Belum ada riwayat parkir</td></tr>`;
      return;
    }

    logs.forEach((log) => {
      let entryDate = "-", exitDate = "-";
      let amountDisplay = "-";
      
      try {
          if(log.entryTime) entryDate = new Date(log.entryTime).toLocaleString("id-ID");
          
          if(log.exitTime) {
             exitDate = new Date(log.exitTime).toLocaleString("id-ID");
             // Tampilkan biaya jika sudah keluar
             const amount = log.amount || calculateCost(log.entryTime, log.exitTime);
             amountDisplay = `Rp ${amount.toLocaleString('id-ID')}`;
          } else {
             exitDate = `<span class="inline-flex items-center px-2 py-1 rounded-full bg-yellow-500/20 text-yellow-300 text-xs font-semibold">Aktif</span>`;
             amountDisplay = `<span class="text-white/30 text-xs">Berjalan</span>`;
          }
      } catch(e) {}

      const row = document.createElement('tr');
      row.className = 'border-b border-white/10 hover:bg-white/5 transition-all';
      row.innerHTML = `
        <td class="px-4 py-4 font-bold">${log.plateNumber || '-'}</td>
        <td class="px-4 py-4 text-white/80">${entryDate}</td>
        <td class="px-4 py-4">${exitDate}</td>
        <td class="px-4 py-4 text-center">
          ${log.paymentStatus === 'paid' || log.exitTime 
            ? `<span class="text-green-400 font-bold text-xs">LUNAS<br>${amountDisplay}</span>` 
            : '<span class="text-yellow-400 text-xs">BELUM</span>'}
        </td>
      `;
      els.parkingTable.appendChild(row);
    });
}

// === LOGIKA HITUNG BIAYA (Backup jika database tidak menyimpan amount) ===
function calculateCost(entryTime, exitTime) {
    if (!entryTime || !exitTime) return 0;
    
    const start = new Date(entryTime).getTime();
    const end = new Date(exitTime).getTime();
    const diffMs = end - start;
    const totalMinutes = Math.floor(diffMs / (1000 * 60));
    
    if (totalMinutes <= 0) return 0;

    const hours = Math.ceil(totalMinutes / 60); // Pembulatan ke atas (1 jam 1 menit = 2 jam)
    
    // Rumus: Jam pertama 3000, selanjutnya 2000
    let total = RATE_FIRST_HOUR;
    if (hours > 1) {
        total += (hours - 1) * RATE_NEXT_HOUR;
    }
    
    return total;
}

// === LOGIKA FILTER PENDAPATAN ===
function calculateTotalIncome() {
    if (!els.periodSelect || !els.totalIncome) return;

    const period = els.periodSelect.value; // 'day', 'week', 'month'
    const now = new Date();
    
    // Reset jam ke 00:00:00 hari ini untuk perbandingan
    const todayStart = new Date(now.getFullYear(), now.getMonth(), now.getDate()).getTime();
    
    // Tentukan batas waktu filter
    let timeLimit = todayStart;
    
    if (period === 'week') {
        // Mundur 7 hari
        timeLimit = todayStart - (7 * 24 * 60 * 60 * 1000); 
    } else if (period === 'month') {
        // Awal bulan ini
        timeLimit = new Date(now.getFullYear(), now.getMonth(), 1).getTime();
    }

    // Filter Logs: Hanya yang sudah bayar/keluar DAN dalam periode waktu
    const filteredLogs = allParkingLogs.filter(log => {
        const isPaid = log.paymentStatus === 'paid' || (log.exitTime && log.exitTime > 0);
        if (!isPaid) return false;

        const logTime = new Date(log.exitTime || log.paidAt).getTime();
        return logTime >= timeLimit;
    });

    // Sum Total
    const totalIncome = filteredLogs.reduce((sum, log) => {
        // Gunakan amount dari DB jika ada, jika tidak hitung manual
        const amount = log.amount ? parseInt(log.amount) : calculateCost(log.entryTime, log.exitTime);
        return sum + amount;
    }, 0);

    // Animasi Update Angka
    els.totalIncome.innerText = totalIncome.toLocaleString('id-ID');
}

// Event Listener untuk Dropdown Filter
if (els.periodSelect) {
    els.periodSelect.addEventListener('change', calculateTotalIncome);
}


// ==========================================
// 5. UTILITIES
// ==========================================
function animateValue(element, start, end, duration) {
  if (!element) return;
  const range = end - start;
  const increment = end > start ? 1 : -1;
  const stepTime = Math.abs(Math.floor(duration / (Math.abs(range) || 1)));
  let current = start;
  const timer = setInterval(() => {
    current += increment;
    element.textContent = current;
    if ((increment > 0 && current >= end) || (increment < 0 && current <= end)) {
      element.textContent = end;
      clearInterval(timer);
    }
  }, Math.max(stepTime, 10));
}

function updateClock() {
  const now = new Date();
  if (document.getElementById('realTimeClock')) 
      document.getElementById('realTimeClock').textContent = now.toLocaleTimeString('id-ID') + ' WIB';
  if (document.getElementById('realTimeDate')) 
      document.getElementById('realTimeDate').textContent = now.toLocaleDateString('id-ID', { weekday: 'long', year: 'numeric', month: 'long', day: 'numeric' });
}

// ==========================================
// 6. INITIALIZATION
// ==========================================
document.addEventListener('DOMContentLoaded', () => {
  updateClock();
  setInterval(updateClock, 1000);
  
  document.querySelectorAll('.stat-card').forEach((card, index) => {
      setTimeout(() => card.classList.add('animate-slide-in'), index * 100);
  });

  initializeStreaming();       
  initializeSlotMonitoring();  
  initializeParkingHistory();
});