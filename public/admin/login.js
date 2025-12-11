// login.js
import { auth } from "../firebase-config.js";
import { signInWithEmailAndPassword } from "https://www.gstatic.com/firebasejs/10.12.0/firebase-auth.js";

// ===== Form Login =====
document.getElementById("login-form").addEventListener("submit", async function (e) {
  e.preventDefault();

  const email = document.getElementById("email").value.trim();
  const password = document.getElementById("password").value.trim();
  const loadingOverlay = document.getElementById("loading-overlay");
  const form = document.getElementById("login-form");

  // Tampilkan overlay loading
  loadingOverlay.classList.remove("hidden");

  try {
    // === Login Firebase ===
    await signInWithEmailAndPassword(auth, email, password);

    // Login sukses → arahkan ke dashboard
    window.location.href = "./dashboard.html";
  } catch (error) {
    console.error("Login gagal:", error.message);

    // Sembunyikan loading
    loadingOverlay.classList.add("hidden");

    // Tambahkan animasi error
    form.classList.add("animate-pulse");
    setTimeout(() => form.classList.remove("animate-pulse"), 1000);

    // Tampilkan notifikasi error
    alert("Username atau password salah! Silakan coba lagi.");

    // Reset password input
    document.getElementById("password").value = "";
    document.getElementById("password").focus();
  }
});

// ===== Enter Key Support =====
document.addEventListener("keydown", function (e) {
  if (e.key === "Enter") {
    document.getElementById("login-form").dispatchEvent(new Event("submit"));
  }
});

// ===== Input Field Animations =====
const inputs = document.querySelectorAll('input[type="text"], input[type="password"]');
inputs.forEach((input) => {
  input.addEventListener("focus", function () {
    this.parentElement.classList.add("ring-2", "ring-blue-400", "rounded-xl");
  });
  input.addEventListener("blur", function () {
    this.parentElement.classList.remove("ring-2", "ring-blue-400");
  });
});

// ===== Checkbox Animation =====
const checkbox = document.querySelector('input[type="checkbox"]');
if (checkbox) {
  const checkIcon = document.querySelector('input[type="checkbox"] + div svg');
  checkbox.addEventListener("change", function () {
    if (this.checked) {
      checkIcon.classList.remove("opacity-0");
      checkIcon.parentElement.classList.add("bg-blue-500/20", "border-blue-400");
    } else {
      checkIcon.classList.add("opacity-0");
      checkIcon.parentElement.classList.remove("bg-blue-500/20", "border-blue-400");
    }
  });
}

// ===== Auto Focus Username Field =====
window.addEventListener("load", function () {
  setTimeout(() => {
    document.getElementById("email").focus();
  }, 500);
});

// ===== Floating Animation for Decorations =====
document.addEventListener("DOMContentLoaded", function () {
  const decorativeElements = document.querySelectorAll(
    ".absolute.bg-blue-500\\/10, .absolute.bg-purple-500\\/10"
  );
  decorativeElements.forEach((el, index) => {
    el.style.animationDelay = `${index * 0.5}s`;
  });
});
