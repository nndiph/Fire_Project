<?php
// Mulai sesi untuk mengenali sesi yang sedang berjalan
session_start();

// Hapus semua variabel sesi
session_unset();

// Hancurkan sesi
session_destroy();

// Redirect (arahkan kembali) ke halaman login
header("Location: login.html");
exit();
?>
