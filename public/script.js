// --- 1. IMPORT FUNGSI FIREBASE ---

// Impor objek 'auth' yang sudah diinisialisasi dari file konfigurasi Anda
import { auth } from './firebase-config.js'; 

// Impor fungsi 'onAuthStateChanged' dan 'signOut' dari Firebase SDK
// (Pastikan Anda sudah memuat Firebase SDK sebagai modul di HTML Anda,
// atau path ini disesuaikan dengan setup 'node_modules' Anda)
import { onAuthStateChanged, signOut } from 'https://www.gstatic.com/firebasejs/9.10.0/firebase-auth.js';


// --- 2. JALANKAN KODE SETELAH DOM SIAP ---

document.addEventListener('DOMContentLoaded', () => {

    console.log('Halaman SpotOn Park dimuat, script.js (versi Firebase) berjalan.');

    // Ambil elemen-elemen dari halaman (DOM)
    const userIcon = document.querySelector('.user-icon');
    const userIconTag = userIcon ? userIcon.querySelector('i') : null;
    const featureBoxes = document.querySelectorAll('.feature-box');
    
    // Per-jelas elemen feature box
    const klienBox = featureBoxes.length > 0 ? featureBoxes[0] : null;
    const qrBox = featureBoxes.length > 1 ? featureBoxes[1] : null;
    const adminBox = featureBoxes.length > 2 ? featureBoxes[2] : null;

    
    // --- 3. PENDENGAR STATUS AUTENTIKASI ---

    // Ini adalah inti dari semuanya.
    // 'onAuthStateChanged' akan otomatis berjalan:
    // 1. Saat halaman baru dimuat (untuk mengecek status login)
    // 2. Saat user login
    // 3. Saat user logout
    onAuthStateChanged(auth, (user) => {
        
        if (user) {
            // --- PENGGUNA SUDAH LOGIN (user 'bukan null') ---
            console.log('Status: Login (user ID:', user.uid, ')');

            // 1. Ubah Ikon User menjadi Tombol Logout
            if (userIconTag) {
                userIconTag.classList.remove('fa-user');
                userIconTag.classList.add('fa-right-from-bracket'); // Ikon "sign-out"
                userIcon.title = 'Log Out';
            }

            // 2. Atur Event Klik untuk LOGOUT (dengan konfirmasi)
            if (userIcon) {
                userIcon.onclick = () => { // Gunakan .onclick agar listener tidak menumpuk
                    if (confirm('Apakah Anda yakin ingin log out?')) {
                        signOut(auth).then(() => {
                            // Logout berhasil
                            console.log('User berhasil log out.');
                            // Kita tidak perlu reload, onAuthStateChanged akan otomatis 
                            // berjalan lagi dan masuk ke blok 'else'
                        }).catch((error) => {
                            console.error('Error saat logout:', error);
                        });
                    }
                };
            }

            // 3. Atur Event Klik Admin Dashboard (jika login)
            if (adminBox) {
                adminBox.onclick = () => {
                    window.location.href = './admin/dashboard.html'; // Langsung ke halaman admin
                };
            }

        } else {
            // --- PENGGUNA TIDAK LOGIN (user 'adalah null') ---
            console.log('Status: Log Out');

            // 1. Pastikan Ikon adalah Tombol Login
            if (userIconTag) {
                userIconTag.classList.remove('fa-right-from-bracket');
                userIconTag.classList.add('fa-user');
                userIcon.title = 'Log In';
            }

            // 2. Atur Event Klik untuk LOGIN
            if (userIcon) {
                userIcon.onclick = () => {
                    window.location.href = './admin/login.html'; // Arahkan ke halaman login
                };
            }

            // 3. Atur Event Klik Admin Dashboard (jika tidak login)
            if (adminBox) {
                adminBox.onclick = () => {
                    alert('Anda harus login untuk mengakses Admin Dashboard.');
                    window.location.href = './admin/login.html'; // Arahkan ke halaman login
                };
            }
        }
    });

    // --- 4. ATUR EVENT UNTUK TOMBOL YANG TIDAK BUTUH LOGIN ---
    // Event ini bisa diatur di luar 'onAuthStateChanged'
    
    if (klienBox) {
        klienBox.addEventListener('click', () => {
            window.location.href = './scan-portal.html';
        });
    }

    if (qrBox) {
        qrBox.addEventListener('click', () => {
            window.location.href = './qr-generate.html';
        });
    }

});