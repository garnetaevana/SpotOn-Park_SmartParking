import { auth } from "../../firebase-config.js";
import { signOut, onAuthStateChanged } from "https://www.gstatic.com/firebasejs/10.12.0/firebase-auth.js";

// === DOM Element ===
const btnSignIn = document.getElementById("btnSignIn");
const btnSignOut = document.getElementById("btnSignOut");

// === Event Tombol Sign In ===
btnSignIn.addEventListener("click", () => {
  // Arahkan ke halaman login
  window.location.href = "../login.html";
});

// === Event Tombol Sign Out ===
btnSignOut.addEventListener("click", async () => {
  try {
    await signOut(auth);
    console.log("Berhasil logout");
    window.location.href = "../../index.html"; // kembali ke halaman utama
  } catch (error) {
    console.error("Gagal logout:", error.message);
  }
});

// === Atur visibilitas tombol berdasarkan status login ===
onAuthStateChanged(auth, (user) => {
  if (user) {
    btnSignIn.classList.add("hidden");
    btnSignOut.classList.remove("hidden");
  } else {
    btnSignIn.classList.remove("hidden");
    btnSignOut.classList.add("hidden");
  }
});
