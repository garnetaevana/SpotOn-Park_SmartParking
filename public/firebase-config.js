// Import the functions you need from the SDKs you need
import { initializeApp } from "https://www.gstatic.com/firebasejs/10.12.0/firebase-app.js";
import { getFirestore } from "https://www.gstatic.com/firebasejs/10.12.0/firebase-firestore.js";
import { getAuth } from "https://www.gstatic.com/firebasejs/10.12.0/firebase-auth.js";
// TODO: Add SDKs for Firebase products that you want to use
// https://firebase.google.com/docs/web/setup#available-libraries

// Your web app's Firebase configuration
// For Firebase JS SDK v7.20.0 and later, measurementId is optional
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
const auth = getAuth(app);

// === EKSPOR OBJEK ===
export { app, db, auth };