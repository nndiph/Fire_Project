<?php
session_start();

// Ambil data dari form login
$username = isset($_POST['username']) ? $_POST['username'] : '';
$password = isset($_POST['password']) ? $_POST['password'] : '';

// Catatan: Jika Anda sudah memiliki tabel pengguna di database `db_fire_system`, 
// Anda bisa men-uncomment baris di bawah ini dan mencocokkan datanya.
// include 'koneksi.php';

// --- MOCKUP LOGIN SEMENTARA ---
// Saat ini, selama username dan password diisi, sistem akan menganggap login berhasil
if (!empty($username) && !empty($password)) {
    
    // Set session untuk menandakan user sudah login (berguna untuk pengembangan nanti)
    $_SESSION['user_logged_in'] = true;
    $_SESSION['username'] = $username;
    
    // Arahkan ke halaman dasbor utama
    header("Location: index.html");
    exit();
} else {
    // Jika form kosong, kembalikan ke halaman login
    header("Location: login.html?error=1");
    exit();
}
?>
